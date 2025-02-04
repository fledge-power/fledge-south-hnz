#include <gtest/gtest.h>
#include <plugin_api.h>

#include "hnzconf.h"

using namespace std;

string protocol_stack_def = QUOTE({
  "protocol_stack": {
    "name": "hnzclient",
    "version": "1.0",
    "transport_layer": {
      "connections": [
        {"srv_ip": "192.168.0.10", "port": 6001},
        {"srv_ip": "192.168.0.12", "port": 6002}
      ]
    },
    "application_layer": {
      "remote_station_addr": 12,
      "inacc_timeout": 200,
      "max_sarm": 40,
      "repeat_path_A": 5,
      "repeat_path_B": 2,
      "repeat_timeout": 2000,
      "anticipation_ratio": 5,
      "test_msg_send": "1305",
      "test_msg_receive": "1306",
      "gi_schedule": "18:05",
      "gi_repeat_count": 5,
      "gi_time": 300,
      "c_ack_time": 20,
      "cmd_recv_timeout": 200000
    },
    "south_monitoring": {
      "asset": "TEST_ASSET"
    }
  }
});

string exchanged_data_def = QUOTE({
  "exchanged_data": {
    "name": "SAMPLE",
    "version": "1.0",
    "datapoints": [
      {
        "label": "TS1",
        "pivot_id": "ID114562",
        "pivot_type": "SpsTyp",
        "protocols": [
          {
            "name": "iec104",
            "address": "45-672",
            "typeid": "M_SP_TB_1"
          },
          {
            "name": "tase2",
            "address": "S_114562",
            "typeid": "Data_StateQTimeTagExtended"
          },
          {
            "name": "hnzip",
            "address": "511",
            "typeid": "TS"
          }
        ]
      },
      {
        "label": "TS2",
        "pivot_id": "S_2367_0_2_32",
        "pivot_type": "SpsTyp",
        "protocols": [
          {
            "name": "iec104",
            "address": "45-673",
            "typeid": "M_SP_TB_1"
          },
          {
            "name": "tase2",
            "address": "S_114563",
            "typeid": "Data_StateQTimeTagExtended"
          },
          {
            "name": "hnzip",
            "address": "512",
            "typeid": "TS"
          }
        ],
        "pivot_subtypes": [
          "trigger_south_gi"
        ]
      },
      {
        "label": "TM1",
        "pivot_id": "ID99876",
        "pivot_type": "DpsTyp",
        "protocols": [
          {"name": "iec104", "address": "45-984", "typeid": "M_ME_NA_1"},
          {"name": "tase2", "address": "S_114562", "typeid": "Data_RealQ"}, {
            "name": "hnzip",
            "address": "511",
            "typeid": "TM"
          }
        ]
      }
    ]
  }
});

class HNZConfTest : public testing::Test {
 protected:
  void SetUp() {
    hnz_conf = std::make_shared<HNZConf>();
    hnz_conf->importConfigJson(protocol_stack_def);
    hnz_conf->importExchangedDataJson(exchanged_data_def);
  }

  void TearDown() {}

  std::shared_ptr<HNZConf> hnz_conf;
};

TEST(HNZCONF, EmptyConf) {
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();

  ASSERT_FALSE(conf->is_complete());
}

TEST(HNZCONF, ConstructorWithParam) {
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>(protocol_stack_def, exchanged_data_def);

  ASSERT_TRUE(conf->is_complete());
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
  ASSERT_STREQ(hnz_conf->getLabel("TS", 511).c_str(), "TS1");
}

TEST_F(HNZConfTest, GetLabelUnknown) {
  ASSERT_STREQ(hnz_conf->getLabel("TS", 999).c_str(), "");
}

TEST_F(HNZConfTest, GetConnxStatusSignal) { ASSERT_STREQ(hnz_conf->get_connx_status_signal().c_str(), "TEST_ASSET"); }

TEST_F(HNZConfTest, GetLastTSAddress) { ASSERT_EQ(hnz_conf->getLastTSAddress(), 512); }

