#include <gtest/gtest.h>
#include <plugin_api.h>
#include <rapidjson/document.h>

#include <string>
#include <sstream>
#include <regex>
#include <thread>

#include "hnz.h"
#include "hnzconf.h"
#include "hnzconnection.h"
#include "hnzpath.h"

using namespace std;

const string protocol_stack_def = QUOTE({
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
      "gi_schedule": "00:00",
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

const string exchanged_data_def = QUOTE({
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

std::pair<int, int> getCurrentHoursMinutes() {
  unsigned long epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  unsigned long totalMinutes = epochMs / 60000;
  unsigned long totalHours = totalMinutes / 60;
  unsigned long totalDays = totalHours / 24;
  int hours = static_cast<int>(totalHours - (totalDays * 24));
  int minutes = static_cast<int>(totalMinutes - (totalHours * 60));
  return {hours, minutes};
}

TEST(HNZConnection, OnlyOnePathConfigured) {
  string protocol_stack_def_one_path = QUOTE({
    "protocol_stack" : {
      "name" : "hnzclient",
      "version" : "1.0",
      "transport_layer" : {"connections" : [ {"srv_ip" : "192.168.0.10"} ]},
      "application_layer" : {"remote_station_addr" : 12}
    }
  });

  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  conf->importConfigJson(protocol_stack_def_one_path);
  conf->importExchangedDataJson(exchanged_data_def);

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());

  ASSERT_NE(nullptr, hnz_connection->getActivePath().get());
  ASSERT_EQ(nullptr, hnz_connection->getPassivePath().get());
}

TEST(HNZConnection, TwoPathConfigured) {
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

  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  conf->importConfigJson(protocol_stack_def_two_path);
  conf->importExchangedDataJson(exchanged_data_def);

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());

  ASSERT_NE(nullptr, hnz_connection->getActivePath().get());
  ASSERT_NE(nullptr, hnz_connection->getPassivePath().get());
}

TEST(HNZConnection, NoPathConfigured) {
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());

  ASSERT_EQ(nullptr, hnz_connection->getActivePath().get());
  ASSERT_EQ(nullptr, hnz_connection->getPassivePath().get());
}

TEST(HNZConnection, GIScheduleInactive) {
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  std::string protocol_stack_custom = std::regex_replace(protocol_stack_def, std::regex("00:00"), "99:99");
  conf->importConfigJson(protocol_stack_custom);
  conf->importExchangedDataJson(exchanged_data_def);
  ASSERT_FALSE(conf->get_gi_schedule().activate);
  ASSERT_TRUE(conf->is_complete());

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());

  ASSERT_NE(nullptr, hnz_connection->getActivePath().get());
  ASSERT_NE(nullptr, hnz_connection->getPassivePath().get());

  hnz_connection->start();
  // Wait for thread HNZConnection::m_manageMessages() to start
  this_thread::sleep_for(chrono::milliseconds(1100));
  ASSERT_TRUE(hnz_connection->isRunning());
}

TEST(HNZConnection, GIScheduleActivePassed) {
  // If we are too close to midnight, wait long enough for the test to pass
  auto hmPair = getCurrentHoursMinutes();
  int hours = hmPair.first;
  int minutes = hmPair.second;
  if ((hours == 0) && (minutes == 0)) {
    this_thread::sleep_for(chrono::minutes(2));
  }

  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  conf->importConfigJson(protocol_stack_def);
  conf->importExchangedDataJson(exchanged_data_def);
  ASSERT_TRUE(conf->get_gi_schedule().activate);
  ASSERT_TRUE(conf->is_complete());

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());

  ASSERT_NE(nullptr, hnz_connection->getActivePath().get());
  ASSERT_NE(nullptr, hnz_connection->getPassivePath().get());

  hnz_connection->start();
  // Wait for thread HNZConnection::m_manageMessages() to start
  this_thread::sleep_for(chrono::milliseconds(1100));
  ASSERT_TRUE(hnz_connection->isRunning());
}

TEST(HNZConnection, GIScheduleActiveFuture) {
  // Get current hours and minutes to set a GI schedule in the near future
  auto hmPair = getCurrentHoursMinutes();
  int hours = hmPair.first;
  int minutes = hmPair.second;
  int delayMin = 2; // Program GI 2 minutes in the future, in case we are close to the end of current minute
  // If we are too close to midnight, wait long enough for the test to pass
  if ((hours == 23) && (minutes >= (60 - delayMin))) {
    this_thread::sleep_for(chrono::minutes(delayMin));
    hmPair = getCurrentHoursMinutes();
    hours = hmPair.first;
    minutes = hmPair.second;
  }

  minutes += delayMin; 
  if (minutes >= 60) {
    hours = (hours + 1) % 24;
    minutes = minutes % 60;
  }
  auto formatTime = [](int time) 
  { 
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << time;
    return ss.str();
  };
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  std::string giSchedule = formatTime(hours) + ":" + formatTime(minutes);
  std::string protocol_stack_custom = std::regex_replace(protocol_stack_def, std::regex("00:00"), giSchedule);
  conf->importConfigJson(protocol_stack_custom);
  conf->importExchangedDataJson(exchanged_data_def);
  ASSERT_TRUE(conf->get_gi_schedule().activate);
  ASSERT_TRUE(conf->is_complete());

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());

  ASSERT_NE(nullptr, hnz_connection->getActivePath().get());
  ASSERT_NE(nullptr, hnz_connection->getPassivePath().get());

  hnz_connection->start();
  // Wait for thread HNZConnection::m_manageMessages() to start
  this_thread::sleep_for(chrono::milliseconds(1100));
  ASSERT_TRUE(hnz_connection->isRunning());

  // Wait for scheduled GI
  this_thread::sleep_for(chrono::minutes(delayMin));
  ASSERT_EQ(hnz_connection->getGiStatus(), GiStatus::STARTED);
}

TEST(HNZConnection, DisconnectPathInDestructor) {
  std::shared_ptr<HNZConf> conf = std::make_shared<HNZConf>();
  conf->importConfigJson(protocol_stack_def);
  conf->importExchangedDataJson(exchanged_data_def);
  ASSERT_TRUE(conf->is_complete());

  std::unique_ptr<HNZ> hnz = make_unique<HNZ>();
  std::unique_ptr<HNZConnection> hnz_connection = make_unique<HNZConnection>(conf, hnz.get());
  std::shared_ptr<HNZPath> hnz_path = std::make_shared<HNZPath>(conf, hnz_connection.get(), false);
  ASSERT_NE(nullptr, hnz_path.get());

  // Start connecting on a thread and wait a little to let it enter the main connection loop
  std::unique_ptr<std::thread> connection_thread = make_unique<std::thread>(&HNZPath::connect, hnz_path.get());
  this_thread::sleep_for(chrono::milliseconds(100));

  // Destroy path object while connecting
  ASSERT_NO_THROW(hnz_path = nullptr);

  // Check that the thread can now be joined as HNZPath::disconnect() was called in destructor
  printf("[TEST HNZConnection] Waiting for connection thread to join..."); fflush(stdout);
  std::atomic<bool> joinSuccess{false};
  std::thread joinThread([&connection_thread, &joinSuccess](){
    connection_thread->join();
    joinSuccess = true;
  });
  joinThread.detach();
  int timeMs = 0;
  // Wait up to 60s for connection_thread thread to join
  while (timeMs < 60000) {
    if (joinSuccess) {
      break;
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    timeMs += 100;
  }
  ASSERT_TRUE(joinSuccess);
}