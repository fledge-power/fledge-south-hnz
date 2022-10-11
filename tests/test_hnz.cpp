#include <config_category.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

#include <boost/thread.hpp>
#include <chrono>
#include <queue>
#include <utility>
#include <vector>

#include "hnz.h"
#include "hnz_server.h"

using namespace std;

#define TEST_PORT 6001

static string protocol_stack_def = QUOTE({
  "protocol_stack" : {
    "name" : "hnzclient",
    "version" : "1.0",
    "transport_layer" :
        {"connections" : [ {"srv_ip" : "0.0.0.0", "port" : TEST_PORT} ]},
    "application_layer" : {"remote_station_addr" : 1}
  }
});

static string exchanged_data_def = QUOTE({
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
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnz",
          "station_address" : 1,
          "message_address" : 20,
          "message_code" : "TMA"
        } ]
      },
      {
        "label" : "TM2",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnz",
          "station_address" : 1,
          "message_address" : 21,
          "message_code" : "TMA"
        } ]
      },
      {
        "label" : "TM3",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnz",
          "station_address" : 1,
          "message_address" : 22,
          "message_code" : "TMA"
        } ]
      },
      {
        "label" : "TM4",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnz",
          "station_address" : 1,
          "message_address" : 23,
          "message_code" : "TMA"
        } ]
      }
    ]
  }
});

class HNZTestComp : public HNZ {
 public:
  HNZTestComp() : HNZ() {}
};

class HNZTest : public testing::Test {
 protected:
  void SetUp() {
    // Create HNZ Plugin object
    if (hnz == nullptr) {
      hnz = new HNZTestComp();

      hnz->setJsonConfig(protocol_stack_def, exchanged_data_def);

      hnz->registerIngest(NULL, ingestCallback);
    }

    // Create a hnz server object
    if (server == nullptr) server = new HNZServer();
  }

  void TearDown() {
    hnz->stop();

    delete hnz;

    while (!storedReadings.empty()) {
      delete storedReadings.front();
      storedReadings.pop();
    }

    server->stop();
    delete server;
  }

  static void startHNZServer(int port) { server->start(port); }

  static void startHNZ() { hnz->start(); }

  static bool hasChild(Datapoint& dp, string childLabel) {
    DatapointValue& dpv = dp.getData();

    auto dps = dpv.getDpVec();

    for (auto sdp : *dps) {
      if (sdp->getName() == childLabel) {
        return true;
      }
    }

    return false;
  }

  static Datapoint* getChild(Datapoint& dp, string childLabel) {
    DatapointValue& dpv = dp.getData();

    auto dps = dpv.getDpVec();

    for (Datapoint* childDp : *dps) {
      if (childDp->getName() == childLabel) {
        return childDp;
      }
    }

    return nullptr;
  }

  static int64_t getIntValue(Datapoint* dp) {
    DatapointValue dpValue = dp->getData();
    return dpValue.toInt();
  }

  static string getStrValue(Datapoint* dp) {
    return dp->getData().toStringValue();
  }

  static bool hasObject(Reading& reading, string label) {
    vector<Datapoint*> dataPoints = reading.getReadingData();

    for (Datapoint* dp : dataPoints) {
      if (dp->getName() == label) {
        return true;
      }
    }

    return false;
  }

  static Datapoint* getObject(Reading& reading, string label) {
    vector<Datapoint*> dataPoints = reading.getReadingData();

    for (Datapoint* dp : dataPoints) {
      if (dp->getName() == label) {
        return dp;
      }
    }

    return nullptr;
  }

  static void ingestCallback(void* parameter, Reading reading) {
    printf("ingestCallback called -> asset: (%s)\n",
           reading.getAssetName().c_str());

    vector<Datapoint*> dataPoints = reading.getReadingData();

    printf("  number of readings: %lu\n", dataPoints.size());

    // for (Datapoint* sdp : dataPoints) {
    //     printf("name: %s value: %s\n", sdp->getName().c_str(),
    //     sdp->getData().toString().c_str());
    // }

    storedReadings.push(new Reading(reading));

    ingestCallbackCalled++;
  }

  // static boost::thread thread_;
  static HNZTestComp* hnz;
  static HNZServer* server;
  static int ingestCallbackCalled;
  static queue<Reading*> storedReadings;
};

// boost::thread HNZTest::thread_;
HNZTestComp* HNZTest::hnz;
HNZServer* HNZTest::server;
int HNZTest::ingestCallbackCalled;
queue<Reading*> HNZTest::storedReadings;

