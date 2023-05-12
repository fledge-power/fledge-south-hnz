#include <gtest/gtest.h>
#include <plugin_api.h>

#include "hnzconf.h"

using namespace std;

string protocol_stack_def = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" : {
      "connections" : [
        {"srv_ip" : "192.168.0.10", "port" : 6001},
        {"srv_ip" : "192.168.0.12", "port" : 6002}
      ]
    },
    "application_layer" : {
      "remote_station_addr" : 12,
      "inacc_timeout" : 200,
      "max_sarm" : 40,
      "repeat_path_A" : 5,
      "repeat_path_B" : 2,
      "repeat_timeout" : 2000,
      "anticipation_ratio" : 5,
      "test_msg_send" : "1305",
      "test_msg_receive" : "1306",
      "gi_schedule" : "18:05",
      "gi_repeat_count" : 5,
      "gi_time" : 300,
      "c_ack_time" : 20,
      "cmd_recv_timeout" : 200000
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
            "station_address" : 12,
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

TEST_F(HNZConfTest, ConfComplete) { EXPECT_TRUE(hnz_conf->is_complete()); }

TEST_F(HNZConfTest, GetIPAdress) {
  ASSERT_STREQ(hnz_conf->get_ip_address_A().c_str(), "192.168.0.10");
  ASSERT_STREQ(hnz_conf->get_ip_address_B().c_str(), "192.168.0.12");
}

TEST_F(HNZConfTest, GetPort) {
  ASSERT_EQ(hnz_conf->get_port_A(), 6001);
  ASSERT_EQ(hnz_conf->get_port_B(), 6002);
}

TEST_F(HNZConfTest, GetRemoteStationAddr) {
  ASSERT_EQ(hnz_conf->get_remote_station_addr(), 12);
}

TEST_F(HNZConfTest, GetInaccTimeout) {
  ASSERT_EQ(hnz_conf->get_inacc_timeout(), 200);
}

TEST_F(HNZConfTest, GetMaxSARM) { ASSERT_EQ(hnz_conf->get_max_sarm(), 40); }

TEST_F(HNZConfTest, GetRepeatPathAB) {
  ASSERT_EQ(hnz_conf->get_repeat_path_A(), 5);
  ASSERT_EQ(hnz_conf->get_repeat_path_B(), 2);
}

TEST_F(HNZConfTest, GetRepeatTimeout) {
  ASSERT_EQ(hnz_conf->get_repeat_timeout(), 2000);
}

TEST_F(HNZConfTest, GetAnticipationRatio) {
  ASSERT_EQ(hnz_conf->get_anticipation_ratio(), 5);
}

TEST_F(HNZConfTest, GetTestMsgSend) {
  ASSERT_EQ(hnz_conf->get_test_msg_send().first, 0x13);
  ASSERT_EQ(hnz_conf->get_test_msg_send().second, 0x05);
}

TEST_F(HNZConfTest, GetTestMsgReceive) {
  ASSERT_EQ(hnz_conf->get_test_msg_receive().first, 0x13);
  ASSERT_EQ(hnz_conf->get_test_msg_receive().second, 0x06);
}

TEST_F(HNZConfTest, GetGISchedule) {
  ASSERT_TRUE(hnz_conf->get_gi_schedule().activate);
  ASSERT_EQ(hnz_conf->get_gi_schedule().hour, 18);
  ASSERT_EQ(hnz_conf->get_gi_schedule().min, 05);
}

TEST_F(HNZConfTest, GetGIRepeatCount) {
  ASSERT_EQ(hnz_conf->get_gi_repeat_count(), 5);
}

TEST_F(HNZConfTest, GetGITime) { ASSERT_EQ(hnz_conf->get_gi_time(), 300); }

TEST_F(HNZConfTest, GetCAckTime) { ASSERT_EQ(hnz_conf->get_c_ack_time(), 20); }

TEST_F(HNZConfTest, GetCmdRecvTimeout) { ASSERT_EQ(hnz_conf->get_cmd_recv_timeout(), 200000); }

TEST_F(HNZConfTest, GetLabelTS1) {
  ASSERT_STREQ(hnz_conf->getLabel("TSCE", 511).c_str(), "TS1");
}

TEST_F(HNZConfTest, GetLabelUnknown) {
  ASSERT_STREQ(hnz_conf->getLabel("TSCE", 999).c_str(), "");
}

string min_protocol_stack_def = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" : {"connections" : [ {"srv_ip" : "0.0.0.0"} ]},
    "application_layer" : {"remote_station_addr" : 18}
  }
});

TEST(HNZCONF, MinimumConf) {
  HNZConf* hnz_conf = new HNZConf();
  hnz_conf->importConfigJson(min_protocol_stack_def);
  hnz_conf->importExchangedDataJson(exchanged_data_def);

  ASSERT_TRUE(hnz_conf->is_complete());

  ASSERT_STREQ(hnz_conf->get_ip_address_A().c_str(), "0.0.0.0");

  ASSERT_STREQ(hnz_conf->get_ip_address_B().c_str(), "");

  ASSERT_EQ(hnz_conf->get_port_A(), 6001);

  ASSERT_EQ(hnz_conf->get_remote_station_addr(), 18);

  ASSERT_EQ(hnz_conf->get_inacc_timeout(), 180);

  ASSERT_EQ(hnz_conf->get_max_sarm(), 30);

  ASSERT_EQ(hnz_conf->get_repeat_path_A(), 3);
  ASSERT_EQ(hnz_conf->get_repeat_path_B(), 3);

  ASSERT_EQ(hnz_conf->get_repeat_timeout(), 3000);

  ASSERT_EQ(hnz_conf->get_anticipation_ratio(), 3);

  ASSERT_EQ(hnz_conf->get_test_msg_send().first, 0x13);
  ASSERT_EQ(hnz_conf->get_test_msg_send().second, 0x04);

  ASSERT_EQ(hnz_conf->get_test_msg_receive().first, 0x13);
  ASSERT_EQ(hnz_conf->get_test_msg_receive().second, 0x04);

  ASSERT_FALSE(hnz_conf->get_gi_schedule().activate);

  ASSERT_EQ(hnz_conf->get_gi_repeat_count(), 3);

  ASSERT_EQ(hnz_conf->get_gi_time(), 255);

  ASSERT_EQ(hnz_conf->get_c_ack_time(), 10);

  delete hnz_conf;
}

string wrong_protocol_stack_def = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" : {"connections" : [ {"srv_ip" : "0.0.0.0"} ]}
  }
});

TEST(HNZCONF, ConfNotComplete) {
  HNZConf* hnz_conf = new HNZConf();
  hnz_conf->importConfigJson(wrong_protocol_stack_def);
  hnz_conf->importExchangedDataJson(exchanged_data_def);

  ASSERT_FALSE(hnz_conf->is_complete());
  delete hnz_conf;
}