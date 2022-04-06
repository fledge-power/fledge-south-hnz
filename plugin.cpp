#include <hnz.h>
#include <plugin_api.h>
#include <string>
#include <logger.h>
#include <config_category.h>
#include <rapidjson/document.h>
#include <version.h>

typedef void (*INGEST_CB)(void *, Reading);

using namespace std;

#define PLUGIN_NAME "hnz"
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 6001

// PLUGIN DEFAULT PROTOCOL STACK CONF
#define PROTOCOL_STACK_DEF QUOTE({         \
    "protocol_stack" : {                   \
        "name" : "hnzclient",              \
        "version" : "1.0",                 \
        "transport_layer" : {              \
            "connection" : {               \
                "path" : {                 \
                    "srv_ip" : "10.0.0.5", \
                    "clt_ip" : "",         \
                    "port" : DEFAULT_PORT  \
                }                          \
            },                             \
            "llevel" : 1,                  \
            "retry_number" : 5,            \
            "retry_delay" : 5              \
        }                                  \
    }                                      \
})

// PLUGIN DEFAULT EXCHANGED DATA CONF
#define EXCHANGED_DATA_DEF QUOTE({          \
    "exchanged_data" : {                    \
        "name" : "hnzclient",               \
        "version" : "1.0",                  \
        "msg_list" : [                      \
            {                               \
                "station_address" : 1,      \
                "message_code" : "0B",      \
                "label" : "7AT761_AL.ECHAU",\
                "info_address" : 511,       \
                "internal_id" : "ID001836"  \
            },                              \
            {                               \
                "station_address" : 1,      \
                "message_code" : "0B",      \
                "label" : "7AT761_AL.TEMP", \
                "info_address" : 324,       \
                "internal_id" : "ID001026"  \
            }                               \
        ]                                   \
    }                                       \
})

// PLUGIN DEFAULT PROTOCOL TRANSLATION CONF
#define PROTOCOL_TRANSLATION_DEF QUOTE({         \
    "protocol_translation" : {                   \
        "name" : "hnz_to_pivot",                 \
        "version" : "1.0",                       \
        "mapping" : {                            \
                "data_object_header" : {         \
                    "doh_type" : "message_code", \
                    "doh_name" : "internal_id"   \
                },                               \
                "data_object_item" : {           \
                    "doi_value" : "value",       \
                    "doi_quality" : "quality",   \
                    "doi_ts" : "timestamp",      \
                    "doi_ts_qual" : "ts_qual"    \
                }                                \
            }                                    \
    }                                            \
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
        "default" : "hnz",
        "displayName" : "Asset Name",
        "order" : "1",
        "mandatory" : "true"
    },

    "protocol_stack" : {
        "description" : "protocol stack parameters",
        "type" : "string",
        "displayName" : "Protocol stack parameters",
        "order" : "2",
        "default" : PROTOCOL_STACK_DEF
    },

    "exchanged_data" :
    {
        "description" : "exchanged data list",
        "type" : "string",
        "displayName" : "Exchanged data list",
        "order" : "3",
        "default" : EXCHANGED_DATA_DEF
    },

    "protocol_translation" : {
        "description" : "protocol translation mapping",
        "type" : "string",
        "displayName" : "Protocol translation mapping",
        "order" : "4",
        "default" : PROTOCOL_TRANSLATION_DEF
    }
});

/**
 * The HNZ plugin interface
 */
extern "C"
{
    static PLUGIN_INFORMATION info = {
        PLUGIN_NAME,       // Name
        VERSION,           // Version
        SP_ASYNC,          // Flags
        PLUGIN_TYPE_SOUTH, // Type
        "1.0.0",           // Interface version
        default_config     // Default configuration
    };

    /**
     * Return the information about this plugin
     */
    PLUGIN_INFORMATION *plugin_info()
    {
        Logger::getLogger()->info("HNZ Config is %s", info.config);
        return &info;
    }

    PLUGIN_HANDLE plugin_init(ConfigCategory *config)
    {
        HNZ *hnz;
        Logger::getLogger()->info("Initializing the plugin");

        hnz = new HNZ(DEFAULT_IP, DEFAULT_PORT);

        if (config->itemExists("asset"))
        {
            hnz->setAssetName(config->getValue("asset"));
        }
        else
        {
            hnz->setAssetName("hnz");
        }

        if (config->itemExists("protocol_stack") && config->itemExists("exchanged_data") && config->itemExists("protocol_translation"))
        {
            hnz->setJsonConfig(config->getValue("protocol_stack"), config->getValue("exchanged_data"), config->getValue("protocol_translation"));
        }

        return (PLUGIN_HANDLE)hnz;
    }

    /**
     * Start the Async handling for the plugin
     */
    void plugin_start(PLUGIN_HANDLE *handle)
    {
        if (!handle)
            return;
        Logger::getLogger()->info("Starting the plugin");
        HNZ *hnz = (HNZ *)handle;
        hnz->start();
    }

    /**
     * Register ingest callback
     */
    void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data)
    {
        if (!handle)
            throw new exception();

        HNZ *hnz = (HNZ *)handle;
        hnz->registerIngest(data, cb);
    }

    /**
     * Poll for a plugin reading
     */
    Reading plugin_poll(PLUGIN_HANDLE *handle)
    {

        throw runtime_error("HNZ is an async plugin, poll should not be called");
    }

    /**
     * Reconfigure the plugin
     *
     */
    void plugin_reconfigure(PLUGIN_HANDLE *handle, string &newConfig)
    {
        ConfigCategory config("new", newConfig);
        auto *hnz = (HNZ *)*handle;

        std::unique_lock<std::mutex> guard2(hnz->loopLock);

        hnz->stop_loop();

        hnz->setPort(DEFAULT_PORT);
        hnz->setIp(DEFAULT_IP);

        if (config.itemExists("protocol_stack") && config.itemExists("exchanged_data") && config.itemExists("protocol_translation"))
            hnz->setJsonConfig(config.getValue("protocol_stack"), config.getValue("exchanged_data"), config.getValue("protocol_translation"));

        if (config.itemExists("asset"))
        {
            hnz->setAssetName(config.getValue("asset"));
            Logger::getLogger()->info("HNZ plugin restart after reconfigure asset");
        }

        hnz->start();
        guard2.unlock();
    }

    /**
     * Shutdown the plugin
     */
    void plugin_shutdown(PLUGIN_HANDLE *handle)
    {
        auto *hnz = (HNZ *)handle;

        hnz->stop();
        delete hnz;
    }
}
