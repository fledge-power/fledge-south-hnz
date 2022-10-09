#include <gtest/gtest.h>
#include <plugin_api.h>
#include <rapidjson/document.h>
#include <string.h>

#include <string>

#include "hnzconf.h"

using namespace std;
using namespace rapidjson;

string protocol_stack_def = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" : {
      "connections" : [
        {"srv_ip" : "192.168.0.10", "port" : 6001},
        {"srv_ip" : "192.168.0.10", "port" : 6002}
      ]
    },
    "application_layer" : {
      "remote_station_addr" : 12,
      "inacc_timeout" : 180,
      "max_sarm" : 30,
      "repeat_path_A" : 3,
      "repeat_path_B" : 3,
      "repeat_timeout" : 3000,
      "anticipation" : 3,
      "test_msg_send" : "1304",
      "test_msg_receive" : "1304",
      "gi_schedule" : "99:99",
      "gi_repeat_count" : 3,
      "gi_time" : 255,
      "c_ack_time" : 10
    }
  }
});

string exchanged_data_def = QUOTE({
  "exchanged_data" : {
    "name" : "SAMPLE",
    "version" : "1.0",
    "datapoints" : [
      {
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
            "name" : "hnz",
            "station_address" : 1,
            "message_address" : 511,
            "message_code" : "TSCE"
          }
        ]
      },
      {
        "label" : "TM1",
        "pivot_id" : "ID99876",
        "pivot_type" : "DpsTyp",
        "protocols" : [
          {"name" : "iec104", "address" : "45-984", "typeid" : "M_ME_NA_1"},
          {"name" : "tase2", "address" : "S_114562", "typeid" : "Data_RealQ"}, {
            "name" : "hnz",
            "station_address" : 20,
            "message_address" : 511,
            "message_code" : "TMN"
          }
        ]
      }
    ]
  }
});

class HNZConfTest : public testing::Test {
 protected:
  void SetUp() {
    hnz_conf = new HNZConf();
    hnz_conf->importConfigJson(protocol_stack_def);
    hnz_conf->importExchangedDataJson(exchanged_data_def);
  }

  void TearDown() { delete hnz_conf; }

  static HNZConf* hnz_conf;
};

HNZConf* HNZConfTest::hnz_conf;

TEST(HNZCONF, EmptyConf) {
  HNZConf* conf = new HNZConf();

  ASSERT_FALSE(conf->is_complete());

  delete conf;
}

// TEST(HNZCONF,ConfWithConfigJsonButNotComplete)
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     EXPECT_FALSE(conf->is_complete());
// }

// TEST(HNZCONF,ConfWithExchangedDataButNotComplete)
// {
//     HNZConf* conf = new HNZConf();
//     conf->importExchangedDataJson();
//     EXPECT_FALSE(conf->is_complete());

// }

TEST_F(HNZConfTest, ConfComplete) { EXPECT_TRUE(hnz_conf->is_complete()); }

TEST_F(HNZConfTest, GetIPAdress) {
  ASSERT_STREQ(hnz_conf->get_ip_address().c_str(), "192.168.0.10");
}

TEST_F(HNZConfTest, GetPort) { ASSERT_EQ(hnz_conf->get_port(), 6001); }

TEST_F(HNZConfTest, GetRemoteStationAddr) {
  ASSERT_EQ(hnz_conf->get_remote_station_addr(), 12);
}

TEST_F(HNZConfTest, GetInaccTimeout) {
  ASSERT_EQ(hnz_conf->get_inacc_timeout(), 180);
}

TEST_F(HNZConfTest, GetMaxSARM) { ASSERT_EQ(hnz_conf->get_max_sarm(), 30); }

TEST_F(HNZConfTest, GetRepeatPathAB) {
  ASSERT_EQ(hnz_conf->get_repeat_path_A(), 3);
  ASSERT_EQ(hnz_conf->get_repeat_path_B(), 3);
}

TEST_F(HNZConfTest, GetRepeatTimeout) {
  ASSERT_EQ(hnz_conf->get_repeat_timeout(), 3000);
}

TEST_F(HNZConfTest, GetAnticipationRatio) {
  ASSERT_EQ(hnz_conf->get_anticipation_ratio(), 3);
}

TEST_F(HNZConfTest, GetTestMsgSend) {
  ASSERT_EQ(hnz_conf->get_test_msg_send().first, 0x13);
  ASSERT_EQ(hnz_conf->get_test_msg_send().second, 0x04);
}

TEST_F(HNZConfTest, GetTestMsgReceive) {
  ASSERT_EQ(hnz_conf->get_test_msg_receive().first, 0x13);
  ASSERT_EQ(hnz_conf->get_test_msg_receive().second, 0x04);
}

TEST_F(HNZConfTest, GetGISchedule) {
  // TODO : Add other case
  ASSERT_FALSE(hnz_conf->get_gi_schedule().activate);
}

TEST_F(HNZConfTest, GetGIRepeatCount) {
  ASSERT_EQ(hnz_conf->get_gi_repeat_count(), 3);
}

TEST_F(HNZConfTest, GetGITime) { ASSERT_EQ(hnz_conf->get_gi_time(), 255); }

TEST_F(HNZConfTest, GetCAckTime) { ASSERT_EQ(hnz_conf->get_c_ack_time(), 10); }