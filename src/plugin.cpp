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
#include <plugin_api.h>
#include <rapidjson/document.h>
#include <version.h>

#include <string>

#include "hnzutility.h"

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
          {"srv_ip" : "192.168.0.10", "port" : 6002}  \
        ]                                             \
      },                                              \
      "application_layer" : {                         \
        "remote_station_addr" : 12,                   \
        "inacc_timeout" : 180,                        \
        "max_sarm" : 30,                              \
        "repeat_path_A" : 3,                          \
        "repeat_path_B" : 3,                          \
        "repeat_timeout" : 3000,                      \
        "anticipation_ratio" : 3,                     \
        "test_msg_send" : "1304",                     \
        "test_msg_receive" : "1304",                  \
        "gi_schedule" : "99:99",                      \
        "gi_repeat_count" : 3,                        \
        "gi_time" : 255,                              \
        "c_ack_time" : 10                             \
      },                                              \
      "south_monitoring" : {                          \
        "asset" : "CONNECTION-1"                      \
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
              "name" : "hnzip",                                                \
              "address" : "511",                                               \
              "typeid" : "TS"                                                  \
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
              "name" : "hnzip",                                                \
              "address" : "511",                                               \
              "typeid" : "TM"                                                  \
            }                                                                  \
          ]                                                                    \
        }                                                                      \
      ]                                                                        \
    }                                                                          \
  })

/**
 * Default configuration
 */

static const char *default_config = QUOTE({
  "plugin" : {
    "description" : "hnz south plugin",
    "type" : "string",
    "default" : PLUGIN_NAME,
    "readonly" : "true"
  },

  "enable": {
      "description": "A switch that can be used to enable or disable execution of the filter.",
      "displayName": "Enabled",
      "type": "boolean",
      "default": "true"
  },

  "protocol_stack" : {
    "description" : "protocol stack parameters",
    "type" : "JSON",
    "displayName" : "Protocol stack parameters",
    "order" : "2",
    "default" : PROTOCOL_STACK_DEF,
    "mandatory" : "true"
  },

  "exchanged_data" : {
    "description" : "exchanged data list",
    "type" : "JSON",
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
    PLUGIN_NAME,           // Name
    VERSION,               // Version
    SP_ASYNC | SP_CONTROL, // Flags
    PLUGIN_TYPE_SOUTH,     // Type
    "1.0.0",               // Interface version
    default_config         // Default configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info() {
  HnzUtility::log_info("HNZ Config is %s", info.config);
  return &info;
}

PLUGIN_HANDLE plugin_init(ConfigCategory* config) {

  std::string beforeLog = std::string(PLUGIN_NAME) + " -";
  HnzUtility::log_info("%s Initializing the plugin", beforeLog.c_str());

  if (config == nullptr) {
      HnzUtility::log_warn("%s No config provided for filter, using default config", beforeLog.c_str());
      auto info = plugin_info();
      config = new ConfigCategory("newConfig", info->config);
      config->setItemsValueFromDefault();
  }

  auto hnz = new HNZ();
  hnz->reconfigure(*config);

  HnzUtility::log_info("%s Pluging initialized", beforeLog.c_str());

  return static_cast<PLUGIN_HANDLE>(hnz);
}

/**
 * Start the Async handling for the plugin
 */
void plugin_start(PLUGIN_HANDLE *handle) {
  if (!handle) return;
  
  std::string beforeLog = std::string(PLUGIN_NAME) + " -";
  HnzUtility::log_info("%s Starting the plugin...", beforeLog.c_str());
  HNZ *hnz = reinterpret_cast<HNZ *>(handle);
  hnz->start();
  HnzUtility::log_info("%s Plugin started", beforeLog.c_str());
}

/**
 * Register ingest callback
 */
void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data) {
  if (!handle) throw exception();

  HNZ *hnz = reinterpret_cast<HNZ *>(handle);
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
  if (!handle) throw exception();

  auto *hnz = reinterpret_cast<HNZ *>(handle);
  ConfigCategory config("newConfig", newConfig);
  hnz->reconfigure(config);
}

/**
 * Shutdown the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle) {
  auto *hnz = reinterpret_cast<HNZ *>(handle);
  delete hnz;
}

/**
 * Plugin_write entry point
 * NOT USED
 */
bool plugin_write(PLUGIN_HANDLE *handle, string &name, string &value) {
  return false;
}

/**
 * Plugin_operation entry point
 */
bool plugin_operation(PLUGIN_HANDLE *handle, string &operation, int count,
                      PLUGIN_PARAMETER **params) {
  if (!handle) throw exception();

  auto *hnz = reinterpret_cast<HNZ *>(handle);

  return hnz->operation(operation, count, params);
}
}