TEST_F(HNZConfTest, GetAllMessages) {
  unsigned int remote_station_addr = 12;
  unsigned int msg_address = 511;
  const auto& allMessages = hnz_conf->get_all_messages();
  ASSERT_EQ(allMessages.size(), 2);
  ASSERT_EQ(allMessages.count("TS"), 1);
  ASSERT_EQ(allMessages.count("TM"), 1);

  const auto& allTSMessages = allMessages.at("TS");
  ASSERT_EQ(allTSMessages.size(), 1);
  ASSERT_EQ(allTSMessages.count(remote_station_addr), 1);
  const auto& allTMMessages = allMessages.at("TM");
  ASSERT_EQ(allTMMessages.size(), 1);
  ASSERT_EQ(allTMMessages.count(remote_station_addr), 1);

  const auto& allTSForRemoteAddr = allTSMessages.at(remote_station_addr);
  ASSERT_EQ(allTSForRemoteAddr.size(), 2);
  ASSERT_EQ(allTSForRemoteAddr.count(msg_address), 1);
  ASSERT_STREQ(allTSForRemoteAddr.at(msg_address).c_str(), "TS1");
  const auto& allTMForRemoteAddr = allTMMessages.at(remote_station_addr);
  ASSERT_EQ(allTMForRemoteAddr.size(), 1);
  ASSERT_EQ(allTMForRemoteAddr.count(msg_address), 1);
  ASSERT_STREQ(allTMForRemoteAddr.at(msg_address).c_str(), "TM1");
}

string min_protocol_stack_def = QUOTE({
  "protocol_stack": {
    "name": "hnzclient",
    "version": "1.0",
    "transport_layer": {"connections": [ {"srv_ip": "0.0.0.0"} ]},
    "application_layer": {"remote_station_addr": 18}
  }
});

TEST(HNZCONF, MinimumConf) {
  std::shared_ptr<HNZConf> hnz_conf = std::make_shared<HNZConf>();
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

  ASSERT_STREQ(hnz_conf->get_connx_status_signal().c_str(), "");

  ASSERT_TRUE(hnz_conf->isTsAddressCgTriggering(512));

  ASSERT_FALSE(hnz_conf->isTsAddressCgTriggering(511));
}

TEST(HNZCONF, ConfNotComplete) {
  std::string wrong_protocol_stack_def = QUOTE({
    "protocol_stack": {
      "name": "hnzclient",
      "version": "1.0",
      "transport_layer": {"connections": [ {"srv_ip": "0.0.0.0"} ]}
    }
  });

  std::shared_ptr<HNZConf> hnz_conf = std::make_shared<HNZConf>();
  hnz_conf->importConfigJson(wrong_protocol_stack_def);
  hnz_conf->importExchangedDataJson(exchanged_data_def);

  ASSERT_FALSE(hnz_conf->is_complete());
}

