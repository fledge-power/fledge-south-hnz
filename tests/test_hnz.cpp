#include <config_category.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

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
      },
      {
        "label" : "TC1",
        "pivot_id" : "ID222222",
        "pivot_type" : "DPCTyp",
        "protocols" : [
          {
            "name": "hnz",
            "station_address": 1,
            "message_address": 142,
            "message_code": "ACK_TC"
          }
        ]
      },
      {
        "label" : "TVC1",
        "pivot_id" : "ID333333",
        "pivot_type" : "DPCTyp",
        "protocols" : [
          {
            "name": "hnz",
            "station_address": 1,
            "message_address": 31,
            "message_code": "ACK_TVC"
          }
        ]
      }
    ]
  }
});

string protocol_stack_generator(int port, int port2) {
  // For tests, we have to use different ports for the server because between 2
  // tests, socket isn't properly closed.
  return "{ \"protocol_stack\" : { \"name\" : \"hnzclient\", \"version\" : "
         "\"1.0\", \"transport_layer\" : { \"connections\" : [ {\"srv_ip\" : "
         "\"0.0.0.0\", \"port\" : " +
         to_string(port) + "}" +
         ((port2 != 0) ? ",{ \"srv_ip\" : \"0.0.0.0\", \"port\" : " +
                             to_string(port2) + "}"
                       : "") +
         " ] } , \"application_layer\" : { "
         "\"remote_station_addr\" : "
         "1, \"max_sarm\" : 5 } } }";
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

  static void startHNZ(int port, int port2) {
    hnz->setJsonConfig(protocol_stack_generator(port, port2),
                       exchanged_data_def);

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

  static HNZTestComp* hnz;
  static int ingestCallbackCalled;
  static queue<Reading*> storedReadings;
};

HNZTestComp* HNZTest::hnz;
int HNZTest::ingestCallbackCalled;
queue<Reading*> HNZTest::storedReadings;

TEST_F(HNZTest, TCPConnectionOnePathOK) {
  BasicHNZServer* server = new BasicHNZServer(6001, 0x05);

  server->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6001, 0);

  if (!server->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";

  SUCCEED();

  delete server;
}

TEST_F(HNZTest, ReceivingMessages) {
  BasicHNZServer* server = new BasicHNZServer(6002, 0x05);

  server->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6002, 0);

  if (!server->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";

  server->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  printf("[HNZ Server] TSCE sent\n");

  // Wait a lit bit to received the frame
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  Reading* currentReading = storedReadings.front();
  ASSERT_NE(nullptr, currentReading);
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

    ASSERT_NE(nullptr, currentReading);
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

  delete server;
}

TEST_F(HNZTest, SendingMessages) {
  BasicHNZServer* server = new BasicHNZServer(6007, 0x05);

  server->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6007, 0);

  if (!server->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";

  // Send TC1
  std::string operationTC("TC");
  int nbParamsTC = 3;
  PLUGIN_PARAMETER paramTC1 = {"type", "TC"};
  PLUGIN_PARAMETER paramTC2 = {"address", "142"};
  PLUGIN_PARAMETER paramTC3 = {"value", "1"};
  PLUGIN_PARAMETER* paramsTC[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3};
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  printf("[HNZ south plugin] TC sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TCframe = nullptr;
  for(auto frame: frames) {
    if((frame->usLgBuffer > 3) && (frame->aubTrame[2] == 0x19)) {
      TCframe = frame;
      break;
    }
  }
  ASSERT_NE(TCframe.get(), nullptr) << "Could not find TC in frames received: " << server->framesToStr(frames);
  int expectedTCLength = 7;
  unsigned char expectedFrameTC[expectedTCLength] = {0x07, 0x08, 0x19, 0x0e, 0x48, 0xa4, 0x57};
  ASSERT_EQ(TCframe->usLgBuffer, expectedTCLength);
  for(int i=0 ; i<expectedTCLength ; i++){
    ASSERT_EQ(TCframe->aubTrame[i], expectedFrameTC[i]) << "mismatch at byte: " << i << server->frameToStr(TCframe);
  }

  // Send TC ACK from server
  server->sendFrame({0x09, 0x0e, 0x49}, false);
  printf("[HNZ Server] TC ACK sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  Reading* currentReading = storedReadings.front();
  storedReadings.pop();
  ASSERT_NE(nullptr, currentReading);
  ASSERT_EQ("TC1", currentReading->getAssetName());
  // Validate TC structure received
  ASSERT_TRUE(hasObject(*currentReading, "data_object"));
  Datapoint* data_object = getObject(*currentReading, "data_object");
  ASSERT_NE(nullptr, data_object);
  ASSERT_TRUE(hasChild(*data_object, "do_type"));
  ASSERT_TRUE(hasChild(*data_object, "do_station"));
  ASSERT_TRUE(hasChild(*data_object, "do_addr"));
  ASSERT_TRUE(hasChild(*data_object, "do_value"));
  ASSERT_FALSE(hasChild(*data_object, "do_val_coding"));

  ASSERT_EQ("ACK_TC", getStrValue(getChild(*data_object, "do_type")));
  ASSERT_EQ((int64_t)1, getIntValue(getChild(*data_object, "do_station")));
  ASSERT_EQ((int64_t)142, getIntValue(getChild(*data_object, "do_addr")));
  ASSERT_EQ((int64_t)1, getIntValue(getChild(*data_object, "do_value")));

  delete currentReading;

  // Send TVC1
  std::string operationTVC("TVC");
  int nbParamsTVC = 4;
  PLUGIN_PARAMETER paramTVC1 = {"type", "TVC"};
  PLUGIN_PARAMETER paramTVC2 = {"address", "31"};
  PLUGIN_PARAMETER paramTVC3 = {"value", "42"};
  PLUGIN_PARAMETER paramTVC4 = {"val_coding", "0"};
  PLUGIN_PARAMETER* paramsTVC[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3, &paramTVC4};
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  printf("[HNZ south plugin] TVC sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TVCframe = nullptr;
  for(auto frame: frames) {
    if((frame->usLgBuffer > 3) && (frame->aubTrame[2] == 0x1a)) {
      TVCframe = frame;
      break;
    }
  }
  ASSERT_NE(TVCframe.get(), nullptr) << "Could not find TVC in frames received: " << server->framesToStr(frames);
  int expectedTVCLength = 8;
  unsigned char expectedFrameTVC[expectedTVCLength] = {0x07, 0x2a, 0x1a, 0x1f, 0x2a, 0x00, 0x79, 0xc9};
  ASSERT_EQ(TVCframe->usLgBuffer, expectedTVCLength);
  for(int i=0 ; i<expectedTVCLength ; i++){
    ASSERT_EQ(TVCframe->aubTrame[i], expectedFrameTVC[i]) << "mismatch at byte: " << i << server->frameToStr(TVCframe);
  }

  // Send TVC ACK from server
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  printf("[HNZ Server] TVC ACK sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = storedReadings.front();
  storedReadings.pop();
  ASSERT_NE(nullptr, currentReading);
  ASSERT_EQ("TVC1", currentReading->getAssetName());
  // Validate TVC structure received
  ASSERT_TRUE(hasObject(*currentReading, "data_object"));
  data_object = getObject(*currentReading, "data_object");
  ASSERT_NE(nullptr, data_object);
  ASSERT_TRUE(hasChild(*data_object, "do_type"));
  ASSERT_TRUE(hasChild(*data_object, "do_station"));
  ASSERT_TRUE(hasChild(*data_object, "do_addr"));
  ASSERT_TRUE(hasChild(*data_object, "do_value"));
  ASSERT_TRUE(hasChild(*data_object, "do_val_coding"));

  ASSERT_EQ("ACK_TVC", getStrValue(getChild(*data_object, "do_type")));
  ASSERT_EQ((int64_t)1, getIntValue(getChild(*data_object, "do_station")));
  ASSERT_EQ((int64_t)31, getIntValue(getChild(*data_object, "do_addr")));
  ASSERT_EQ((int64_t)42, getIntValue(getChild(*data_object, "do_value")));
  ASSERT_EQ((int64_t)0, getIntValue(getChild(*data_object, "do_val_coding")));

  delete currentReading;
  
  delete server;
}

TEST_F(HNZTest, TCPConnectionTwoPathOK) {
  BasicHNZServer* server = new BasicHNZServer(6003, 0x05);
  BasicHNZServer* server2 = new BasicHNZServer(6004, 0x05);

  server->startHNZServer();
  server2->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6003, 6004);

  if (!server->HNZServerIsReady()) {
    delete server2;
    FAIL() << "Something went wrong. Connection is not established in 10s...";
  }
  if (!server2->HNZServerIsReady()) {
    delete server;
    FAIL() << "Something went wrong. Connection is not established in 10s... ";
  }

  SUCCEED();

  delete server;
  delete server2;
}

TEST_F(HNZTest, ReceivingMessagesTwoPath) {
  BasicHNZServer* server = new BasicHNZServer(6005, 0x05);
  BasicHNZServer* server2 = new BasicHNZServer(6006, 0x05);

  server->startHNZServer();
  server2->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6005, 6006);

  if (!server->HNZServerIsReady()) {
    delete server2;
    FAIL() << "Something went wrong. Connection is not established in 10s...";
  }
  if (!server2->HNZServerIsReady()) {
    delete server;
    FAIL() << "Something went wrong. Connection is not established in 10s... ";
  }

  server->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  server2->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  printf("[HNZ Server] TSCE sent on both path\n");

  // Wait a lit bit to received the frame
  this_thread::sleep_for(chrono::milliseconds(3000));

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(ingestCallbackCalled, 1);

  // Send a SARM to put hnz plugin on path A in connection state
  // and don't send UA then to switch on path B
  server->sendSARM();

  // Wait 20s
  this_thread::sleep_for(chrono::milliseconds(30000));

  ingestCallbackCalled = 0;

  server2->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  printf("[HNZ Server] TSCE sent on path B\n");

  // Wait a lit bit to received the frame
  this_thread::sleep_for(chrono::milliseconds(3000));

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(ingestCallbackCalled, 1);

  delete server;
  delete server2;
}

TEST_F(HNZTest, SendingMessagesTwoPath) {
  BasicHNZServer* server = new BasicHNZServer(6008, 0x05);
  BasicHNZServer* server2 = new BasicHNZServer(6009, 0x05);

  server->startHNZServer();
  server2->startHNZServer();

  // Start HNZ Plugin
  startHNZ(6008, 6009);

  if (!server->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";
  if (!server2->HNZServerIsReady())
    FAIL() << "Something went wrong. Connection is not established in 10s...";

  // Send TC1
  std::string operationTC("TC");
  int nbParamsTC = 3;
  PLUGIN_PARAMETER paramTC1 = {"type", "TC"};
  PLUGIN_PARAMETER paramTC2 = {"address", "142"};
  PLUGIN_PARAMETER paramTC3 = {"value", "1"};
  PLUGIN_PARAMETER* paramsTC[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3};
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  printf("[HNZ south plugin] TC sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TCframe = nullptr;
  for(auto frame: frames) {
    if((frame->usLgBuffer > 3) && (frame->aubTrame[2] == 0x19)) {
      TCframe = frame;
      break;
    }
  }
  ASSERT_NE(TCframe.get(), nullptr) << "Could not find TC in frames received: " << server->framesToStr(frames);
  // Check that TC is only received on active path (server) and not on passive path (server2)
  frames = server2->popLastFramesReceived();
  for(auto frame: frames) {
    if((frame->usLgBuffer > 3) && (frame->aubTrame[2] == 0x19)) {
      FAIL() << "No TC frame should be received by server2, found: " << server2->frameToStr(frame);
    }
  }

  // Send TC ACK from servers
  server->sendFrame({0x09, 0x0e, 0x49}, false);
  server2->sendFrame({0x09, 0x0e, 0x49}, false);
  printf("[HNZ Server] TC ACK sent on both path\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;

  // Send TVC1
  std::string operationTVC("TVC");
  int nbParamsTVC = 4;
  PLUGIN_PARAMETER paramTVC1 = {"type", "TVC"};
  PLUGIN_PARAMETER paramTVC2 = {"address", "31"};
  PLUGIN_PARAMETER paramTVC3 = {"value", "42"};
  PLUGIN_PARAMETER paramTVC4 = {"val_coding", "0"};
  PLUGIN_PARAMETER* paramsTVC[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3, &paramTVC4};
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  printf("[HNZ south plugin] TVC sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server
  frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TVCframe = nullptr;
  for(auto frame: frames) {
    if((frame->usLgBuffer > 3) && (frame->aubTrame[2] == 0x1a)) {
      TVCframe = frame;
      break;
    }
  }
  ASSERT_NE(TVCframe.get(), nullptr) << "Could not find TVC in frames received: " << server->framesToStr(frames);
  // Check that TVC is only received on active path (server) and not on passive path (server2)
  frames = server2->popLastFramesReceived();
  for(auto frame: frames) {
    if((frame->usLgBuffer > 3) && (frame->aubTrame[2] == 0x1a)) {
      FAIL() << "No TVC frame should be received by server2, found: " << server2->frameToStr(frame);
    }
  }

  // Send TVC ACK from servers
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  server2->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  printf("[HNZ Server] TVC ACK sent on both path\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  
  delete server;
  delete server2;
}