#include <gtest/gtest.h>
#include <plugin_api.h>
#include <rapidjson/document.h>
#include <string.h>

#include <string>

#include "hnz.h"
#include "hnzconf.h"
#include "hnzconnection.h"

using namespace std;

string protocol_stack_def_one_path = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" : {"connections" : [ {"srv_ip" : "192.168.0.10"} ]},
    "application_layer" : {"remote_station_addr" : 12}
  }
});

string protocol_stack_def_two_path = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" : {
      "connections" : [
        {"srv_ip" : "192.168.0.10"}, {"srv_ip" : "192.168.0.12", "port" : 6002}
      ]
    },
    "application_layer" : {"remote_station_addr" : 12}
  }
});

static string exchanged_data_def = QUOTE({
  "exchanged_data" : {
    "name" : "SAMPLE",
    "version" : "1.0",
    "datapoints" : [ {
      "label" : "TS1",
      "pivot_id" : "ID114562",
      "pivot_type" : "SpsTyp",
      "protocols" : [
        {"name" : "iec104", "address" : "45-672", "typeid" : "M_SP_TB_1"}, {
          "name" : "tase2",
          "address" : "S_114562",
          "typeid" : "Data_StateQTimeTagExtended"
        },
        {
          "name" : "hnzip",
          "address" : "511",
          "typeid" : "TSCE"
        }
      ]
    } ]
  }
});

TEST(HNZConnection, OnlyOnePathConfigured) {
  HNZConf* conf = new HNZConf();
  conf->importConfigJson(protocol_stack_def_one_path);
  conf->importExchangedDataJson(exchanged_data_def);

  HNZ* hnz = new HNZ();
  HNZConnection* hnz_connection = new HNZConnection(conf, hnz);

  ASSERT_NE(nullptr, hnz_connection->getActivePath());
  ASSERT_EQ(nullptr, hnz_connection->getPassivePath());

  delete hnz_connection;
  delete conf;
  delete hnz;
}

TEST(HNZConnection, TwoPathConfigured) {
  HNZConf* conf = new HNZConf();
  conf->importConfigJson(protocol_stack_def_two_path);
  conf->importExchangedDataJson(exchanged_data_def);

  HNZ* hnz = new HNZ();
  HNZConnection* hnz_connection = new HNZConnection(conf, hnz);

  ASSERT_NE(nullptr, hnz_connection->getActivePath());
  ASSERT_NE(nullptr, hnz_connection->getPassivePath());

  delete hnz_connection;
  delete conf;
  delete hnz;
}