TEST(HNZCONF, ProtocolStackImportErrors) {
  std::shared_ptr<HNZConf> hnz_conf = std::make_shared<HNZConf>();
  hnz_conf->importExchangedDataJson(exchanged_data_def);

  std::string configParseError = QUOTE({42});
  hnz_conf->importConfigJson(configParseError);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configRootNotObject = QUOTE([]);
  hnz_conf->importConfigJson(configRootNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoProtocolStack = QUOTE({});
  hnz_conf->importConfigJson(configNoProtocolStack);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configProtocolStackNotObject = QUOTE({
    "protocol_stack": 42
  });
  hnz_conf->importConfigJson(configProtocolStackNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoName = QUOTE({
    "protocol_stack": {}
  });
  hnz_conf->importConfigJson(configNoName);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNameNotString = QUOTE({
    "protocol_stack": {
      "name": 42
    }
  });
  hnz_conf->importConfigJson(configNameNotString);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoVersion = QUOTE({
    "protocol_stack": {
      "name": "test"
    }
  });
  hnz_conf->importConfigJson(configNoVersion);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configVersionNotString = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": 42
    }
  });
  hnz_conf->importConfigJson(configVersionNotString);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoTransportLayer = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1"
    }
  });
  hnz_conf->importConfigJson(configNoTransportLayer);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configTransportLayerNotObject = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": 42
    }
  });
  hnz_conf->importConfigJson(configTransportLayerNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoConnections = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {}
    }
  });
  hnz_conf->importConfigJson(configNoConnections);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configConnectionsNotArray = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": 42
      }
    }
  });
  hnz_conf->importConfigJson(configConnectionsNotArray);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configConnectionsEmpty = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": []
      }
    }
  });
  hnz_conf->importConfigJson(configConnectionsEmpty);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configConnectionsNotContainingOneObject = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [42]
      }
    }
  });
  hnz_conf->importConfigJson(configConnectionsNotContainingOneObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoSrvIp = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{}]
      }
    }
  });
  hnz_conf->importConfigJson(configNoSrvIp);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configConnectionsNotContainingTwoObjects = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{}, 42]
      }
    }
  });
  hnz_conf->importConfigJson(configConnectionsNotContainingTwoObjects);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configConnectionsNotContainingTwoObjects2 = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [42, {}]
      }
    }
  });
  hnz_conf->importConfigJson(configConnectionsNotContainingTwoObjects2);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configSrvIpNotString = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": 42
        }]
      }
    }
  });
  hnz_conf->importConfigJson(configSrvIpNotString);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoPort = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0"
        }]
      }
    }
  });
  hnz_conf->importConfigJson(configNoPort);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configPortNotInt = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": "nope"
        }]
      }
    }
  });
  hnz_conf->importConfigJson(configPortNotInt);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoApplicationLayer = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      }
    }
  });
  hnz_conf->importConfigJson(configNoApplicationLayer);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configApplicationLayerNotObject = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": 42
    }
  });
  hnz_conf->importConfigJson(configApplicationLayerNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configApplicationLayerNoContent = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {}
    }
  });
  hnz_conf->importConfigJson(configApplicationLayerNoContent);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configRemoteStationAddrOutOfBounds = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 142
      }
    }
  });
  hnz_conf->importConfigJson(configRemoteStationAddrOutOfBounds);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configApplicationLayerWrongContentType = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": "nope",
        "inacc_timeout": "nope",
        "max_sarm": "nope",
        "repeat_path_A": "nope",
        "repeat_path_B": "nope",
        "repeat_timeout": "nope",
        "anticipation_ratio": "nope",
        "test_msg_send": 42,
        "test_msg_receive": 42,
        "gi_schedule": 42,
        "gi_repeat_count": "nope",
        "gi_time": "nope",
        "c_ack_time": "nope",
        "cmd_recv_timeout": "nope"
      }
    }
  });
  hnz_conf->importConfigJson(configApplicationLayerWrongContentType);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configInvalidBulleFormat = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "42",
        "test_msg_receive": "zzzz",
        "gi_schedule": "1805",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configInvalidBulleFormat);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configInvalidGiScheduleFormat = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "18?05",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configInvalidGiScheduleFormat);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configInvalidGiScheduleFormat2 = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "zz:zz",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configInvalidGiScheduleFormat2);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configGiScheduleOutOfBounds = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "-1:00",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configGiScheduleOutOfBounds);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configGiScheduleOutOfBounds2 = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "80:00",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configGiScheduleOutOfBounds2);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configGiScheduleOutOfBounds3 = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "00:-1",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configGiScheduleOutOfBounds3);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configGiScheduleOutOfBounds4 = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "00:80",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      }
    }
  });
  hnz_conf->importConfigJson(configGiScheduleOutOfBounds4);
  ASSERT_FALSE(hnz_conf->is_complete());

  // "south_monitoring" is optional, so all configs below are considered valid
  std::string configSouthMonitoringNotObject = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "18:05",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      },
      "south_monitoring": 42
    }
  });
  hnz_conf->importConfigJson(configSouthMonitoringNotObject);
  ASSERT_TRUE(hnz_conf->is_complete());

  std::string configNoAsset = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "18:05",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      },
      "south_monitoring": {}
    }
  });
  hnz_conf->importConfigJson(configNoAsset);
  ASSERT_TRUE(hnz_conf->is_complete());
  
  std::string configAssetNotString = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "18:05",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      },
      "south_monitoring": {
        "asset": 42
      }
    }
  });
  hnz_conf->importConfigJson(configAssetNotString);
  ASSERT_TRUE(hnz_conf->is_complete());

  std::string configValid = QUOTE({
    "protocol_stack": {
      "name": "test",
      "version": "1",
      "transport_layer": {
        "connections": [{
          "srv_ip": "0.0.0.0",
          "port": 42
        }]
      },
      "application_layer": {
        "remote_station_addr": 12,
        "inacc_timeout": 200,
        "max_sarm": 40,
        "repeat_path_A": 5,
        "repeat_path_B": 2,
        "repeat_timeout": 2000,
        "anticipation_ratio": 5,
        "test_msg_send": "1305",
        "test_msg_receive": "1306",
        "gi_schedule": "18:05",
        "gi_repeat_count": 5,
        "gi_time": 300,
        "c_ack_time": 20,
        "cmd_recv_timeout": 200000
      },
      "south_monitoring": {
        "asset": "TEST"
      }
    }
  });
  hnz_conf->importConfigJson(configValid);
  ASSERT_TRUE(hnz_conf->is_complete());
}