TEST_F(HNZTest, ReceivingMessages) {
  // Start a hnz test server
  long start = time(NULL);
  thread t1(startHNZServer, TEST_PORT);

  // Start HNZ Plugin
  startHNZ();

  // Check server is connected
  while (!server->isConnected()) {
    if (time(NULL) - start > 10) {
      // server->stop();
      FAIL() << "Something went wrong. Connection is not established in 10s...";
      break;
    }
    this_thread::sleep_for(chrono::milliseconds(500));
  }
  t1.join();

  // Send SARM
  unsigned char message[1];
  message[0] = 0x0F;
  server->createAndSendFr(0x05, message, sizeof(message));

  // Wait for UA and send UA in response of SARM
  bool ua_ok = false;
  bool sarm_ok = false;
  while (1) {
    unsigned char* data = server->receiveData();
    unsigned char c = data[1];
    switch (c) {
      case UA_CODE:
        printf("[HNZ Server] UA received\n");
        ua_ok = true;
        break;
      case SARM_CODE:
        printf("[HNZ Server] SARM received, sending UA\n");
        unsigned char message[1];
        message[0] = 0x63;
        server->createAndSendFr(0x07, message, sizeof(message));
        sarm_ok = true;
        break;
      default:
        printf("[HNZ Server] Neither UA nor SARM\n");
        break;
    }
    if (ua_ok && sarm_ok) break;
  }
  printf("[HNZ Server] Connection OK !!\n");

  printf("[HNZ Server] Sending a TSCE\n");
  unsigned char message1[6]{0x00, 0x0B, 0x33, 0x28, 0x36, 0xF2};
  server->createAndSendFr(0x05, message1, sizeof(message1));
  printf("[HNZ Server] TSCE sent\n");

  // Wait a lit bit to received the frame
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  Reading* currentReading = storedReadings.front();
  ASSERT_EQ("TS1", currentReading->getAssetName());

  ASSERT_TRUE(hasObject(*currentReading, "data_object"));
  Datapoint* data_object = getObject(*currentReading, "data_object");
  ASSERT_NE(nullptr, data_object);
  ASSERT_TRUE(hasChild(*data_object, "do_type"));
  ASSERT_TRUE(hasChild(*data_object, "do_station"));
  ASSERT_TRUE(hasChild(*data_object, "do_addr"));
  ASSERT_TRUE(hasChild(*data_object, "do_value"));
  ASSERT_TRUE(hasChild(*data_object, "do_valid"));
  ASSERT_TRUE(hasChild(*data_object, "do_ts"));
  ASSERT_TRUE(hasChild(*data_object, "do_ts_iv"));
  ASSERT_TRUE(hasChild(*data_object, "do_ts_c"));
  ASSERT_TRUE(hasChild(*data_object, "do_ts_s"));

  ASSERT_EQ("TSCE", getStrValue(getChild(*data_object, "do_type")));
  ASSERT_EQ((int64_t)1, getIntValue(getChild(*data_object, "do_station")));
  ASSERT_EQ((int64_t)511, getIntValue(getChild(*data_object, "do_addr")));
  ASSERT_EQ((int64_t)1, getIntValue(getChild(*data_object, "do_value")));
  ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_valid")));
  ASSERT_EQ((int64_t)14066, getIntValue(getChild(*data_object, "do_ts")));
  ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_ts_iv")));
  ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_ts_c")));
  ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_ts_s")));

  delete currentReading;
  storedReadings.pop();

  printf("[HNZ Server] Sending a TM4\n");
  unsigned char message2[8]{0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
  server->createAndSendFr(0x05, message2, sizeof(message2));
  printf("[HNZ Server] TM4 sent\n");

  // Wait a lit bit to received the frame
  this_thread::sleep_for(chrono::milliseconds(2000));

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(ingestCallbackCalled, 5);
  for (int i = 0; i < 4; i++) {
    currentReading = storedReadings.front();

    ASSERT_EQ("TM" + to_string(i + 1), currentReading->getAssetName());
    ASSERT_TRUE(hasObject(*currentReading, "data_object"));
    data_object = getObject(*currentReading, "data_object");
    ASSERT_NE(nullptr, data_object);
    ASSERT_TRUE(hasChild(*data_object, "do_type"));
    ASSERT_TRUE(hasChild(*data_object, "do_station"));
    ASSERT_TRUE(hasChild(*data_object, "do_addr"));
    ASSERT_TRUE(hasChild(*data_object, "do_value"));
    ASSERT_TRUE(hasChild(*data_object, "do_valid"));

    ASSERT_EQ("TMA", getStrValue(getChild(*data_object, "do_type")));
    ASSERT_EQ((int64_t)1, getIntValue(getChild(*data_object, "do_station")));
    ASSERT_EQ((int64_t)20 + i, getIntValue(getChild(*data_object, "do_addr")));
    ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_value")));
    ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_valid")));

    delete currentReading;
    storedReadings.pop();
  }
}
