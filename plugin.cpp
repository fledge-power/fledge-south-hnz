/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Lucas Barret, Colin Constans, Justin Facquet
 */

#include <config_category.h>
#include <hnz.h>
#include <logger.h>
#include <plugin_api.h>
#include <rapidjson/document.h>
#include <version.h>

#include <string>

typedef void (*INGEST_CB)(void *, Reading);

using namespace std;

#define PLUGIN_NAME "hnz"

// PLUGIN DEFAULT PROTOCOL STACK CONF
#define PROTOCOL_STACK_DEF                            \
  QUOTE({                                             \
    "protocol_stack" : {                              \
      "name" : "hnzclient",                           \
      "version" : "1.0",                              \
      "transport_layer" : {                           \
        "connections" : [                             \
          {"srv_ip" : "192.168.0.10", "port" : 6001}, \
          {"srv_ip" : "192.168.0.11", "port" : 6002}  \
        ]                                             \
      },                                              \
      "application_layer" : {                         \
        "remote_station_addr" : 12,                   \
        "inacc_timeout" : 180,                        \
        "max_sarm" : 30,                              \
        "repeat_path_A" : 3,                          \
        "repeat_path_B" : 3,                          \
        "repeat_timeout" : 3000,                      \
        "anticipation" : 3,                           \
        "test_msg_send" : "1304",                     \
        "test_msg_receive" : "1304",                  \
        "gi_schedule" : "99:99",                      \
        "gi_repeat_count" : 3,                        \
        "gi_time" : 255,                              \
        "c_ack_time" : 10                             \
      }                                               \
    }                                                 \
  })

// PLUGIN DEFAULT EXCHANGED DATA CONF
#define EXCHANGED_DATA_DEF                                                     \
  QUOTE({                                                                      \
    "exchanged_data" : {                                                       \
      "name" : "SAMPLE",                                                       \
      "version" : "1.0",                                                       \
      "datapoints" : [                                                         \
        {                                                                      \
          "label" : "TS1",                                                     \
          "pivot_id" : "ID114562",                                             \
          "pivot_type" : "SpsTyp",                                             \
          "protocols" : [                                                      \
            {"name" : "iec104", "address" : "45-672", "typeid" : "M_SP_TB_1"}, \
            {                                                                  \
              "name" : "tase2",                                                \
              "address" : "S_114562",                                          \
              "typeid" : "Data_StateQTimeTagExtended"                          \
            },                                                                 \
            {                                                                  \
              "name" : "hnz",                                                  \
              "station_address" : 1,                                           \
              "message_address" : 511,                                         \
              "message_code" : "TSCE"                                          \
            }                                                                  \
          ]                                                                    \
        },                                                                     \
        {                                                                      \
          "label" : "TM1",                                                     \
          "pivot_id" : "ID99876",                                              \
          "pivot_type" : "DpsTyp",                                             \
          "protocols" : [                                                      \
            {"name" : "iec104", "address" : "45-984", "typeid" : "M_ME_NA_1"}, \
            {                                                                  \
              "name" : "tase2",                                                \
              "address" : "S_114562",                                          \
              "typeid" : "Data_RealQ"                                          \
            },                                                                 \
            {                                                                  \
              "name" : "hnz",                                                  \
              "station_address" : 20,                                          \
              "message_address" : 511,                                         \
              "message_code" : "TMN"                                           \
            }                                                                  \
          ]                                                                    \
        }                                                                      \
      ]                                                                        \
    }                                                                          \
  })

/**
 * Default configuration
 */

const char *default_config = QUOTE({
  "plugin" : {
    "description" : "hnz south plugin",
    "type" : "string",
    "default" : PLUGIN_NAME,
    "readonly" : "true"
  },

  "asset" : {
    "description" : "Asset name",
    "type" : "string",
    "default" : PLUGIN_NAME,
    "displayName" : "Asset Name",
    "order" : "1",
    "mandatory" : "true"
  },

  "protocol_stack" : {
    "description" : "protocol stack parameters",
    "type" : "string",
    "displayName" : "Protocol stack parameters",
    "order" : "2",
    "default" : PROTOCOL_STACK_DEF,
    "mandatory" : "true"
  },

  "exchanged_data" : {
    "description" : "exchanged data list",
    "type" : "string",
    "displayName" : "Exchanged data list",
    "order" : "3",
    "default" : EXCHANGED_DATA_DEF,
    "mandatory" : "true"
  }
});

/**
 * The HNZ plugin interface
 */
extern "C" {
static PLUGIN_INFORMATION info = {
    PLUGIN_NAME,        // Name
    VERSION,            // Version
    SP_ASYNC,           // Flags
    PLUGIN_TYPE_SOUTH,  // Type
    "1.0.0",            // Interface version
    default_config      // Default configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info() {
  Logger::getLogger()->info("HNZ Config is %s", info.config);
  return &info;
}

PLUGIN_HANDLE plugin_init(ConfigCategory *config) {
  HNZ *hnz;
  Logger::getLogger()->info("Initializing the plugin");

  hnz = new HNZ();

  if (config->itemExists("asset")) {
    hnz->setAssetName(config->getValue("asset"));
  } else {
    hnz->setAssetName(PLUGIN_NAME);
  }

  if (config->itemExists("protocol_stack") &&
      config->itemExists("exchanged_data")) {
    if (!hnz->setJsonConfig(config->getValue("protocol_stack"),
                            config->getValue("exchanged_data")))
      return nullptr;
  }

  Logger::getLogger()->info("Pluging initialized");

  return (PLUGIN_HANDLE)hnz;
}

/**
 * Start the Async handling for the plugin
 */
void plugin_start(PLUGIN_HANDLE *handle) {
  if (!handle) return;
  Logger::getLogger()->info("Starting the plugin");
  HNZ *hnz = (HNZ *)handle;
  hnz->start();
  Logger::getLogger()->info("Plugin started");
}

/**
 * Register ingest callback
 */
void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data) {
  if (!handle) throw new exception();

  HNZ *hnz = (HNZ *)handle;
  hnz->registerIngest(data, cb);
}

/**
 * Poll for a plugin reading
 */
Reading plugin_poll(PLUGIN_HANDLE *handle) {
  throw runtime_error("HNZ is an async plugin, poll should not be called");
}

/**
 * Reconfigure the plugin
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, string &newConfig) {
  ConfigCategory config("new", newConfig);
  auto *hnz = (HNZ *)*handle;

  if (config.itemExists("protocol_stack") &&
      config.itemExists("exchanged_data"))
    hnz->setJsonConfig(config.getValue("protocol_stack"),
                       config.getValue("exchanged_data"));

  if (config.itemExists("asset")) {
    hnz->setAssetName(config.getValue("asset"));
  }
}

/**
 * Shutdown the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle) {
  auto *hnz = (HNZ *)handle;
  delete hnz;
}
}