TEST(HNZCONF, ExchangedDataImportErrors) {
  std::shared_ptr<HNZConf> hnz_conf = std::make_shared<HNZConf>();
  hnz_conf->importConfigJson(protocol_stack_def);

  std::string configParseError = QUOTE({42});
  hnz_conf->importExchangedDataJson(configParseError);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configRootNotObject = QUOTE([]);
  hnz_conf->importExchangedDataJson(configRootNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoExchangedData = QUOTE({});
  hnz_conf->importExchangedDataJson(configNoExchangedData);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configExchangedDataNotObject = QUOTE({
    "exchanged_data": 42
  });
  hnz_conf->importExchangedDataJson(configExchangedDataNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoName = QUOTE({
    "exchanged_data": {}
  });
  hnz_conf->importExchangedDataJson(configNoName);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNameNotString = QUOTE({
    "exchanged_data": {
      "name": 42
    }
  });
  hnz_conf->importExchangedDataJson(configNameNotString);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoVersion = QUOTE({
    "exchanged_data": {
      "name": "test"
    }
  });
  hnz_conf->importExchangedDataJson(configNoVersion);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configVersionNotString = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": 42
    }
  });
  hnz_conf->importExchangedDataJson(configVersionNotString);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configNoDatapoints = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1"
    }
  });
  hnz_conf->importExchangedDataJson(configNoDatapoints);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configDatapointsNotArray = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": 42
    }
  });
  hnz_conf->importExchangedDataJson(configNoDatapoints);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configDatapointsContentNotObject = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [42]
    }
  });
  hnz_conf->importExchangedDataJson(configDatapointsContentNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configDatapointsContentEmpty = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{}]
    }
  });
  hnz_conf->importExchangedDataJson(configDatapointsContentEmpty);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configDatapointsWrongContentTypes = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": 42,
        "pivot_id": 42,
        "pivot_type": 42,
        "protocols": 42
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configDatapointsWrongContentTypes);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configProtocolsContentNotObject = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [42]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configProtocolsContentNotObject);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configProtocolsNoName = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{}]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configProtocolsNoName);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configProtocolsNameNotString = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": 42
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configProtocolsNameNotString);
  ASSERT_FALSE(hnz_conf->is_complete());

  // Messages from non-HNZ protocol are skipped, so config is valid
  std::string configProtocolsNameNotHnz = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "test"
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configProtocolsNameNotHnz);
  ASSERT_TRUE(hnz_conf->is_complete());

  std::string configProtocolsNoContent = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip"
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configProtocolsNoContent);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configProtocolsWrongContentTypes = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": 42,
          "typeid": 42
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configProtocolsWrongContentTypes);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configAddressNotConvertibleToInt = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "zzz",
          "typeid": 42
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configAddressNotConvertibleToInt);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configAddressOutOfRange = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "18446744073709551616",
          "typeid": "test"
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configAddressOutOfRange);
  ASSERT_FALSE(hnz_conf->is_complete());

  std::string configValid = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "42",
          "typeid": "test"
        }]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configValid);
  ASSERT_TRUE(hnz_conf->is_complete());

  // Test with the wrong type for pivot_subtypes 
  std::string configPivotSubtypesString = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "42",
          "typeid": "test"
        }],
        "pivot_subtypes": "test"
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configPivotSubtypesString);
  ASSERT_TRUE(hnz_conf->is_complete());

  // Test with an empty array for pivot_subtypes 
  std::string configPivotSubtypesEmptyArray = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "42",
          "typeid": "test"
        }],
        "pivot_subtypes": []
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configPivotSubtypesEmptyArray);
  ASSERT_TRUE(hnz_conf->is_complete());

  // Test with the wrong array type for pivot_subtypes 
  std::string configPivotSubtypesIntegerArray = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "42",
          "typeid": "test"
        }],
        "pivot_subtypes": [
          42
        ]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configPivotSubtypesIntegerArray);
  ASSERT_TRUE(hnz_conf->is_complete());

  // Test with the string content for pivot_subtypes 
  std::string configPivotSubtypesWrongStringArray = QUOTE({
    "exchanged_data": {
      "name": "test",
      "version": "1",
      "datapoints": [{
        "label": "test",
        "pivot_id": "test",
        "pivot_type": "test",
        "protocols": [{
          "name": "hnzip",
          "address": "42",
          "typeid": "test"
        }],
        "pivot_subtypes": [
          "42"
        ]
      }]
    }
  });
  hnz_conf->importExchangedDataJson(configPivotSubtypesWrongStringArray);
  ASSERT_TRUE(hnz_conf->is_complete());
}

TEST(HNZCONF, GetNumberCG) {
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  ASSERT_FALSE(conf->is_complete());
  ASSERT_EQ(conf->getNumberCG(), 0);

  conf = std::make_shared<HNZConf>(protocol_stack_def, exchanged_data_def);
  ASSERT_TRUE(conf->is_complete());
  ASSERT_EQ(conf->getNumberCG(), 2);
}

