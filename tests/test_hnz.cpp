#include <config_category.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

#include <boost/thread.hpp>
#include <chrono>
#include <queue>
#include <utility>
#include <vector>

#include "hnz.h"
#include "server/basic_hnz_server.h"

using namespace std;

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

string protocol_stack_generator(int port) {
  // For tests, we have to use different ports for the server because between 2
  // tests, socket isn't properly closed.
  return "{ \"protocol_stack\" : { \"name\" : \"hnzclient\", \"version\" : "
         "\"1.0\", \"transport_layer\" : { \"connections\" : [ {\"srv_ip\" : "
         "\"0.0.0.0\", \"port\" : " +
         to_string(port) +
         "} ]}, \"application_layer\" : { "
         "\"remote_station_addr\" : "
         "1 } } }";
}

class HNZTestComp : public HNZ {
 public:
  HNZTestComp() : HNZ() {}
};

class HNZTest : public testing::Test {
 protected:
  void SetUp() {
    // Create HNZ Plugin object
    hnz = new HNZTestComp();

    hnz->registerIngest(NULL, ingestCallback);

    ingestCallbackCalled = 0;
  }

  void TearDown() {
    hnz->stop();

    delete hnz;

    while (!storedReadings.empty()) {
      delete storedReadings.front();
      storedReadings.pop();
    }
  }

  static void startHNZ(int port) {
    hnz->setJsonConfig(protocol_stack_generator(port), exchanged_data_def);

    hnz->start();
  }

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
  static int ingestCallbackCalled;
  static queue<Reading*> storedReadings;
};

// boost::thread HNZTest::thread_;
HNZTestComp* HNZTest::hnz;
int HNZTest::ingestCallbackCalled;
queue<Reading*> HNZTest::storedReadings;

TEST_F(HNZTest, TCPConnectionOnePathOK) {
  BasicHNZServer* server = new BasicHNZServer(6001, 0x05);

  server->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6001);

  if (!server->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";

  SUCCEED();

  delete server;
}

TEST_F(HNZTest, ReceivingMessages) {
  BasicHNZServer* server = new BasicHNZServer(6002, 0x05);

  server->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6002);

  if (!server->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";

  server->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
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

  server->sendFrame({0x02, 0x02, 0x00, 0x00, 0x00, 0x00}, false);
  printf("[HNZ Server] TM4 sent\n");

  // Wait a lit bit to received the frame
  this_thread::sleep_for(chrono::milliseconds(1000));

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

  // server->stop();

  delete server;
}
