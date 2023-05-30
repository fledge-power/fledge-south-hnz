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
        "pivot_id" : "ID114561",
        "pivot_type" : "SpsTyp",
        "protocols" : [
          {"name" : "iec104", "address" : "45-672", "typeid" : "M_SP_TB_1"}, {
            "name" : "tase2",
            "address" : "S_114561",
            "typeid" : "Data_StateQTimeTagExtended"
          },
          {
            "name" : "hnz",
            "station_address" : 1,
            "message_address" : 511,
            "message_code" : "TS"
          }
        ]
      },
      {
        "label" : "TS2",
        "pivot_id" : "ID114562",
        "pivot_type" : "SpsTyp",
        "protocols" : [
          {"name" : "iec104", "address" : "45-672", "typeid" : "M_SP_TB_2"}, {
            "name" : "tase2",
            "address" : "S_114562",
            "typeid" : "Data_StateQTimeTagExtended"
          },
          {
            "name" : "hnz",
            "station_address" : 1,
            "message_address" : 522,
            "message_code" : "TS"
          }
        ]
      },
      {
        "label" : "TS3",
        "pivot_id" : "ID114567",
        "pivot_type" : "SpsTyp",
        "protocols" : [
          {"name" : "iec104", "address" : "45-672", "typeid" : "M_SP_TB_7"}, {
            "name" : "tase2",
            "address" : "S_114567",
            "typeid" : "Data_StateQTimeTagExtended"
          },
          {
            "name" : "hnz",
            "station_address" : 1,
            "message_address" : 577,
            "message_code" : "TS"
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
          "message_code" : "TM"
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
          "message_code" : "TM"
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
          "message_code" : "TM"
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
          "message_code" : "TM"
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
         "1, \"max_sarm\" : 5, \"gi_time\" : 1, \"gi_repeat_count\" : 2 } } }";
}

class HNZTestComp : public HNZ {
 public:
  HNZTestComp() : HNZ() {}
  void sendCG() {
    m_hnz_connection->sendInitialGI();
  }
};

class HNZTest : public testing::Test {
 public:
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
      delete popFrontReading();
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

  struct ReadingInfo {
    std::string type;
    std::string value;
  };
  static void validateReading(Reading* currentReading, std::string assetName, std::map<std::string, ReadingInfo> attributes) {
    ASSERT_NE(nullptr, currentReading) << assetName << ": Invalid reading";
    ASSERT_EQ(assetName, currentReading->getAssetName());
    // Validate data_object structure received
    ASSERT_TRUE(hasObject(*currentReading, "data_object")) << assetName << ": data_object not found";
    Datapoint* data_object = getObject(*currentReading, "data_object");
    ASSERT_NE(nullptr, data_object) << assetName << ": data_object is null";
    // Validate existance of the required keys and non-existance of the others
    for(const std::string& name: allAttributeNames) {
      ASSERT_EQ(hasChild(*data_object, name), static_cast<bool>(attributes.count(name))) << assetName << ": Attribute not found: " << name;;
    }
    // Validate value and type of each key
    for(auto const& kvp: attributes) {
      const std::string& name = kvp.first;
      const std::string& type = kvp.second.type;
      const std::string& expectedValue = kvp.second.value;
      if(type == std::string("string")) {
        ASSERT_EQ(expectedValue, getStrValue(getChild(*data_object, name))) << assetName << ": Unexpected value for attribute " << name;
      }
      else if(type == std::string("int64_t")) {
        ASSERT_EQ(std::stoll(expectedValue), getIntValue(getChild(*data_object, name))) << assetName << ": Unexpected value for attribute " << name;
      }
      else {
        FAIL() << assetName << ": Unknown type: " << type;
      }
    }

    delete currentReading;
  }

  static std::shared_ptr<MSG_TRAME> findFrameWithId(const std::vector<std::shared_ptr<MSG_TRAME>>& frames, unsigned char frameId) {
    std::shared_ptr<MSG_TRAME> frameFound = nullptr;
    for(auto frame: frames) {
      if((frame->usLgBuffer > 2) && (frame->aubTrame[2] == frameId)) {
        frameFound = frame;
        break;
      }
    }
    return frameFound;
  }

  static void validateFrame(const std::vector<std::shared_ptr<MSG_TRAME>>& frames,
                            const std::vector<unsigned char>& expectedFrame, bool fullFrame = false) {
    // When fullFrame is true, expectedFrame shall contain the complete frame:
    // | NPC   | A/B | 1 |
    // | NR | P | NS | 0 |
    // | Function code   |
    // | Data            |
    // | FCS             |
    // | FCS             |
    // When fullFrame is false, expectedFrame shall contain only the function code and data bytes:
    // | Function code   |
    // | Data            |
    int minSize = fullFrame ? 2 : 0;
    ASSERT_GT(expectedFrame.size(), minSize) << "Cannot search for empty frame";
    unsigned char frameId = fullFrame ? expectedFrame[2] : expectedFrame[0];
    std::shared_ptr<MSG_TRAME> frameFound = findFrameWithId(frames, frameId);
    ASSERT_NE(frameFound.get(), nullptr) << "Could not find frame with id " << BasicHNZServer::toHexStr(frameId) <<
                                            " in frames received: " << BasicHNZServer::framesToStr(frames);

    int expectedLength = expectedFrame.size();
    if(!fullFrame) {
      expectedLength += 4;
    }
    ASSERT_EQ(frameFound->usLgBuffer, expectedLength) << "Unexpected length for frame with id " << BasicHNZServer::toHexStr(frameId);
    for(int i=0 ; i<expectedLength ; i++){
      if(!fullFrame){
        // Ignore the first two bytes (NPC + A/B, NR + P + NS) and the last two bytes (FCS x2)
        if(i < 2){
          continue;
        }
        if(i >= expectedLength - 2){
          break;
        }
      }
      int expIndex = fullFrame ? i : i-2;
      ASSERT_EQ(frameFound->aubTrame[i], expectedFrame[expIndex]) << "mismatch at byte: " << i << BasicHNZServer::frameToStr(frameFound);
    }
  }

  // When a test using a BasicHNZServer completes, the server is not destroyed immediately
  // so the next test can start before the server is deleted.
  // Because of that the port of the server from the previous test is still in use, so when starting a new test
  // we need a new port to make sure we do not run into server initialization error because of port already in use.
  static int getNextPort() {
    static int port = 6000;
    port++;
    return port;
  }

  static Reading* popFrontReading() {
    Reading* currentReading = nullptr;
    if (!storedReadings.empty()) {
      currentReading = storedReadings.front();
      storedReadings.pop();
    }
    return currentReading;
  }

  static HNZTestComp* hnz;
  static int ingestCallbackCalled;
  static queue<Reading*> storedReadings;
  static const std::vector<std::string> allAttributeNames;
  static constexpr unsigned long oneHourMs = 3600000; // 60 * 60 * 1000
  static constexpr unsigned long oneDayMs = 86400000; // 24 * 60 * 60 * 1000
  static constexpr unsigned long tenMinMs = 600000;   // 10 * 60 * 1000
};

HNZTestComp* HNZTest::hnz;
int HNZTest::ingestCallbackCalled;
queue<Reading*> HNZTest::storedReadings;
const std::vector<std::string> HNZTest::allAttributeNames = {
  "do_type", "do_station", "do_addr", "do_value", "do_valid", "do_ts", "do_ts_iv", "do_ts_c", "do_ts_s", "do_cg"
};
constexpr unsigned long HNZTest::oneHourMs;
constexpr unsigned long HNZTest::oneDayMs;
constexpr unsigned long HNZTest::tenMinMs;

class ServersWrapper {
  public:
    ServersWrapper(int addr, int port1, int port2=0) {
      m_server1 = std::make_shared<BasicHNZServer>(port1, addr);
      m_server1->startHNZServer();

      if(port2 > 0) {
        m_server2 = std::make_shared<BasicHNZServer>(port2, addr);
        m_server2->startHNZServer();
      }

      // Start HNZ Plugin
      HNZTest::startHNZ(port1, port2); 
    }
    std::shared_ptr<BasicHNZServer> server1() {
      if (!m_server1->HNZServerIsReady()) {
        m_server1 = nullptr;
      }
      return m_server1;
    }
    std::shared_ptr<BasicHNZServer> server2() {
      if (!m_server2->HNZServerIsReady()) {
        m_server2 = nullptr;
      }
      return m_server2;
    }
  private:
    std::shared_ptr<BasicHNZServer> m_server1;
    std::shared_ptr<BasicHNZServer> m_server2;
};

TEST_F(HNZTest, TCPConnectionOnePathOK) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
}

TEST_F(HNZTest, ReceivingTSCEMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";

  ///////////////////////////////////////
  // Validate epoch timestamp encoding
  ///////////////////////////////////////
  // Default is 0
  std::chrono::time_point<std::chrono::system_clock> dateTime = {};
  unsigned char daySection = 0;
  unsigned int ts = 0;
  unsigned long epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  unsigned long expectedEpochMs = 0;
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid default value";

  // Any time from dateTime is erazed
  dateTime += std::chrono::hours{1};
  epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid use of time from dateTime";

  // One day added to datetime is visible in epoch time
  dateTime += std::chrono::hours{24};
  expectedEpochMs += oneDayMs;
  epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid scale of dateTime";

  // One day section represents 10 mins
  daySection += 1;
  expectedEpochMs += tenMinMs;
  epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid scale of daySection";

  // One ts represents 10 miliseconds
  ts += 1;
  expectedEpochMs += 10;
  epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid scale of ts";

  // End of day section received at beginning of local day result in local day -1 being used in timestamp
  dateTime = {};
  dateTime += std::chrono::hours{48}; // 2 days
  daySection = 143;
  ts = 0;
  expectedEpochMs = oneDayMs + (tenMinMs * 143);
  epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid end of day management";

  // Start of day section received at end of local day result in local day +1 being used in timestamp
  dateTime += std::chrono::hours{-1}; // 1 day 23 h
  daySection = 0;
  expectedEpochMs = oneDayMs * 2;
  epochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  ASSERT_EQ(epochMs, expectedEpochMs) << "Invalid start of day management";

  ///////////////////////////////////////
  // Send TS1
  ///////////////////////////////////////
  dateTime = std::chrono::system_clock::now();
  daySection = 0;
  ts = 14066;
  expectedEpochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  unsigned char msb = static_cast<unsigned char>(ts >> 8);
  unsigned char lsb = static_cast<unsigned char>(ts & 0xFF);
  server->sendFrame({0x0B, 0x33, 0x28, msb, lsb}, false);
  printf("[HNZ Server] TSCE sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  Reading* currentReading = popFrontReading();
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_ts", {"int64_t", to_string(expectedEpochMs)}},
    {"do_ts_iv", {"int64_t", "0"}},
    {"do_ts_c", {"int64_t", "0"}},
    {"do_ts_s", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TS1 with invalid flag
  ///////////////////////////////////////
  server->sendFrame({0x0B, 0x33, 0x38, msb, lsb}, false);
  printf("[HNZ Server] TSCE 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_ts", {"int64_t", to_string(expectedEpochMs)}},
    {"do_ts_iv", {"int64_t", "0"}},
    {"do_ts_c", {"int64_t", "0"}},
    {"do_ts_s", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TS1 with modified day section
  ///////////////////////////////////////
  daySection = 12;
  expectedEpochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  server->sendFrame({0x0F, daySection}, false);
  server->sendFrame({0x0B, 0x33, 0x38, msb, lsb}, false);
  printf("[HNZ Server] TSCE 3 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_ts", {"int64_t", to_string(expectedEpochMs)}},
    {"do_ts_iv", {"int64_t", "0"}},
    {"do_ts_c", {"int64_t", "0"}},
    {"do_ts_s", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TS1 with modified timestamp
  ///////////////////////////////////////
  ts = ts + 6000;
  expectedEpochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  msb = static_cast<unsigned char>(ts >> 8);
  lsb = static_cast<unsigned char>(ts & 0xFF);
  server->sendFrame({0x0B, 0x33, 0x38, msb, lsb}, false);
  printf("[HNZ Server] TSCE 4 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_ts", {"int64_t", to_string(expectedEpochMs)}},
    {"do_ts_iv", {"int64_t", "0"}},
    {"do_ts_c", {"int64_t", "0"}},
    {"do_ts_s", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, ReceivingTSCGMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";

  ///////////////////////////////////////
  // CG abandonned after gi_repeat_count retries
  ///////////////////////////////////////

  hnz->sendCG();
  printf("[HNZ south plugin] CG request sent\n");
  this_thread::sleep_for(chrono::milliseconds(500)); // must be < gi_time
  int totalCG = 3; // initial CG (1) + gi_repeat_count (2)
  for(int i=0 ; i<totalCG ; i++) {
    // Find the CG frame in the list of frames received by server and validate it
    printf("Validating CG frame %d\n", i);
    validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
    if(HasFatalFailure()) return;
    this_thread::sleep_for(chrono::milliseconds(1000)); // gi_time
  }
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> CGframe = findFrameWithId(frames, 0x13);
  ASSERT_EQ(CGframe.get(), nullptr) << "No CG frame should be sent after gi_repeat_count was reached, but found: " << BasicHNZServer::frameToStr(CGframe);
  

  ///////////////////////////////////////
  // Send TS1 + TS2 + TS3 as CG answer
  ///////////////////////////////////////
  hnz->sendCG();
  printf("[HNZ south plugin] CG request 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(500)); // must be < gi_time

  // Find the CG frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  // Send only one of the two expected TS
  server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
  printf("[HNZ Server] TSCG 1 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1200)); // gi_time + 200ms

  // Only one of the 2 TS CG messages were sent, check that the TS have not yet been transmitted
  ASSERT_EQ(ingestCallbackCalled, 0);
  // Extra CG messages should have been sent automatically because some TS are missing and gi_time was reached
  frames = server->popLastFramesReceived();
  CGframe = findFrameWithId(frames, 0x13);
  ASSERT_NE(CGframe.get(), nullptr) << "Cound not find CG in frames received: " << BasicHNZServer::framesToStr(frames);

  // Send both TS this time (new CG was sent so the TS received earlier is ignored)
  server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x01, 0x00, 0x00}, false);
  printf("[HNZ Server] TSCG 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1200)); // gi_time + 200ms

  // All TS were received so no more CG should be sent automatically any more
  frames = server->popLastFramesReceived();
  CGframe = findFrameWithId(frames, 0x13);
  ASSERT_EQ(CGframe.get(), nullptr) << "No CG frame should be sent after all TS were received, but found: " << BasicHNZServer::frameToStr(CGframe);

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 3);
  ingestCallbackCalled = 0;
  Reading* currentReading = popFrontReading();
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "1"}}
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS2", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "522"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "1"}}
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS3", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "577"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "1"}}
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TS1 + TS2 + TS3 as CG answer with invalid flag for TS3
  ///////////////////////////////////////
  hnz->sendCG();
  printf("[HNZ south plugin] CG request 3 sent\n");
  this_thread::sleep_for(chrono::milliseconds(500)); // must not be too close to a multiple of gi_time

  // Find the CG frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  printf("[HNZ Server] TSCG 3 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 3);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "0"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "1"}}
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS2", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "522"}},
    {"do_value", {"int64_t", "0"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "1"}}
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReading();
  validateReading(currentReading, "TS3", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "577"}},
    {"do_value", {"int64_t", "0"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "1"}}
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, ReceivingTMAMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";

  ///////////////////////////////////////
  // Send TMA
  ///////////////////////////////////////
  int values[] = {-127, -1, 1, 127};
  unsigned char val0 = static_cast<unsigned char>((-values[0]) ^ 0xFF); // Ones' complement
  unsigned char val1 = static_cast<unsigned char>((-values[1]) ^ 0xFF); // Ones' complement
  unsigned char val2 = static_cast<unsigned char>(values[2]);
  unsigned char val3 = static_cast<unsigned char>(values[3]);
  server->sendFrame({0x02, 0x14, val0, val1, val2, val3}, false);
  printf("[HNZ Server] TMA sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(ingestCallbackCalled, 4);
  ingestCallbackCalled = 0;

  Reading* currentReading = nullptr;
  for (int i = 0; i < 4; i++) {
    currentReading = popFrontReading();
    validateReading(currentReading, "TM" + to_string(i + 1), {
      {"do_type", {"string", "TM"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", std::to_string(20 + i)}},
      {"do_value", {"int64_t", std::to_string(values[i])}},
      {"do_valid", {"int64_t", "0"}},
      {"do_an", {"string", "TMA"}},
    });
    if(HasFatalFailure()) return;
  }

  ///////////////////////////////////////
  // Send TMA with invalid flag for the last one
  ///////////////////////////////////////
  server->sendFrame({0x02, 0x14, val0, val1, val2, 0xFF}, false);
  printf("[HNZ Server] TMA 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(ingestCallbackCalled, 4);
  ingestCallbackCalled = 0;

  for (int i = 0; i < 4; i++) {
    currentReading = popFrontReading();
    if (i < 3) {
      validateReading(currentReading, "TM" + to_string(i + 1), {
        {"do_type", {"string", "TM"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", std::to_string(20 + i)}},
        {"do_value", {"int64_t", std::to_string(values[i])}},
        {"do_valid", {"int64_t", "0"}},
        {"do_an", {"string", "TMA"}},
      });
    } else {
      validateReading(currentReading, "TM" + to_string(i + 1), {
        {"do_type", {"string", "TM"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", std::to_string(20 + i)}},
        {"do_value", {"int64_t", "0"}},
        {"do_valid", {"int64_t", "1"}},
        {"do_an", {"string", "TMA"}},
      });
    }
    if(HasFatalFailure()) return;
  }
}

TEST_F(HNZTest, ReceivingTMNMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";

  ///////////////////////////////////////
  // Send TMN 8 bits
  ///////////////////////////////////////
  int values[] = {1, 42, 142, 255};
  unsigned char val0 = static_cast<unsigned char>(values[0]);
  unsigned char val1 = static_cast<unsigned char>(values[1]);
  unsigned char val2 = static_cast<unsigned char>(values[2]);
  unsigned char val3 = static_cast<unsigned char>(values[3]);
  server->sendFrame({0x0c, 0x14, val0, val1, val2, val3, 0x80}, false);
  printf("[HNZ Server] TM8 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(ingestCallbackCalled, 4);
  ingestCallbackCalled = 0;

  Reading* currentReading = nullptr;
  for (int i = 0; i < 4; i++) {
    currentReading = popFrontReading();
    validateReading(currentReading, "TM" + to_string(i + 1), {
      {"do_type", {"string", "TM"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", std::to_string(20 + i)}},
      {"do_value", {"int64_t", std::to_string(values[i])}},
      {"do_valid", {"int64_t", "0"}},
      {"do_an", {"string", "TM8"}},
    });
    if(HasFatalFailure()) return;
  }

  ///////////////////////////////////////
  // Send TMN 8 bits with invalid flag for the last one
  ///////////////////////////////////////
  server->sendFrame({0x0c, 0x14, val0, val1, val2, val3, 0x88}, false);
  printf("[HNZ Server] TM8 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(ingestCallbackCalled, 4);
  ingestCallbackCalled = 0;

  for (int i = 0; i < 4; i++) {
    currentReading = popFrontReading();
    if (i < 3) {
      validateReading(currentReading, "TM" + to_string(i + 1), {
        {"do_type", {"string", "TM"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", std::to_string(20 + i)}},
        {"do_value", {"int64_t", std::to_string(values[i])}},
        {"do_valid", {"int64_t", "0"}},
        {"do_an", {"string", "TM8"}},
      });
    } else {
      validateReading(currentReading, "TM" + to_string(i + 1), {
        {"do_type", {"string", "TM"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", std::to_string(20 + i)}},
        {"do_value", {"int64_t", std::to_string(values[i])}},
        {"do_valid", {"int64_t", "1"}},
        {"do_an", {"string", "TM8"}},
      });
    }
    if(HasFatalFailure()) return;
  }
  
  ///////////////////////////////////////
  // Send TMN 16 bits
  ///////////////////////////////////////
  int val11 = 420;
  int val12 = -420;
  unsigned char lsb1 = static_cast<unsigned char>(val11 & 0xFF);
  unsigned char msb1 = static_cast<unsigned char>(val11 >> 8);
  unsigned char lsb2 = static_cast<unsigned char>(val12 & 0xFF);
  unsigned char msb2 = static_cast<unsigned char>(val12 >> 8);
  server->sendFrame({0x0c, 0x14, lsb1, msb1, lsb2, msb2, 0x00}, false);
  printf("[HNZ Server] TM16 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called 2x time more
  ASSERT_EQ(ingestCallbackCalled, 2);
  ingestCallbackCalled = 0;

  currentReading = popFrontReading();
  validateReading(currentReading, "TM1", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "20"}},
    {"do_value", {"int64_t", std::to_string(val11)}},
    {"do_valid", {"int64_t", "0"}},
    {"do_an", {"string", "TM16"}},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReading();
  validateReading(currentReading, "TM3", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "22"}},
    {"do_value", {"int64_t", std::to_string(val12)}},
    {"do_valid", {"int64_t", "0"}},
    {"do_an", {"string", "TM16"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TMN 16 bits with invalid flag for the last one
  ///////////////////////////////////////
  server->sendFrame({0x0c, 0x14, lsb1, msb1, lsb2, msb2, 0x04}, false);
  printf("[HNZ Server] TM16 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Check that ingestCallback had been called 2x time more
  ASSERT_EQ(ingestCallbackCalled, 2);
  ingestCallbackCalled = 0;

  currentReading = popFrontReading();
  validateReading(currentReading, "TM1", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "20"}},
    {"do_value", {"int64_t", std::to_string(val11)}},
    {"do_valid", {"int64_t", "0"}},
    {"do_an", {"string", "TM16"}},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReading();
  validateReading(currentReading, "TM3", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "22"}},
    {"do_value", {"int64_t", std::to_string(val12)}},
    {"do_valid", {"int64_t", "1"}},
    {"do_an", {"string", "TM16"}},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, SendingTCMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";

  ///////////////////////////////////////
  // Send TC1
  ///////////////////////////////////////
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
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server
  server->sendFrame({0x09, 0x0e, 0x49}, false);
  printf("[HNZ Server] TC ACK sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  Reading* currentReading = popFrontReading();
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "ACK_TC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "142"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TC1 with negative ack (Critical fault)
  ///////////////////////////////////////
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  printf("[HNZ south plugin] TC 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 011b
  server->sendFrame({0x09, 0x0e, 0x4b}, false);
  printf("[HNZ Server] TC ACK 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "ACK_TC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "142"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TC1 with negative ack (Non critical fault)
  ///////////////////////////////////////
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  printf("[HNZ south plugin] TC 3 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 101b
  server->sendFrame({0x09, 0x0e, 0x4d}, false);
  printf("[HNZ Server] TC ACK 3 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "ACK_TC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "142"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TC1 with negative ack (Exterior fault)
  ///////////////////////////////////////
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  printf("[HNZ south plugin] TC 4 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 111b
  server->sendFrame({0x09, 0x0e, 0x4f}, false);
  printf("[HNZ Server] TC ACK 4 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "ACK_TC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "142"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TC1 with negative ack (Order not allowed)
  ///////////////////////////////////////
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  printf("[HNZ south plugin] TC 5 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 010b
  server->sendFrame({0x09, 0x0e, 0x4a}, false);
  printf("[HNZ Server] TC ACK 5 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "ACK_TC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "142"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, SendingTVCMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";

  ///////////////////////////////////////
  // Send TVC1
  ///////////////////////////////////////
  std::string operationTVC("TVC");
  int nbParamsTVC = 3;
  PLUGIN_PARAMETER paramTVC1 = {"type", "TVC"};
  PLUGIN_PARAMETER paramTVC2 = {"address", "31"};
  PLUGIN_PARAMETER paramTVC3 = {"value", "42"};
  PLUGIN_PARAMETER* paramsTVC[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3};
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  printf("[HNZ south plugin] TVC sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x1a, 0x1f, 0x2a, 0x00});
  if(HasFatalFailure()) return;

  // Send TVC ACK from server
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  printf("[HNZ Server] TVC ACK sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  Reading* currentReading = popFrontReading();
  validateReading(currentReading, "TVC1", {
    {"do_type", {"string", "ACK_TVC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "31"}},
    {"do_value", {"int64_t", "42"}},
    {"do_valid", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TVC1 with negative value
  ///////////////////////////////////////
  paramTVC3.value = "-42";
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  printf("[HNZ south plugin] TVC 2 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x1a, 0x1f, 0x2a, 0x80});
  if(HasFatalFailure()) return;

  // Send TVC ACK from server
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x80}, false);
  printf("[HNZ Server] TVC 2 ACK sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TVC1", {
    {"do_type", {"string", "ACK_TVC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "31"}},
    {"do_value", {"int64_t", "-42"}},
    {"do_valid", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TVC1 with negative ack
  ///////////////////////////////////////
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  printf("[HNZ south plugin] TVC 3 sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x1a, 0x1f, 0x2a, 0x80});
  if(HasFatalFailure()) return;

  // Send TVC ACK from server with A bit = 1
  server->sendFrame({0x0a, 0xdf, 0x2a, 0x80}, false);
  printf("[HNZ Server] TVC 3 ACK sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
  currentReading = popFrontReading();
  validateReading(currentReading, "TVC1", {
    {"do_type", {"string", "ACK_TVC"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "31"}},
    {"do_value", {"int64_t", "-42"}},
    {"do_valid", {"int64_t", "1"}},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, TCPConnectionTwoPathOK) {
  ServersWrapper wrapper(0x05, getNextPort(), getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  BasicHNZServer* server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection is not established in 10s...";
}

TEST_F(HNZTest, ReceivingMessagesTwoPath) {
  ServersWrapper wrapper(0x05, getNextPort(), getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  BasicHNZServer* server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection is not established in 10s...";

  server->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  server2->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  printf("[HNZ Server] TSCE sent on both path\n");
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
  this_thread::sleep_for(chrono::milliseconds(3000));

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(ingestCallbackCalled, 1);
}

TEST_F(HNZTest, SendingMessagesTwoPath) {
  ServersWrapper wrapper(0x05, getNextPort(), getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  BasicHNZServer* server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection is not established in 10s...";

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
  std::shared_ptr<MSG_TRAME> TCframe = findFrameWithId(frames, 0x19);
  ASSERT_NE(TCframe.get(), nullptr) << "Could not find TC in frames received: " << BasicHNZServer::framesToStr(frames);
  // Check that TC is only received on active path (server) and not on passive path (server2)
  frames = server2->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "No TC frame should be received by server2, found: " << BasicHNZServer::frameToStr(TCframe);

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
  int nbParamsTVC = 3;
  PLUGIN_PARAMETER paramTVC1 = {"type", "TVC"};
  PLUGIN_PARAMETER paramTVC2 = {"address", "31"};
  PLUGIN_PARAMETER paramTVC3 = {"value", "42"};
  PLUGIN_PARAMETER* paramsTVC[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3};
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  printf("[HNZ south plugin] TVC sent\n");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server
  frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_NE(TVCframe.get(), nullptr) << "Could not find TVC in frames received: " << BasicHNZServer::framesToStr(frames);
  // Check that TVC is only received on active path (server) and not on passive path (server2)
  frames = server2->popLastFramesReceived();
  TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_EQ(TCframe.get(), nullptr) << "No TVC frame should be received by server2, found: " << BasicHNZServer::frameToStr(TVCframe);

  // Send TVC ACK from servers
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  server2->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  printf("[HNZ Server] TVC ACK sent on both path\n");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(ingestCallbackCalled, 1);
  ingestCallbackCalled = 0;
}