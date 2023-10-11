#include <config_category.h>  //pour le moment
#include <gtest/gtest.h>
#include <plugin_api.h>  //pour le moment

#include <memory>
#include <string>
#include <utility>
#include <thread>

#include "hnz.h"

using namespace std;

typedef void (*INGEST_CB)(void *, Reading);

extern "C" {
PLUGIN_INFORMATION *plugin_info();
PLUGIN_HANDLE plugin_init(ConfigCategory *config);
void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data);
Reading plugin_poll(PLUGIN_HANDLE *handle);
void plugin_reconfigure(PLUGIN_HANDLE *handle, string &newConfig);
bool plugin_write(PLUGIN_HANDLE *handle, string &name, string &value);
bool plugin_operation(PLUGIN_HANDLE *handle, string &operation, int count,
                      PLUGIN_PARAMETER **params);
void plugin_shutdown(PLUGIN_HANDLE *handle);
void plugin_start(PLUGIN_HANDLE *handle);
};

#define PROTOCOL_STACK_DEF QUOTE({"protocol_stack" : {"name" : "test_ps"}})

#define EXCHANGED_DATA_DEF QUOTE({"exchanged_data" : {"name" : "test_ed"}})

static const char *default_config = QUOTE({
  "plugin" : {
    "description" : "hnz south plugin",
    "type" : "string",
    "default" : "TEST_PLUGIN",
    "readonly" : "true"
  },

  "protocol_stack" : {
    "description" : "protocol stack parameters",
    "type" : "JSON",
    "displayName" : "Protocol stack parameters",
    "order" : "2",
    "default" : PROTOCOL_STACK_DEF
  },

  "exchanged_data" : {
    "description" : "exchanged data list",
    "type" : "JSON",
    "displayName" : "Exchanged data list",
    "order" : "3",
    "default" : EXCHANGED_DATA_DEF
  }
});

void ingestCallback(void *data, Reading reading){};

TEST(HNZ, PluginInit) {
  ConfigCategory *config = new ConfigCategory("Test_Config", default_config);
  config->setItemsValueFromDefault();
  PLUGIN_HANDLE handle = nullptr;

  ASSERT_NO_THROW(handle = plugin_init(config));
  ASSERT_NE(handle, nullptr);

  if (handle != nullptr) ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete config;
}

TEST(HNZ, PluginInitNoConfig) {
  PLUGIN_HANDLE handle = nullptr;

  ASSERT_NO_THROW(handle = plugin_init(nullptr));
  ASSERT_NE(handle, nullptr);

  if (handle != nullptr) ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));
}

TEST(HNZ, PluginStart) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);
  ASSERT_NO_THROW(plugin_start(static_cast<PLUGIN_HANDLE *>(handle)));

  if (handle != nullptr) ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete emptyConfig;
}

TEST(HNZ, PluginInitEmptyConfig) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);

  if (handle != nullptr) ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete emptyConfig;
}

TEST(HNZ, PluginRegisterIngest) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);

  ASSERT_NO_THROW(plugin_register_ingest(static_cast<PLUGIN_HANDLE *>(handle), ingestCallback, nullptr));

  ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete emptyConfig;
}

TEST(HNZ, PluginRegisterIngestFailed) {
  PLUGIN_HANDLE handle = nullptr;

  ASSERT_THROW(plugin_register_ingest(static_cast<PLUGIN_HANDLE *>(handle), ingestCallback, nullptr), exception);
}

TEST(HNZ, PluginPoll) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);

  ASSERT_THROW(plugin_poll(static_cast<PLUGIN_HANDLE *>(handle)), runtime_error);

  ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete emptyConfig;
}

string new_test_conf = QUOTE({
  "plugin" : {
    "description" : "hnz south plugin",
    "type" : "string",
    "default" : "TEST_PLUGIN",
    "readonly" : "true"
  },

  "protocol_stack" : {
    "description" : "protocol stack parameters",
    "type" : "JSON",
    "displayName" : "Protocol stack parameters",
    "order" : "2",
    "default" : PROTOCOL_STACK_DEF
  },

  "exchanged_data" : {
    "description" : "exchanged data list",
    "type" : "JSON",
    "displayName" : "Exchanged data list",
    "order" : "3",
    "default" : EXCHANGED_DATA_DEF
  }
});

TEST(HNZ, PluginReconfigure) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);
  ASSERT_NO_THROW(plugin_reconfigure(static_cast<PLUGIN_HANDLE *>(handle), new_test_conf));

  ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  handle = nullptr;
  ASSERT_THROW(plugin_reconfigure(static_cast<PLUGIN_HANDLE *>(handle), new_test_conf), exception);

  delete emptyConfig;
}

TEST(HNZ, PluginWrite) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);

  string name("name");
  string value("value");
  ASSERT_FALSE(plugin_write(static_cast<PLUGIN_HANDLE *>(handle), name, value));

  ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete emptyConfig;
}

TEST(HNZ, PluginOperation) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);

  string operation("operation_test");
  bool res = false;
  ASSERT_NO_THROW(res = plugin_operation(static_cast<PLUGIN_HANDLE *>(handle), operation, 10, nullptr));
  ASSERT_FALSE(res);

  ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  handle = nullptr;
  ASSERT_THROW(plugin_operation(static_cast<PLUGIN_HANDLE *>(handle), operation, 10, nullptr), exception);

  delete emptyConfig;
}

TEST(HNZ, PluginStop) {
  ConfigCategory *emptyConfig = new ConfigCategory();
  PLUGIN_HANDLE handle = nullptr;
  ASSERT_NO_THROW(handle = plugin_init(emptyConfig));
  ASSERT_NE(handle, nullptr);

  ASSERT_NO_THROW(plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle)));

  delete emptyConfig;
}

TEST(HNZ, PluginStopDuringConnect) {
  PLUGIN_INFORMATION *info = nullptr;
  ASSERT_NO_THROW(info = plugin_info());
  ConfigCategory *config = new ConfigCategory("default_config", info->config);
  config->setItemsValueFromDefault();
  PLUGIN_HANDLE handle = nullptr;

  ASSERT_NO_THROW(handle = plugin_init(config));
  ASSERT_NE(handle, nullptr);

  ASSERT_NO_THROW(plugin_start(static_cast<PLUGIN_HANDLE *>(handle)));

  // Wait for plugin to enter HNZPath::connect() on both path
  printf("Waiting for HNZPath::connect()...\n"); fflush(stdout);
  this_thread::sleep_for(chrono::seconds(10));

  // Make sure that shutdown does not hang forever on "hnz - HNZ::stop - Waiting for the receiving thread"
  printf("Waiting for plugin_shutdown() to complete...\n"); fflush(stdout);
  std::atomic<bool> shutdownSuccess{false};
  std::thread joinThread([&handle, &shutdownSuccess](){
    plugin_shutdown(static_cast<PLUGIN_HANDLE *>(handle));
    shutdownSuccess = true;
  });
  joinThread.detach();
  int timeMs = 0;
  // Wait up to 60s for plugin_shutdown to complete
  while (timeMs < 60000) {
    if (shutdownSuccess) {
      break;
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    timeMs += 100;
  }
  ASSERT_TRUE(shutdownSuccess) << "Shutdown did not complete in time, it's probably hanging on a thread join()";

  delete config;
}