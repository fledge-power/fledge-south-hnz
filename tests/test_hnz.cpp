#include <config_category.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

#include <chrono>
#include <queue>
#include <utility>
#include <vector>
#include <mutex>
#include <sstream>
#include <regex>

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
            "name" : "hnzip",
            "address" : "511",
            "typeid" : "TS"
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
            "name" : "hnzip",
            "address" : "522",
            "typeid" : "TS"
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
            "name" : "hnzip",
            "address" : "577",
            "typeid" : "TS"
          }
        ]
      },
      {
        "label" : "TM1",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnzip",
          "address" : "20",
          "typeid" : "TM"
        } ]
      },
      {
        "label" : "TM2",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnzip",
          "address" : "21",
          "typeid" : "TM"
        } ]
      },
      {
        "label" : "TM3",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnzip",
          "address" : "22",
          "typeid" : "TM"
        } ]
      },
      {
        "label" : "TM4",
        "pivot_id" : "ID111111",
        "pivot_type" : "SpsTyp",
        "protocols" : [ {
          "name" : "hnzip",
          "address" : "23",
          "typeid" : "TM"
        } ]
      },
      {
        "label" : "TC1",
        "pivot_id" : "ID222222",
        "pivot_type" : "DPCTyp",
        "protocols" : [
          {
            "name": "hnzip",
            "address" : "142",
            "typeid": "TC"
          }
        ]
      },
      {
        "label" : "TVC1",
        "pivot_id" : "ID333333",
        "pivot_type" : "DPCTyp",
        "protocols" : [
          {
            "name": "hnzip",
            "address" : "31",
            "typeid": "TVC"
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
         " ] } , \"application_layer\" : { \"repeat_timeout\" : 3000, \"repeat_path_A\" : 3,"
         "\"remote_station_addr\" : 1, \"max_sarm\" : 5, \"gi_time\" : 1, \"gi_repeat_count\" : 2,"
         "\"anticipation_ratio\" : 5 }, \"south_monitoring\" : { \"asset\" : \"TEST_STATUS\" } } }";
}

class HNZTestComp : public HNZ {
 public:
  HNZTestComp() : HNZ() {}
  void sendCG() {
    sendInitialGI();
  }
};

class HNZTest : public testing::Test {
 public:
  void SetUp() {
    // Create HNZ Plugin object
    hnz = new HNZTestComp();

    hnz->registerIngest(NULL, ingestCallback);

    resetCounters();
  }

  void TearDown() {
    hnz->stop();

    delete hnz;

    std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
    storedReadings = {};
  }

  static void resetCounters() {
    std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
    ingestCallbackCalled = 0;
    dataObjectsReceived = 0;
    southEventsReceived = 0;
  }

  static void initConfig(int port, int port2) {
    static const std::string configureTemplate = QUOTE({
      "enable" : {
        "value": "true"
      },
      "protocol_stack" : {
        "value": <protocol_stack>
      },
      "exchanged_data" : {
        "value": <exchanged_data>
      }
    });
    const std::string& protocol_stack = protocol_stack_generator(port, port2);
    std::string configure = std::regex_replace(configureTemplate, std::regex("<protocol_stack>"), protocol_stack);
    configure = std::regex_replace(configure, std::regex("<exchanged_data>"), exchanged_data_def);
    ConfigCategory config("newConfig", configure);
    hnz->reconfigure(config);
  }

  static void startHNZ(int port, int port2) {
    initConfig(port, port2);
    hnz->start(true);
  }

  template<class... Args>
  static void debug_print(std::string format, Args&&... args) {    
    printf(format.append("\n").c_str(), std::forward<Args>(args)...);
    fflush(stdout);
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

  static std::string readingToJson(const Reading& reading) {
    std::vector<Datapoint*> dataPoints = reading.getReadingData();
    std::string out = "[";
    static const std::string readingTemplate = QUOTE({"<name>":<reading>});
    for (Datapoint* sdp : dataPoints) {
      std::string reading = std::regex_replace(readingTemplate, std::regex("<name>"), sdp->getName());
      reading = std::regex_replace(reading, std::regex("<reading>"), sdp->getData().toString());
      if(out.size() > 1) {
        out += ", ";
      }
      out += reading;
    }
    out += "]";
    return out;
  }

  static void ingestCallback(void* parameter, Reading reading) {
    debug_print("ingestCallback called -> asset: (%s)",
           reading.getAssetName().c_str());
    debug_print(readingToJson(reading));

    std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
    storedReadings.push(std::make_shared<Reading>(reading));

    ingestCallbackCalled++;
    if (hasObject(reading, "data_object"))  {
      dataObjectsReceived++;
    }
    else if (hasObject(reading, "south_event")) {
      southEventsReceived++;
    }
  }

  static std::vector<std::string> split(const std::string &s, char delim) {
    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, delim)) {
      elems.push_back(std::move(item));
    }
    return elems;
  }

  struct ReadingInfo {
    std::string type;
    std::string value;
  };
  static void validateReading(std::shared_ptr<Reading> currentReading, std::string assetName, std::map<std::string, ReadingInfo> attributes) {
    ASSERT_NE(nullptr, currentReading.get()) << assetName << ": Invalid reading";
    ASSERT_EQ(assetName, currentReading->getAssetName());
    // Validate data_object structure received
    ASSERT_TRUE(hasObject(*currentReading, "data_object")) << assetName << ": data_object not found";
    Datapoint* data_object = getObject(*currentReading, "data_object");
    ASSERT_NE(nullptr, data_object) << assetName << ": data_object is null";
    // Validate existance of the required keys and non-existance of the others
    for(const std::string& name: allAttributeNames) {
      bool attributeIsExpected = static_cast<bool>(attributes.count(name));
      ASSERT_EQ(hasChild(*data_object, name), attributeIsExpected) << assetName << ": Attribute " << (attributeIsExpected ? "not found: " : "should not exist: ") << name;
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
      else if(type == std::string("int64_t_range")) {
        auto splitted = split(expectedValue, ';');
        ASSERT_EQ(splitted.size(), 2);
        const std::string& expectedRangeMin = splitted.front();
        const std::string& expectedRangeMax = splitted.back();
        ASSERT_GE(getIntValue(getChild(*data_object, name)), std::stoll(expectedRangeMin)) << assetName << ": Value lower than min for attribute " << name;
        ASSERT_LE(getIntValue(getChild(*data_object, name)), std::stoll(expectedRangeMax)) << assetName << ": Value higher than max for attribute " << name;
      }
      else {
        FAIL() << assetName << ": Unknown type: " << type;
      }
    }
  }

  static void validateSouthEvent(std::shared_ptr<Reading> currentReading, std::string assetName, std::map<std::string, std::string> attributes) {
    ASSERT_NE(nullptr, currentReading.get()) << assetName << ": Invalid south event";
    ASSERT_EQ(assetName, currentReading->getAssetName());
    // Validate data_object structure received
    ASSERT_TRUE(hasObject(*currentReading, "south_event")) << assetName << ": south_event not found";
    Datapoint* data_object = getObject(*currentReading, "south_event");
    ASSERT_NE(nullptr, data_object) << assetName << ": south_event is null";
    // Validate existance of the required keys and non-existance of the others
    for(const std::string& name: allSouthEventAttributeNames) {
      ASSERT_EQ(hasChild(*data_object, name), static_cast<bool>(attributes.count(name))) << assetName << ": Attribute not found: " << name;;
    }
    // Validate value and type of each key
    for(auto const& kvp: attributes) {
      const std::string& name = kvp.first;
      const std::string& expectedValue = kvp.second;
      ASSERT_EQ(expectedValue, getStrValue(getChild(*data_object, name))) << assetName << ": Unexpected value for attribute " << name;
    }
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

  static std::shared_ptr<MSG_TRAME> findProtocolFrameWithId(const std::vector<std::shared_ptr<MSG_TRAME>>& frames, unsigned char frameId) {
    std::shared_ptr<MSG_TRAME> frameFound = nullptr;
    for(auto frame: frames) {
      if((frame->usLgBuffer > 1) && (frame->aubTrame[1] == frameId)) {
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

  static void validateAllTIQualityUpdate(bool invalid, bool outdated, bool noCG = false) {
    // We only expect invalid messages at init, and during init we will also receive 3 extra messages for the failed CG request
    int waitCG = invalid && !noCG;
    int expectedMessages = waitCG ? 10 : 7;
    int maxWaitTimeMs = waitCG ? 3000 : 0; // Max time necessary for initial CG to fail due to timeout (gi_time * (gi_repeat_count+1) * 1000)
    std::string validStr(invalid ? "1" : "0");
    std::string ourdatedStr(outdated ? "1" : "0");
    debug_print("[HNZ Server] Waiting for quality update...");
    waitUntil(dataObjectsReceived, expectedMessages, maxWaitTimeMs);
    ASSERT_EQ(dataObjectsReceived, expectedMessages);
    resetCounters();
    unsigned long epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // First 7 messages are from init
    // Those messages are expected to be sent before the CG time frame
    std::string timeRangeStr(to_string(epochMs - (maxWaitTimeMs + 10000)) + ";" + to_string(epochMs - maxWaitTimeMs));
    std::shared_ptr<Reading> currentReading = nullptr;
    for (int i = 0; i < 4; i++) {
      std::string label("TM" + to_string(i + 1));
      currentReading = popFrontReadingsUntil(label);
      validateReading(currentReading, label, {
        {"do_type", {"string", "TM"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", std::to_string(20 + i)}},
        {"do_valid", {"int64_t", validStr}},
        {"do_an", {"string", "TMA"}},
        {"do_outdated", {"int64_t", ourdatedStr}},
      });
      if(HasFatalFailure()) return;
    }
    for (int i = 0; i < 3; i++) {
      std::string label("TS" + to_string(i + 1));
      currentReading = popFrontReadingsUntil(label);
      validateReading(currentReading, label, {
        {"do_type", {"string", "TS"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", addrByTS[label]}},
        {"do_valid", {"int64_t", validStr}},
        {"do_cg", {"int64_t", "0"}},
        {"do_outdated", {"int64_t", ourdatedStr}},
        {"do_ts", {"int64_t_range", timeRangeStr}},
        {"do_ts_iv", {"int64_t", "0"}},
        {"do_ts_c", {"int64_t", "0"}},
        {"do_ts_s", {"int64_t", "0"}},
      });
      if(HasFatalFailure()) return;
    }
    if (expectedMessages > 7) {
      // Last 3 messages are from failed initial CG
      // Those messages are expected to be sent during the CG time frame
      std::string timeRangeStr2(to_string(epochMs - maxWaitTimeMs) + ";" + to_string(epochMs));
      for (int i = 0; i < 3; i++) {
        std::string label("TS" + to_string(i + 1));
        currentReading = popFrontReadingsUntil(label);
        validateReading(currentReading, label, {
          {"do_type", {"string", "TS"}},
          {"do_station", {"int64_t", "1"}},
          {"do_addr", {"int64_t", addrByTS[label]}},
          {"do_valid", {"int64_t", validStr}},
          {"do_cg", {"int64_t", "0"}},
          {"do_outdated", {"int64_t", ourdatedStr}},
          {"do_ts", {"int64_t_range", timeRangeStr2}},
          {"do_ts_iv", {"int64_t", "0"}},
          {"do_ts_c", {"int64_t", "0"}},
          {"do_ts_s", {"int64_t", "0"}},
        });
        if(HasFatalFailure()) return;
      }
    }
  }

  void validateMissingTSCGQualityUpdate(const std::vector<std::string> labels, bool validateCount = true) {
    if (validateCount) {
      ASSERT_EQ(dataObjectsReceived, labels.size());
      resetCounters();
    }
    unsigned long epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string timeRangeStr(to_string(epochMs - 1000) + ";" + to_string(epochMs));
    std::shared_ptr<Reading> currentReading = nullptr;
    for(const auto& label: labels) {
      currentReading = popFrontReadingsUntil(label);
      validateReading(currentReading, label, {
        {"do_type", {"string", "TS"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", addrByTS[label]}},
        {"do_valid", {"int64_t", "1"}},
        {"do_cg", {"int64_t", "0"}},
        {"do_outdated", {"int64_t", "0"}},
        {"do_ts", {"int64_t_range", timeRangeStr}},
        {"do_ts_iv", {"int64_t", "0"}},
        {"do_ts_c", {"int64_t", "0"}},
        {"do_ts_s", {"int64_t", "0"}},
      });
      if(HasFatalFailure()) return;
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

  static std::shared_ptr<Reading> popFrontReading() {
    std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
    std::shared_ptr<Reading> currentReading = nullptr;
    if (!storedReadings.empty()) {
      currentReading = storedReadings.front();
      storedReadings.pop();
    }
    return currentReading;
  }

  static std::shared_ptr<Reading> popFrontReadingsUntil(std::string label) {
    std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
    std::shared_ptr<Reading> foundReading = nullptr;
    while (!storedReadings.empty()) {
      std::shared_ptr<Reading> currentReading = popFrontReading();
      if (label == currentReading->getAssetName()) {
        foundReading = currentReading;
        break;
      }
    }

    return foundReading;
  }

  static void clearReadings() {
    std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
    if (!storedReadings.empty()) {
      storedReadings.pop();
    }
  }

  static void waitUntil(int& counter, int expectedCount, int timeoutMs) {
    int waitTimeMs = 100;
    int attempts = timeoutMs / waitTimeMs;
    bool expectedCountNotReached = true;
    {
      // Counter is often one of the readings counter, so lock-guard it
      std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
      expectedCountNotReached = counter < expectedCount;
    }
    while (expectedCountNotReached && (attempts > 0)) {
      this_thread::sleep_for(chrono::milliseconds(waitTimeMs));
      attempts--;
      {
        // Counter is often one of the readings counter, so lock-guard it
        std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
        expectedCountNotReached = counter < expectedCount;
      }
    } 
  }

  static HNZTestComp* hnz;
  static int ingestCallbackCalled;
  static int dataObjectsReceived;
  static int southEventsReceived;
  static queue<std::shared_ptr<Reading>> storedReadings;
  static std::recursive_mutex storedReadingsMutex;
  static const std::vector<std::string> allAttributeNames;
  static const std::vector<std::string> allSouthEventAttributeNames;
  static constexpr unsigned long oneHourMs = 3600000; // 60 * 60 * 1000
  static constexpr unsigned long oneDayMs = 86400000; // 24 * 60 * 60 * 1000
  static constexpr unsigned long tenMinMs = 600000;   // 10 * 60 * 1000
  static std::map<std::string, std::string> addrByTS;
};

HNZTestComp* HNZTest::hnz;
int HNZTest::ingestCallbackCalled;
int HNZTest::dataObjectsReceived;
int HNZTest::southEventsReceived;
queue<std::shared_ptr<Reading>> HNZTest::storedReadings;
std::recursive_mutex HNZTest::storedReadingsMutex;
const std::vector<std::string> HNZTest::allAttributeNames = {
  "do_type", "do_station", "do_addr", "do_value", "do_valid", "do_ts", "do_ts_iv", "do_ts_c", "do_ts_s", "do_cg", "do_outdated"
};
const std::vector<std::string> HNZTest::allSouthEventAttributeNames = {"connx_status", "gi_status"};
constexpr unsigned long HNZTest::oneHourMs;
constexpr unsigned long HNZTest::oneDayMs;
constexpr unsigned long HNZTest::tenMinMs;
std::map<std::string, std::string> HNZTest::addrByTS = {{"TS1", "511"}, {"TS2", "522"}, {"TS3", "577"}};

class ServersWrapper {
  public:
    ServersWrapper(int addr, int port1, int port2=0, bool autoStart = true) {
      m_port1 = port1;
      m_port2 = port2;
      m_server1 = std::make_shared<BasicHNZServer>(m_port1, addr);
      m_server1->startHNZServer();

      if(m_port2 > 0) {
        m_server2 = std::make_shared<BasicHNZServer>(m_port2, addr);
        m_server2->startHNZServer();
      }

      // Add a small delay to make sure the servers are ready to accept TCP connections,
      // else a path swap will happen during init and some tests will fail.
      this_thread::sleep_for(chrono::milliseconds(1000));

      if (autoStart) {
        // Start HNZ Plugin
        startHNZPlugin(); 
      }
    }
    void initHNZPlugin() {
      HNZTest::initConfig(m_port1, m_port2);
    }
    void startHNZPlugin() {
      HNZTest::startHNZ(m_port1, m_port2);
    }
    std::shared_ptr<BasicHNZServer> server1() {
      if (m_server1 && !m_server1->HNZServerIsReady()) {
        return nullptr;
      }
      return m_server1;
    }
    std::shared_ptr<BasicHNZServer> server2() {
      if (m_server2 && !m_server2->HNZServerIsReady()) {
        return nullptr;
      }
      return m_server2;
    }
  private:
    std::shared_ptr<BasicHNZServer> m_server1;
    std::shared_ptr<BasicHNZServer> m_server2;
    int m_port1 = 0;
    int m_port2 = 0;
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
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

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
  // Find SET TIME message sent at startup and extract modulo value from it
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TIMEframe = findFrameWithId(frames, 0x1d);
  ASSERT_NE(TIMEframe.get(), nullptr) << "Could not find SET TIME in frames received: " << BasicHNZServer::framesToStr(frames);
  ASSERT_EQ(TIMEframe->usLgBuffer, 9);
  unsigned char startupModulo = TIMEframe->aubTrame[3];
  ASSERT_GE(startupModulo, 0);
  ASSERT_LE(startupModulo, 143);

  dateTime = std::chrono::system_clock::now();
  // Day section is initialized when sending SET TIME message after connection is established
  daySection = startupModulo;
  ts = 14066;
  expectedEpochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  unsigned char msb = static_cast<unsigned char>(ts >> 8);
  unsigned char lsb = static_cast<unsigned char>(ts & 0xFF);
  server->sendFrame({0x0B, 0x33, 0x28, msb, lsb}, false);
  debug_print("[HNZ Server] TSCE sent");
  waitUntil(dataObjectsReceived, 1, 1000);

  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TS1");
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_outdated", {"int64_t", "0"}},
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
  debug_print("[HNZ Server] TSCE 2 sent");
  waitUntil(dataObjectsReceived, 1, 1000);

  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TS1");
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_outdated", {"int64_t", "0"}},
    {"do_ts", {"int64_t", to_string(expectedEpochMs)}},
    {"do_ts_iv", {"int64_t", "0"}},
    {"do_ts_c", {"int64_t", "0"}},
    {"do_ts_s", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TS1 with modified day section
  ///////////////////////////////////////
  daySection = (daySection + 12) % 144;
  expectedEpochMs = HNZ::getEpochMsTimestamp(dateTime, daySection, ts);
  server->sendFrame({0x0F, daySection}, false);
  server->sendFrame({0x0B, 0x33, 0x38, msb, lsb}, false);
  debug_print("[HNZ Server] TSCE 3 sent");
  waitUntil(dataObjectsReceived, 1, 1000);

  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TS1");
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_outdated", {"int64_t", "0"}},
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
  debug_print("[HNZ Server] TSCE 4 sent");
  waitUntil(dataObjectsReceived, 1, 1000);

  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TS1");
  validateReading(currentReading, "TS1", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "511"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "1"}},
    {"do_cg", {"int64_t", "0"}},
    {"do_outdated", {"int64_t", "0"}},
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
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // CG abandonned after gi_repeat_count retries
  ///////////////////////////////////////

  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request sent");
  this_thread::sleep_for(chrono::milliseconds(500)); // must be < gi_time
  int totalCG = 3; // initial CG (1) + gi_repeat_count (2)
  for(int i=0 ; i<totalCG ; i++) {
    // Find the CG frame in the list of frames received by server and validate it
    debug_print("Validating CG frame %d", i);
    validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
    if(HasFatalFailure()) return;
    this_thread::sleep_for(chrono::milliseconds(1000)); // gi_time
  }
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> CGframe = findFrameWithId(frames, 0x13);
  ASSERT_EQ(CGframe.get(), nullptr) << "No CG frame should be sent after gi_repeat_count was reached, but found: " << BasicHNZServer::frameToStr(CGframe);
  // Validate quality update for TS messages that were not sent
  validateMissingTSCGQualityUpdate({"TS1", "TS2", "TS3"});
  if(HasFatalFailure()) return;
  
  ///////////////////////////////////////
  // Send TS1 + TS2 only, then TS3 only as CG answer
  ///////////////////////////////////////
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request 2 sent");
  this_thread::sleep_for(chrono::milliseconds(500)); // must be < gi_time

  // Find the CG frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  // Send only first of the two expected TS
  server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
  debug_print("[HNZ Server] TSCG 1 sent");
  this_thread::sleep_for(chrono::milliseconds(1200)); // gi_time + 200ms

  // Only first of the 2 TS CG messages were sent, it contains data for TS1 and TS2 only
  ASSERT_EQ(dataObjectsReceived, 2);
  resetCounters();
  std::shared_ptr<Reading> currentReading = nullptr;
  for (int i = 0; i < 2; i++) {
    std::string label("TS" + to_string(i + 1));
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TS"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", addrByTS[label]}},
      {"do_value", {"int64_t", "1"}},
      {"do_valid", {"int64_t", "0"}},
      {"do_cg", {"int64_t", "1"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }

  // Extra CG messages should have been sent automatically because some TS are missing and gi_time was reached
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  // Send only second of the two expected TS (new CG was sent so the TS received earlier are ignored)
  server->sendFrame({0x16, 0x39, 0x00, 0x01, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG 2 sent");
  waitUntil(dataObjectsReceived, 3, 1000);

  // Only second of the 2 TS CG messages were sent, it contains data for TS3
  // CG is incomplete, but as last TS was received, it is still considered a finished CG
  // Then quality update were sent for all missing TS (TS1 + TS2)
  ASSERT_EQ(dataObjectsReceived, 3);
  resetCounters();
  currentReading = popFrontReadingsUntil("TS3");
  validateReading(currentReading, "TS3", {
    {"do_type", {"string", "TS"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "577"}},
    {"do_value", {"int64_t", "1"}},
    {"do_valid", {"int64_t", "0"}},
    {"do_cg", {"int64_t", "1"}},
    {"do_outdated", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  // Validate quality update for TS messages that were not sent
  validateMissingTSCGQualityUpdate({"TS1", "TS2"}, false);
  if(HasFatalFailure()) return;

  // As CG is finished, no more CG should be sent automatically any more
  this_thread::sleep_for(chrono::milliseconds(1200)); // gi_time + 200ms
  frames = server->popLastFramesReceived();
  CGframe = findFrameWithId(frames, 0x13);
  ASSERT_EQ(CGframe.get(), nullptr) << "No CG frame should be sent after last TS was received, but found: " << BasicHNZServer::frameToStr(CGframe);

  ///////////////////////////////////////
  // Send TS1 + TS2 + TS3 as CG answer
  ///////////////////////////////////////
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request 3 sent");
  this_thread::sleep_for(chrono::milliseconds(500)); // must not be too close to a multiple of gi_time

  // Find the CG frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  // Send both TS this time (new CG was sent so the TS received earlier are ignored)
  server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x01, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG 3 sent");
  this_thread::sleep_for(chrono::milliseconds(1200)); // gi_time + 200ms

  // All TS were received so no more CG should be sent automatically any more
  frames = server->popLastFramesReceived();
  CGframe = findFrameWithId(frames, 0x13);
  ASSERT_EQ(CGframe.get(), nullptr) << "No CG frame should be sent after all TS were received, but found: " << BasicHNZServer::frameToStr(CGframe);

  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 3);
  resetCounters();
  for (int i = 0; i < 3; i++) {
    std::string label("TS" + to_string(i + 1));
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TS"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", addrByTS[label]}},
      {"do_value", {"int64_t", "1"}},
      {"do_valid", {"int64_t", "0"}},
      {"do_cg", {"int64_t", "1"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }

  ///////////////////////////////////////
  // Send TS1 + TS2 + TS3 as CG answer with invalid flag for TS3
  ///////////////////////////////////////
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request 4 sent");
  this_thread::sleep_for(chrono::milliseconds(500)); // must not be too close to a multiple of gi_time

  // Find the CG frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG 4 sent");
  waitUntil(dataObjectsReceived, 3, 1000);

  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 3);
  resetCounters();
  for (int i = 0; i < 3; i++) {
    std::string label("TS" + to_string(i + 1));
    std::string valid(label == "TS3" ? "1" : "0");
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TS"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", addrByTS[label]}},
      {"do_value", {"int64_t", "0"}},
      {"do_valid", {"int64_t", valid}},
      {"do_cg", {"int64_t", "1"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }

  ///////////////////////////////////////
  // Validate missing TS sent as invalid in case all CG attempts failed
  ///////////////////////////////////////
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request 6 sent");
  this_thread::sleep_for(chrono::milliseconds(500)); // must not be too close to a multiple of gi_time

  // Find the CG frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
  if(HasFatalFailure()) return;

  // Abort the first two of the 3 CG requests by sending the first TS only
  for(int i=0 ; i<2 ; i++) {
    // Send only first of the two expected TS
    server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
    debug_print("[HNZ Server] TSCG %d sent", (5+i));
    this_thread::sleep_for(chrono::milliseconds(1200)); // gi_time + 200ms

    // Check that ingestCallback had been called for TS1 and TS2 only
    ASSERT_EQ(dataObjectsReceived, 2);
    resetCounters();
    for (int j = 0; j < 2; j++) {
      std::string label("TS" + to_string(j + 1));
      currentReading = popFrontReadingsUntil(label);
      validateReading(currentReading, label, {
        {"do_type", {"string", "TS"}},
        {"do_station", {"int64_t", "1"}},
        {"do_addr", {"int64_t", addrByTS[label]}},
        {"do_value", {"int64_t", "1"}},
        {"do_valid", {"int64_t", "0"}},
        {"do_cg", {"int64_t", "1"}},
        {"do_outdated", {"int64_t", "0"}},
      });
      if(HasFatalFailure()) return;
    }

    // Extra CG messages should have been sent automatically because some TS are missing and gi_time was reached
    validateFrame(server->popLastFramesReceived(), {0x13, 0x01});
    if(HasFatalFailure()) return;
  }

  // Send only first of the two expected TS on the final CG attempt
  server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
  debug_print("[HNZ Server] TSCG 7 sent");
  waitUntil(dataObjectsReceived, 3, 1200); // gi_time + 200ms

  // Check that ingestCallback had been called for TS1 and TS2 only
  ASSERT_EQ(dataObjectsReceived, 3);
  resetCounters();
  for (int j = 0; j < 2; j++) {
    std::string label("TS" + to_string(j + 1));
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TS"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", addrByTS[label]}},
      {"do_value", {"int64_t", "1"}},
      {"do_valid", {"int64_t", "0"}},
      {"do_cg", {"int64_t", "1"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }

  // Validate quality update for TS messages that were not sent
  validateMissingTSCGQualityUpdate({"TS3"}, false);
  if(HasFatalFailure()) return;

  // Send a few extra CG requests to trigger the anticipation ratio message
  hnz->sendCG();
  hnz->sendCG();
  hnz->sendCG();
}

TEST_F(HNZTest, ReceivingTMAMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TMA
  ///////////////////////////////////////
  int values[] = {-127, -1, 1, 127};
  unsigned char val0 = static_cast<unsigned char>((-values[0]) ^ 0xFF); // Ones' complement
  unsigned char val1 = static_cast<unsigned char>((-values[1]) ^ 0xFF); // Ones' complement
  unsigned char val2 = static_cast<unsigned char>(values[2]);
  unsigned char val3 = static_cast<unsigned char>(values[3]);
  server->sendFrame({0x02, 0x14, val0, val1, val2, val3}, false);
  debug_print("[HNZ Server] TMA sent");
  waitUntil(dataObjectsReceived, 4, 1000);

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(dataObjectsReceived, 4);
  resetCounters();

  std::shared_ptr<Reading> currentReading = nullptr;
  for (int i = 0; i < 4; i++) {
    std::string label("TM" + to_string(i + 1));
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TM"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", std::to_string(20 + i)}},
      {"do_value", {"int64_t", std::to_string(values[i])}},
      {"do_valid", {"int64_t", "0"}},
      {"do_an", {"string", "TMA"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }

  ///////////////////////////////////////
  // Send TMA with invalid flag for the last one
  ///////////////////////////////////////
  server->sendFrame({0x02, 0x14, val0, val1, val2, 0xFF}, false);
  debug_print("[HNZ Server] TMA 2 sent");
  waitUntil(dataObjectsReceived, 4, 1000);

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(dataObjectsReceived, 4);
  resetCounters();

  for (int i = 0; i < 4; i++) {
    std::string label("TM" + to_string(i + 1));
    std::string value(label == "TM4" ? "0" : std::to_string(values[i]));
    std::string valid(label == "TM4" ? "1" : "0");
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TM"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", std::to_string(20 + i)}},
      {"do_value", {"int64_t", value}},
      {"do_valid", {"int64_t", valid}},
      {"do_an", {"string", "TMA"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }
}

TEST_F(HNZTest, ReceivingTMNMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TMN 8 bits
  ///////////////////////////////////////
  int values[] = {1, 42, 142, 255};
  unsigned char val0 = static_cast<unsigned char>(values[0]);
  unsigned char val1 = static_cast<unsigned char>(values[1]);
  unsigned char val2 = static_cast<unsigned char>(values[2]);
  unsigned char val3 = static_cast<unsigned char>(values[3]);
  server->sendFrame({0x0c, 0x14, val0, val1, val2, val3, 0x80}, false);
  debug_print("[HNZ Server] TM8 sent");
  waitUntil(dataObjectsReceived, 4, 1000);

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(dataObjectsReceived, 4);
  resetCounters();

  std::shared_ptr<Reading> currentReading = nullptr;
  for (int i = 0; i < 4; i++) {
    std::string label("TM" + to_string(i + 1));
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TM"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", std::to_string(20 + i)}},
      {"do_value", {"int64_t", std::to_string(values[i])}},
      {"do_valid", {"int64_t", "0"}},
      {"do_an", {"string", "TM8"}},
      {"do_outdated", {"int64_t", "0"}},
    });
    if(HasFatalFailure()) return;
  }

  ///////////////////////////////////////
  // Send TMN 8 bits with invalid flag for the last one
  ///////////////////////////////////////
  server->sendFrame({0x0c, 0x14, val0, val1, val2, val3, 0x88}, false);
  debug_print("[HNZ Server] TM8 2 sent");
  waitUntil(dataObjectsReceived, 4, 1000);

  // Check that ingestCallback had been called 4x time more
  ASSERT_EQ(dataObjectsReceived, 4);
  resetCounters();

  for (int i = 0; i < 4; i++) {
    std::string label("TM" + to_string(i + 1));
    std::string valid(label == "TM4" ? "1" : "0");
    currentReading = popFrontReadingsUntil(label);
    validateReading(currentReading, label, {
      {"do_type", {"string", "TM"}},
      {"do_station", {"int64_t", "1"}},
      {"do_addr", {"int64_t", std::to_string(20 + i)}},
      {"do_value", {"int64_t", std::to_string(values[i])}},
      {"do_valid", {"int64_t", valid}},
      {"do_an", {"string", "TM8"}},
      {"do_outdated", {"int64_t", "0"}},
    });
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
  debug_print("[HNZ Server] TM16 sent");
  waitUntil(dataObjectsReceived, 2, 1000);

  // Check that ingestCallback had been called 2x time more
  ASSERT_EQ(dataObjectsReceived, 2);
  resetCounters();

  currentReading = popFrontReadingsUntil("TM1");
  validateReading(currentReading, "TM1", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "20"}},
    {"do_value", {"int64_t", std::to_string(val11)}},
    {"do_valid", {"int64_t", "0"}},
    {"do_an", {"string", "TM16"}},
    {"do_outdated", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TM3");
  validateReading(currentReading, "TM3", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "22"}},
    {"do_value", {"int64_t", std::to_string(val12)}},
    {"do_valid", {"int64_t", "0"}},
    {"do_an", {"string", "TM16"}},
    {"do_outdated", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TMN 16 bits with invalid flag for the last one
  ///////////////////////////////////////
  server->sendFrame({0x0c, 0x14, lsb1, msb1, lsb2, msb2, 0x04}, false);
  debug_print("[HNZ Server] TM16 2 sent");
  waitUntil(dataObjectsReceived, 2, 1000);

  // Check that ingestCallback had been called 2x time more
  ASSERT_EQ(dataObjectsReceived, 2);
  resetCounters();

  currentReading = popFrontReadingsUntil("TM1");
  validateReading(currentReading, "TM1", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "20"}},
    {"do_value", {"int64_t", std::to_string(val11)}},
    {"do_valid", {"int64_t", "0"}},
    {"do_an", {"string", "TM16"}},
    {"do_outdated", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TM3");
  validateReading(currentReading, "TM3", {
    {"do_type", {"string", "TM"}},
    {"do_station", {"int64_t", "1"}},
    {"do_addr", {"int64_t", "22"}},
    {"do_value", {"int64_t", std::to_string(val12)}},
    {"do_valid", {"int64_t", "1"}},
    {"do_an", {"string", "TM16"}},
    {"do_outdated", {"int64_t", "0"}},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, SendingTCMessages) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TC1
  ///////////////////////////////////////
  std::string operationTC("HNZCommand");
  int nbParamsTC = 3;
  PLUGIN_PARAMETER paramTC1 = {"co_type", "TC"};
  PLUGIN_PARAMETER paramTC2 = {"co_addr", "142"};
  PLUGIN_PARAMETER paramTC3 = {"co_value", "1"};
  PLUGIN_PARAMETER* paramsTC[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3};
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  debug_print("[HNZ south plugin] TC sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server
  server->sendFrame({0x09, 0x0e, 0x49}, false);
  debug_print("[HNZ Server] TC ACK sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TC1");
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "TC"}},
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
  debug_print("[HNZ south plugin] TC 2 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 011b
  server->sendFrame({0x09, 0x0e, 0x4b}, false);
  debug_print("[HNZ Server] TC ACK 2 sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TC1");
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "TC"}},
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
  debug_print("[HNZ south plugin] TC 3 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 101b
  server->sendFrame({0x09, 0x0e, 0x4d}, false);
  debug_print("[HNZ Server] TC ACK 3 sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TC1");
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "TC"}},
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
  debug_print("[HNZ south plugin] TC 4 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 111b
  server->sendFrame({0x09, 0x0e, 0x4f}, false);
  debug_print("[HNZ Server] TC ACK 4 sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TC1");
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "TC"}},
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
  debug_print("[HNZ south plugin] TC 5 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x19, 0x0e, 0x48});
  if(HasFatalFailure()) return;

  // Send TC ACK from server with CR bit = 010b
  server->sendFrame({0x09, 0x0e, 0x4a}, false);
  debug_print("[HNZ Server] TC ACK 5 sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TC1");
  validateReading(currentReading, "TC1", {
    {"do_type", {"string", "TC"}},
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
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  ///////////////////////////////////////
  // Send TVC1
  ///////////////////////////////////////
  std::string operationTVC("HNZCommand");
  int nbParamsTVC = 3;
  PLUGIN_PARAMETER paramTVC1 = {"co_type", "TVC"};
  PLUGIN_PARAMETER paramTVC2 = {"co_addr", "31"};
  PLUGIN_PARAMETER paramTVC3 = {"co_value", "42"};
  PLUGIN_PARAMETER* paramsTVC[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3};
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC));
  debug_print("[HNZ south plugin] TVC sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x1a, 0x1f, 0x2a, 0x00});
  if(HasFatalFailure()) return;

  // Send TVC ACK from server
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x00}, false);
  debug_print("[HNZ Server] TVC ACK sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TVC1");
  validateReading(currentReading, "TVC1", {
    {"do_type", {"string", "TVC"}},
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
  debug_print("[HNZ south plugin] TVC 2 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x1a, 0x1f, 0x2a, 0x80});
  if(HasFatalFailure()) return;

  // Send TVC ACK from server
  server->sendFrame({0x0a, 0x9f, 0x2a, 0x80}, false);
  debug_print("[HNZ Server] TVC 2 ACK sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TVC1");
  validateReading(currentReading, "TVC1", {
    {"do_type", {"string", "TVC"}},
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
  debug_print("[HNZ south plugin] TVC 3 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TVC frame in the list of frames received by server and validate it
  validateFrame(server->popLastFramesReceived(), {0x1a, 0x1f, 0x2a, 0x80});
  if(HasFatalFailure()) return;

  // Send TVC ACK from server with A bit = 1
  server->sendFrame({0x0a, 0xdf, 0x2a, 0x80}, false);
  debug_print("[HNZ Server] TVC 3 ACK sent");
  waitUntil(dataObjectsReceived, 1, 1000);
  // Check that ingestCallback had been called
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
  currentReading = popFrontReadingsUntil("TVC1");
  validateReading(currentReading, "TVC1", {
    {"do_type", {"string", "TVC"}},
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
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  server->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  server2->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  debug_print("[HNZ Server] TSCE sent on both path");
  this_thread::sleep_for(chrono::milliseconds(3000));

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(dataObjectsReceived, 1);

  // Send a SARM to put hnz plugin on path A in connection state
  // and don't send UA then to switch on path B
  debug_print("[HNZ Server] Send SARM on Path A to force switch to Path B");
  server->sendSARM();

  // Wait 30s
  this_thread::sleep_for(chrono::milliseconds(30000));
  resetCounters();

  server2->sendFrame({0x0B, 0x33, 0x28, 0x36, 0xF2}, false);
  debug_print("[HNZ Server] TSCE sent on path B");
  this_thread::sleep_for(chrono::milliseconds(3000));

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(dataObjectsReceived, 1);
}

TEST_F(HNZTest, SendingMessagesTwoPath) {
  ServersWrapper wrapper(0x05, getNextPort(), getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  BasicHNZServer* server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  // Send TC1
  std::string operationTC("HNZCommand");
  int nbParamsTC = 3;
  PLUGIN_PARAMETER paramTC1 = {"co_type", "TC"};
  PLUGIN_PARAMETER paramTC2 = {"co_addr", "142"};
  PLUGIN_PARAMETER paramTC3 = {"co_value", "1"};
  PLUGIN_PARAMETER* paramsTC[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3};
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  debug_print("[HNZ south plugin] TC sent");
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
  debug_print("[HNZ Server] TC ACK sent on both path");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();

  // Send a SARM to put hnz plugin on path A in connection state
  // and don't send UA then to switch on path B
  debug_print("[HNZ Server] Send SARM on Path A to force switch to Path B");
  server->sendSARM();

  // Wait 30s
  this_thread::sleep_for(chrono::seconds(30));
  resetCounters();

  // Send TC1 on path B
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  debug_print("[HNZ south plugin] TC 2 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));

  // Find the TC frame in the list of frames received by server2
  frames = server2->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_NE(TCframe.get(), nullptr) << "Could not find TC in frames received: " << BasicHNZServer::framesToStr(frames);
  // Check that TC is only received on active path (server2) and not on passive path (server)
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "No TC frame should be received by server, found: " << BasicHNZServer::frameToStr(TCframe);

  // Send TC ACK from server2 only
  server2->sendFrame({0x09, 0x0e, 0x49}, false);
  debug_print("[HNZ Server] TC ACK sent on path B");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(dataObjectsReceived, 1);
  resetCounters();
}

TEST_F(HNZTest, ConnectionLossAndGIStatus) {
  // Create server but do not start connection to HNZ device
  ServersWrapper wrapper(0x05, getNextPort(), 0, false);
  // Initialize configuration only (mandatory for operation processing)
  wrapper.initHNZPlugin();
  // Send request_connection_status
  std::string operationRCS("request_connection_status");
  ASSERT_TRUE(hnz->operation(operationRCS, 0, nullptr));
  debug_print("[HNZ south plugin] request_connection_status sent");

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate initial states
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
    {"gi_status", "idle"},
  });
  if(HasFatalFailure()) return;

  // Wait for connection to be initialized
  wrapper.startHNZPlugin();
  debug_print("[HNZ south plugin] waiting for connection established...");
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  // Also wait for initial CG request to expire (gi_time * (gi_repeat_count+1) * 1000)
  waitUntil(southEventsReceived, 3, 3000);
  // Check that ingestCallback had been called the expected number of times
  ASSERT_EQ(southEventsReceived, 3);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "failed"},
  });
  if(HasFatalFailure()) return;

  // Send new CG request
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request sent");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;

  // Send one TS CG
  server->sendFrame({0x16, 0x33, 0x10, 0x00, 0x04, 0x00}, false);
  debug_print("[HNZ Server] TSCG 1 sent");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "in progress"},
  });
  if(HasFatalFailure()) return;

  // Wait for all CG attempts to expire (gi_time * (gi_repeat_count + initial CG + 1) * 1000)
  debug_print("[HNZ south plugin] waiting for full CG timeout...");
  waitUntil(southEventsReceived, 1, 4000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "failed"},
  });
  if(HasFatalFailure()) return;

  // Send new CG request
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request 2 sent");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;

  // Complete CG request by sending all expected TS
  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG 2 sent");
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "in progress"},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "finished"},
  });
  if(HasFatalFailure()) return;

  // Disconnect server
  ASSERT_TRUE(server->stopHNZServer());
  debug_print("[HNZ Server] Server disconnected");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
  });
  if(HasFatalFailure()) return;

  // Wait 1 min (to make sure we can reconnect even after a long time)
  debug_print("[HNZ south plugin] Wait before reconnect...");
  this_thread::sleep_for(chrono::minutes(1));
  
  // Reconnect server
  server->startHNZServer();
  // Wait for connection to be initialized
  debug_print("[HNZ south plugin] waiting for connection 2 established...");
  server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection 2 is not established in 10s...";
  // Also wait for initial CG request to expire (gi_time * (gi_repeat_count+1) * 1000)
  waitUntil(southEventsReceived, 3, 3000);
  // Check that ingestCallback had been called the expected number of times
  ASSERT_EQ(southEventsReceived, 3);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "failed"},
  });
  if(HasFatalFailure()) return;

  // Validate that frames can be exchanged on the newly opened connection
  // Send new CG request
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request 3 sent");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;

  // Complete CG request by sending all expected TS
  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG 3 sent");
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "in progress"},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "finished"},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, ConnectionLossTwoPath) {
  // Create server but do not start connection to HNZ device
  ServersWrapper wrapper(0x05, getNextPort(), getNextPort(), false);
  // Initialize configuration only (mandatory for operation processing)
  wrapper.initHNZPlugin();
  // Send request_connection_status
  std::string operationRCS("request_connection_status");
  ASSERT_TRUE(hnz->operation(operationRCS, 0, nullptr));
  debug_print("[HNZ south plugin] request_connection_status sent");

  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate initial states
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
    {"gi_status", "idle"},
  });
  if(HasFatalFailure()) return;

  // Wait for connection to be initialized
  wrapper.startHNZPlugin();
  debug_print("[HNZ south plugin] waiting for connection established...");
  BasicHNZServer* server = wrapper.server1().get();
  BasicHNZServer* server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection is not established in 10s...";
  // Also wait for initial CG request to expire (gi_time * (gi_repeat_count+1) * 1000)
  waitUntil(southEventsReceived, 3, 3000);
  // Check that ingestCallback had been called the expected number of times
  ASSERT_EQ(southEventsReceived, 3);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "failed"},
  });
  if(HasFatalFailure()) return;

  // Disconnect server 1
  ASSERT_TRUE(server->stopHNZServer());
  debug_print("[HNZ Server] Server 1 disconnected");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that ingestCallback had not been called (second path is still connected)
  ASSERT_EQ(southEventsReceived, 0);
  // Disconnect server 2
  ASSERT_TRUE(server2->stopHNZServer());
  debug_print("[HNZ Server] Server 2 disconnected");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
  });
  if(HasFatalFailure()) return;

  // Reconnect server
  server->startHNZServer();
  server2->startHNZServer();
  // Wait for connection to be initialized
  debug_print("[HNZ south plugin] waiting for connection 2 established...");
  server = wrapper.server1().get();
  server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection 2 is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection 2 is not established in 10s...";
  // Also wait for initial CG request to expire (gi_time * (gi_repeat_count+1) * 1000)
  waitUntil(southEventsReceived, 3, 3000);
  // Check that ingestCallback had been called the expected number of times
  ASSERT_EQ(southEventsReceived, 3);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "failed"},
  });
  if(HasFatalFailure()) return;

  // Disconnect server 1 and 2 simultaneously
  ASSERT_TRUE(server->stopHNZServer());
  ASSERT_TRUE(server2->stopHNZServer());
  debug_print("[HNZ Server] Server 1 & 2 disconnected");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
  });
  if(HasFatalFailure()) return;

  // Reconnect server
  server->startHNZServer();
  server2->startHNZServer();
  // Wait for connection to be initialized
  debug_print("[HNZ south plugin] waiting for connection 3 established...");
  server = wrapper.server1().get();
  server2 = wrapper.server2().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection 3 is not established in 10s...";
  ASSERT_NE(server2, nullptr) << "Something went wrong. Connection 3 is not established in 10s...";
  // Also wait for initial CG request to expire (gi_time * (gi_repeat_count+1) * 1000)
  waitUntil(southEventsReceived, 3, 3000);
  // Check that ingestCallback had been called the expected number of times
  ASSERT_EQ(southEventsReceived, 3);
  resetCounters();
  // Validate new connection state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "failed"},
  });
  if(HasFatalFailure()) return;

  // Validate that frames can be exchanged on the newly opened connection
  // Send new CG request
  hnz->sendCG();
  debug_print("[HNZ south plugin] CG request sent");
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  resetCounters();
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;

  // Complete CG request by sending all expected TS
  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG sent");
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "in progress"},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "finished"},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, ReconfigureWhileConnectionActive) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  // This calls HNZ::reconfigure() again, causing a reconnect of the client
  debug_print("[HNZ south plugin] Reconfigure plugin");
  clearReadings();
  wrapper.initHNZPlugin();

  // Check that connection was lost
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  // Validate new connection state
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
  });
  if(HasFatalFailure()) return;

  // Check that connection attempt to reopen on client side
  validateAllTIQualityUpdate(true, false, true);
  if(HasFatalFailure()) return;

  // Also stop the server as it is unable to reconnect on the fly
  debug_print("[HNZ server] Request server stop...");
  ASSERT_TRUE(server->stopHNZServer());
  debug_print("[HNZ south plugin] Waiting for outdated TI emission...");
  this_thread::sleep_for(chrono::milliseconds(1000));
  validateAllTIQualityUpdate(false, true);
  debug_print("[HNZ server] Request server start...");
  server->startHNZServer();

  // Check that the server is reconnected after reconfigure
  server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection 2 is not established in 10s...";
  // Wait for initial CG request
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  // Validate reconnection
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;

  // Complete CG request by sending all expected TS
  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG sent");
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "in progress"},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "finished"},
  });
  if(HasFatalFailure()) return;
}

