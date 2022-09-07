#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include "../include/hnz.h"  //pour le moment
#include <plugin_api.h>      //pour le moment
#include <config_category.h> //pour le moment

#include <string>

using namespace std;

typedef void (*INGEST_CB)(void *, Reading);

extern "C"
{
    PLUGIN_HANDLE plugin_init(ConfigCategory *config);
    void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb,
                                void *data);
    Reading plugin_poll(PLUGIN_HANDLE *handle);
    void plugin_reconfigure(PLUGIN_HANDLE *handle, string &newConfig);
    void plugin_shutdown(PLUGIN_HANDLE *handle);
    void plugin_start(PLUGIN_HANDLE *handle);
};

/**
 * Default configuration
 */

const char *default_config2 = QUOTE({
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


void ingestCallback(void *data, Reading reading) {};

TEST(HNZ, PluginInit)
{
    ConfigCategory *config = new ConfigCategory("Test_Config", default_config2);
    config->setItemsValueFromDefault();
    ASSERT_NO_THROW(PLUGIN_HANDLE handle = plugin_init(config));
    
}


TEST(HNZ, PluginInitEmptyConfig) {
    ConfigCategory *emptyConfig = new ConfigCategory();
    ASSERT_NO_THROW(PLUGIN_HANDLE handle = plugin_init(emptyConfig));
}

TEST(HNZ, PluginRegisterIngest)
{

    ConfigCategory *emptyConfig = new ConfigCategory();
    PLUGIN_HANDLE handle = plugin_init(emptyConfig);
    ASSERT_NO_THROW(
        plugin_register_ingest((PLUGIN_HANDLE *)handle, ingestCallback, NULL));

}

TEST(HNZ, PluginRegisterIngestFailed) 
{
    PLUGIN_HANDLE handle = nullptr;
    ASSERT_THROW(
        plugin_register_ingest((PLUGIN_HANDLE *)handle, ingestCallback, NULL),
        exception);
}

TEST(HNZ, PluginPoll)
{
    ConfigCategory *emptyConfig = new ConfigCategory();
    PLUGIN_HANDLE handle = plugin_init(emptyConfig);
    ASSERT_THROW(plugin_poll((PLUGIN_HANDLE *)handle), runtime_error);
}

TEST(HNZ, PluginStop)
{
    ConfigCategory *emptyConfig = new ConfigCategory();
    PLUGIN_HANDLE handle = plugin_init(emptyConfig);
    ASSERT_NO_THROW(plugin_shutdown((PLUGIN_HANDLE *)handle));
}