TEST_F(HNZTest, ReconfigureBadConfig) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  debug_print("[HNZ south plugin] Send bad plugin configuration");
  clearReadings();
  static const std::string badConfig = QUOTE({
    "exchanged_data" : {
      "value": 42
    }
  });
  ConfigCategory config("newConfig", badConfig);
  hnz->reconfigure(config);

  // Check that connection was lost
  waitUntil(southEventsReceived, 1, 1000);
  // Check that ingestCallback had been called only one time
  ASSERT_EQ(southEventsReceived, 1);
  // Validate new connection state
  std::shared_ptr<Reading> currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "not connected"},
  });
  if(HasFatalFailure()) return;

  // No quality update message as new config contains no TI
  ASSERT_EQ(dataObjectsReceived, 0);

  // Also stop the server as it is unable to reconnect on the fly
  debug_print("[HNZ server] Request server stop...");
  ASSERT_TRUE(server->stopHNZServer());
  debug_print("[HNZ server] Request server start...");
  server->startHNZServer();

  // Check that connection cannot be established as client is no longer running due to invalid configuration
  BasicHNZServer* deadServer = wrapper.server1().get();
  ASSERT_EQ(deadServer, nullptr) << "Something went wrong. Server should not be able to reconnect, but it did!";

  // This calls HNZ::reconfigure() again, causing a reconnect of the client
  debug_print("[HNZ south plugin] Send good plugin configuration");
  clearReadings();
  wrapper.initHNZPlugin();

  // Check that connection attempt to reopen on client side
  validateAllTIQualityUpdate(true, false, true);
  if(HasFatalFailure()) return;

  // Restart the server
  debug_print("[HNZ server] Request server stop 2...");
  ASSERT_TRUE(server->stopHNZServer());
  debug_print("[HNZ server] Request server start 2...");
  server->startHNZServer();

  // Check that the server is reconnected after reconfigure
  server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection 2 is not established in 10s...";
  // Wait for initial CG request
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  // Validate reconnection
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"connx_status", "started"},
  });
  if(HasFatalFailure()) return;
  // Validate new GI state
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "started"},
  });
  if(HasFatalFailure()) return;

  // Complete CG request by sending all expected TS
  server->sendFrame({0x16, 0x33, 0x00, 0x00, 0x00, 0x00}, false);
  server->sendFrame({0x16, 0x39, 0x00, 0x02, 0x00, 0x00}, false);
  debug_print("[HNZ Server] TSCG sent");
  waitUntil(southEventsReceived, 2, 1000);
  // Check that ingestCallback had been called only for two GI status updates
  ASSERT_EQ(southEventsReceived, 2);
  resetCounters();
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "in progress"},
  });
  if(HasFatalFailure()) return;
  currentReading = popFrontReadingsUntil("TEST_STATUS");
  validateSouthEvent(currentReading, "TEST_STATUS", {
    {"gi_status", "finished"},
  });
  if(HasFatalFailure()) return;

  // Manually stop the server here or we may end up in a deadlock in the HNZServer
  debug_print("[HNZ server] Request server stop 3...");
  ASSERT_TRUE(server->stopHNZServer());
}

TEST_F(HNZTest, UnknownMessage) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  // Send an unknown message
  server->sendFrame({0x00}, false);
  debug_print("[HNZ Server] Unknown message sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TSCE
  server->sendFrame({0x0B, 0xff, 0x28, 0x00, 0x00}, false);
  debug_print("[HNZ Server] Unknown TSCE sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TSCG
  server->sendFrame({0x16, 0xff, 0x10, 0x00, 0x04, 0x00}, false);
  debug_print("[HNZ Server] Unknown TSCG sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TMA
  server->sendFrame({0x02, 0xff, 0x00, 0x00, 0x00, 0x00}, false);
  debug_print("[HNZ Server] Unknown TMA sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TM8
  server->sendFrame({0x0c, 0xff, 0x00, 0x00, 0x00, 0x00, 0x80}, false);
  debug_print("[HNZ Server] Unknown TM8 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TM16
  server->sendFrame({0x0c, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00}, false);
  debug_print("[HNZ Server] Unknown TM16 sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TC ACK
  server->sendFrame({0x09, 0x00, 0x49}, false);
  debug_print("[HNZ Server] Unknown TC ACK sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);

  // Send an unknown TVC ACK
  server->sendFrame({0x0a, 0x00, 0x2a, 0x00}, false);
  debug_print("[HNZ Server] Unknown TVC ACK sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  // Check that no message was received
  ASSERT_EQ(ingestCallbackCalled, 0);
}

TEST_F(HNZTest, InvalidOperations) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  // Send invalid operation type
  ASSERT_FALSE(hnz->operation("INVALID", 0, nullptr));
  debug_print("[HNZ south plugin] Invalid operation sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send invalid command type
  std::string operationTC("HNZCommand");
  int nbParamsTC = 3;
  PLUGIN_PARAMETER paramTC1 = {"co_type", "TC"};
  PLUGIN_PARAMETER paramTC2 = {"co_addr", "142"};
  PLUGIN_PARAMETER paramTC3 = {"co_value", "1"};
  PLUGIN_PARAMETER paramTC1_bad = {"co_type", "test"};
  PLUGIN_PARAMETER paramTC2_bad = {"co_addr", "test"};
  PLUGIN_PARAMETER paramTC2_bad2 = {"co_addr", "9999999999"};
  PLUGIN_PARAMETER paramTC3_bad = {"co_value", "test"};
  PLUGIN_PARAMETER paramTC3_bad2 = {"co_value", "9999999999"};
  PLUGIN_PARAMETER paramTC4_bad = {"test", "result"};
  PLUGIN_PARAMETER* paramsTC[nbParamsTC] = {&paramTC1_bad, &paramTC2, &paramTC3};
  ASSERT_FALSE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  debug_print("[HNZ south plugin] Invalid command sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TC with extra parameter
  nbParamsTC = 4;
  PLUGIN_PARAMETER* paramsTC2[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3, &paramTC4_bad};
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC2));
  debug_print("[HNZ south plugin] TC with extra param sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_NE(TCframe.get(), nullptr) << "Could not find TC in frames received: " << BasicHNZServer::framesToStr(frames);

  // Send TC with missing parameter
  nbParamsTC = 2;
  PLUGIN_PARAMETER* paramsTC3[nbParamsTC] = {&paramTC1, &paramTC2};
  ASSERT_FALSE(hnz->operation(operationTC, nbParamsTC, paramsTC3));
  debug_print("[HNZ south plugin] TC with missing param sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TC with invalid address
  nbParamsTC = 3;
  PLUGIN_PARAMETER* paramsTC4[nbParamsTC] = {&paramTC1, &paramTC2_bad, &paramTC3};
  ASSERT_FALSE(hnz->operation(operationTC, nbParamsTC, paramsTC4));
  debug_print("[HNZ south plugin] TC with invalid address sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TC with address out of bounds
  PLUGIN_PARAMETER* paramsTC5[nbParamsTC] = {&paramTC1, &paramTC2_bad2, &paramTC3};
  ASSERT_FALSE(hnz->operation(operationTC, nbParamsTC, paramsTC5));
  debug_print("[HNZ south plugin] TC with address out of bounds sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TC with invalid value
  PLUGIN_PARAMETER* paramsTC6[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3_bad};
  ASSERT_FALSE(hnz->operation(operationTC, nbParamsTC, paramsTC6));
  debug_print("[HNZ south plugin] TC with invalid value sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TC with value out of bounds
  PLUGIN_PARAMETER* paramsTC7[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3_bad2};
  ASSERT_FALSE(hnz->operation(operationTC, nbParamsTC, paramsTC7));
  debug_print("[HNZ south plugin] TC with value out of bounds sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TCframe = findFrameWithId(frames, 0x19);
  ASSERT_EQ(TCframe.get(), nullptr) << "Found unexpected TC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TVC with extra parameter
  std::string operationTVC("HNZCommand");
  int nbParamsTVC = 4;
  PLUGIN_PARAMETER paramTVC1 = {"co_type", "TVC"};
  PLUGIN_PARAMETER paramTVC2 = {"co_addr", "31"};
  PLUGIN_PARAMETER paramTVC3 = {"co_value", "42"};
  PLUGIN_PARAMETER paramTVC1_bad = {"co_type", "test"};
  PLUGIN_PARAMETER paramTVC2_bad = {"co_addr", "test"};
  PLUGIN_PARAMETER paramTVC2_bad2 = {"co_addr", "9999999999"};
  PLUGIN_PARAMETER paramTVC3_bad = {"co_value", "test"};
  PLUGIN_PARAMETER paramTVC3_bad2 = {"co_value", "9999999999"};
  PLUGIN_PARAMETER paramTVC4_bad = {"test", "result"};
  PLUGIN_PARAMETER* paramsTVC2[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3, &paramTVC4_bad};
  ASSERT_TRUE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC2));
  debug_print("[HNZ south plugin] TVC with extra param sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_NE(TVCframe.get(), nullptr) << "Could not find TVC in frames received: " << BasicHNZServer::framesToStr(frames);

  // Send TVC with missing parameter
  nbParamsTVC = 2;
  PLUGIN_PARAMETER* paramsTVC3[nbParamsTVC] = {&paramTVC1, &paramTVC2};
  ASSERT_FALSE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC3));
  debug_print("[HNZ south plugin] TVC with missing param sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_EQ(TVCframe.get(), nullptr) << "Found unexpected TVC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TVC with invalid address
  nbParamsTVC = 3;
  PLUGIN_PARAMETER* paramsTVC4[nbParamsTVC] = {&paramTVC1, &paramTVC2_bad, &paramTVC3};
  ASSERT_FALSE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC4));
  debug_print("[HNZ south plugin] TVC with invalid address sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_EQ(TVCframe.get(), nullptr) << "Found unexpected TVC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TVC with address out of bounds
  PLUGIN_PARAMETER* paramsTVC5[nbParamsTVC] = {&paramTVC1, &paramTVC2_bad2, &paramTVC3};
  ASSERT_FALSE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC5));
  debug_print("[HNZ south plugin] TVC with address out of bounds sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_EQ(TVCframe.get(), nullptr) << "Found unexpected TVC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TVC with invalid value
  PLUGIN_PARAMETER* paramsTVC6[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3_bad};
  ASSERT_FALSE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC6));
  debug_print("[HNZ south plugin] TVC with invalid value sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_EQ(TVCframe.get(), nullptr) << "Found unexpected TVC frame: " << BasicHNZServer::framesToStr(frames);

  // Send TVC with value out of bounds
  PLUGIN_PARAMETER* paramsTVC7[nbParamsTVC] = {&paramTVC1, &paramTVC2, &paramTVC3_bad2};
  ASSERT_FALSE(hnz->operation(operationTVC, nbParamsTVC, paramsTVC7));
  debug_print("[HNZ south plugin] TVC with value out of bounds sent");
  this_thread::sleep_for(chrono::milliseconds(1000));
  frames = server->popLastFramesReceived();
  TVCframe = findFrameWithId(frames, 0x1a);
  ASSERT_EQ(TVCframe.get(), nullptr) << "Found unexpected TVC frame: " << BasicHNZServer::framesToStr(frames);
}

TEST_F(HNZTest, FrameToStr) {
  ASSERT_STREQ(hnz->frameToStr({}).c_str(), "\n[]");
  ASSERT_STREQ(hnz->frameToStr({0x42}).c_str(), "\n[0x42]");
  ASSERT_STREQ(hnz->frameToStr({0x00, 0xab, 0xcd, 0xff}).c_str(), "\n[0x00, 0xab, 0xcd, 0xff]");
}

TEST_F(HNZTest, BackToSARM) {
  ServersWrapper wrapper(0x05, getNextPort());
  BasicHNZServer* server = wrapper.server1().get();
  ASSERT_NE(server, nullptr) << "Something went wrong. Connection is not established in 10s...";
  validateAllTIQualityUpdate(true, false);
  if(HasFatalFailure()) return;

  // Stop sending automatic ack in response to messages from south plugin
  server->disableAcks(true);

  // Send 2 TCs so that south plugin has something to send to HNZ server
  // and we have more than one message waiting in the list
  std::string operationTC("HNZCommand");
  int nbParamsTC = 3;
  PLUGIN_PARAMETER paramTC1 = {"co_type", "TC"};
  PLUGIN_PARAMETER paramTC2 = {"co_addr", "142"};
  PLUGIN_PARAMETER paramTC3 = {"co_value", "1"};
  PLUGIN_PARAMETER* paramsTC[nbParamsTC] = {&paramTC1, &paramTC2, &paramTC3};
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  ASSERT_TRUE(hnz->operation(operationTC, nbParamsTC, paramsTC));
  debug_print("[HNZ south plugin] TCs sent");

  // Clear messages received from south plugin
  server->popLastFramesReceived();
  // Wait (repeat_timeout * repeat_path_A) + 1 = (3 * 3) + 1 = 10s
  this_thread::sleep_for(chrono::seconds(10));
  
  // Find the SARM frame in the list of frames received by server
  std::vector<std::shared_ptr<MSG_TRAME>> frames = server->popLastFramesReceived();
  std::shared_ptr<MSG_TRAME> SARMframe = findProtocolFrameWithId(frames, 0x0f);
  ASSERT_NE(SARMframe.get(), nullptr) << "Could not find SARM in frames received: " << BasicHNZServer::framesToStr(frames);
}
