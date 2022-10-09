#include <config_category.h>
#include <gtest/gtest.h>
#include <plugin_api.h>
#include <string.h>

#include "../include/hnz.h"
// pour l'instant
#include <boost/thread.hpp>
#include <chrono>
#include <utility>
#include <vector>

#include "/home/lucas/dev/git/libhnzlocal/src/inc/hnz_server.h"

using namespace std;
// using namespace nlohmann;

#define TEST_PORT 6001

class HNZTestComp : public HNZ {
 public:
  HNZTestComp() : HNZ() {}
};

class HNZTest : public testing::Test {
 protected:
  struct sTestInfo {
    int callbackCalled;
    Reading* storedReading;
  };

  // Per-test-suite set-up.
  // Called before the first test in this test suite.
  // Can be omitted if not needed.
  static void SetUpTestSuite() {
    // Avoid reallocating static objects if called in subclasses of FooTest.
    if (hnz == nullptr) {
      hnz = new HNZTestComp();
      hnz->registerIngest(NULL, ingestCallback);
    }
  }

  // Per-test-suite tear-down.
  // Called after the last test in this test suite.
  // Can be omitted if not needed.
  static void TearDownTestSuite() { hnz->stop(); }

  static void startHNZ() { hnz->start(); }

  static bool hasChild(Datapoint& dp, std::string childLabel) {
    DatapointValue& dpv = dp.getData();

    auto dps = dpv.getDpVec();

    for (auto sdp : *dps) {
      if (sdp->getName() == childLabel) {
        return true;
      }
    }

    return false;
  }

  static Datapoint* getChild(Datapoint& dp, std::string childLabel) {
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

  static std::string getStrValue(Datapoint* dp) {
    return dp->getData().toStringValue();
  }

  static bool hasObject(Reading& reading, std::string label) {
    std::vector<Datapoint*> dataPoints = reading.getReadingData();

    for (Datapoint* dp : dataPoints) {
      if (dp->getName() == label) {
        return true;
      }
    }

    return false;
  }

  static Datapoint* getObject(Reading& reading, std::string label) {
    std::vector<Datapoint*> dataPoints = reading.getReadingData();

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

    std::vector<Datapoint*> dataPoints = reading.getReadingData();

    printf("  number of readings: %lu\n", dataPoints.size());

    // for (Datapoint* sdp : dataPoints) {
    //     printf("name: %s value: %s\n", sdp->getName().c_str(),
    //     sdp->getData().toString().c_str());
    // }
    storedReading = new Reading(reading);

    ingestCallbackCalled++;
    printf("My value is");
    printf("Mu value is%u", ingestCallbackCalled);
  }

  static boost::thread thread_;
  static HNZTestComp* hnz;
  static int ingestCallbackCalled;
  static Reading* storedReading;
};

boost::thread HNZTest::thread_;
HNZTestComp* HNZTest::hnz;
int HNZTest::ingestCallbackCalled;
Reading* HNZTest::storedReading;

TEST_F(HNZTest, ReceivingMessage) {
  startHNZ();
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  ASSERT_EQ(ingestCallbackCalled, 1);

  // CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC,
  // 0, 41025, false, false);

  // struct sCP56Time2a ts;

  // uint64_t timestamp = Hal_getTimeInMs();

  // CP56Time2a_createFromMsTimestamp(&ts, timestamp);

  // InformationObject io = (InformationObject)
  // SinglePointWithCP56Time2a_create(NULL, 4206948, true,
  // IEC60870_QUALITY_GOOD, &ts);

  // CS101_ASDU_addInformationObject(newAsdu, io);

  // InformationObject_destroy(io);

  // /* Add ASDU to slave event queue */
  // CS104_Slave_enqueueASDU(slave, newAsdu);

  // CS101_ASDU_destroy(newAsdu);

  // Thread_sleep(500);

  // ASSERT_EQ(ingestCallbackCalled, 1);
  // ASSERT_EQ("TS-1", storedReading->getAssetName());
  // ASSERT_TRUE(hasObject(*storedReading, "data_object"));
  // Datapoint* data_object = getObject(*storedReading, "data_object");
  // ASSERT_NE(nullptr, data_object);
  // ASSERT_TRUE(hasChild(*data_object, "do_type"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ca"));
  // ASSERT_TRUE(hasChild(*data_object, "do_oa"));
  // ASSERT_TRUE(hasChild(*data_object, "do_cot"));
  // ASSERT_TRUE(hasChild(*data_object, "do_test"));
  // ASSERT_TRUE(hasChild(*data_object, "do_negative"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ioa"));
  // ASSERT_TRUE(hasChild(*data_object, "do_value"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_iv"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_bl"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_sb"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_nt"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ts"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ts_iv"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ts_su"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ts_sub"));

  // ASSERT_EQ("M_SP_TB_1", getStrValue(getChild(*data_object, "do_type")));
  // ASSERT_EQ((int64_t) 41025, getIntValue(getChild(*data_object, "do_ca")));
  // ASSERT_EQ((int64_t) 1, getIntValue(getChild(*data_object, "do_cot")));
  // ASSERT_EQ((int64_t) 4206948, getIntValue(getChild(*data_object,
  // "do_ioa"))); ASSERT_EQ((int64_t) timestamp,
  // getIntValue(getChild(*data_object, "do_ts")));

  // delete storedReading;

  // CS101_ASDU newAsdu2 = CS101_ASDU_create(alParams, false,
  // CS101_COT_INTERROGATED_BY_STATION, 0, 41025, false, false);

  // io = (InformationObject) SinglePointInformation_create(NULL, 4206948, true,
  // IEC60870_QUALITY_GOOD);

  // CS101_ASDU_addInformationObject(newAsdu2, io);

  // InformationObject_destroy(io);

  // /* Add ASDU to slave event queue */
  // CS104_Slave_enqueueASDU(slave, newAsdu2);

  // CS101_ASDU_destroy(newAsdu2);

  // Thread_sleep(500);

  // ASSERT_EQ(ingestCallbackCalled, 2);
  // ASSERT_EQ("TS-1", storedReading->getAssetName());
  // ASSERT_TRUE(hasObject(*storedReading, "data_object"));
  // data_object = getObject(*storedReading, "data_object");
  // ASSERT_NE(nullptr, data_object);
  // ASSERT_TRUE(hasChild(*data_object, "do_type"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ca"));
  // ASSERT_TRUE(hasChild(*data_object, "do_oa"));
  // ASSERT_TRUE(hasChild(*data_object, "do_cot"));
  // ASSERT_TRUE(hasChild(*data_object, "do_test"));
  // ASSERT_TRUE(hasChild(*data_object, "do_negative"));
  // ASSERT_TRUE(hasChild(*data_object, "do_ioa"));
  // ASSERT_TRUE(hasChild(*data_object, "do_value"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_iv"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_bl"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_sb"));
  // ASSERT_TRUE(hasChild(*data_object, "do_quality_nt"));
  // ASSERT_FALSE(hasChild(*data_object, "do_ts"));
  // ASSERT_FALSE(hasChild(*data_object, "do_ts_iv"));
  // ASSERT_FALSE(hasChild(*data_object, "do_ts_su"));
  // ASSERT_FALSE(hasChild(*data_object, "do_ts_sub"));

  // ASSERT_EQ("M_SP_NA_1", getStrValue(getChild(*data_object, "do_type")));
  // ASSERT_EQ((int64_t) 41025, getIntValue(getChild(*data_object, "do_ca")));
  // ASSERT_EQ((int64_t) 20, getIntValue(getChild(*data_object, "do_cot")));
  // ASSERT_EQ((int64_t) 4206948, getIntValue(getChild(*data_object,
  // "do_ioa"))); ASSERT_EQ(0, getIntValue(getChild(*data_object,
  // "do_quality_iv"))); ASSERT_EQ(0, getIntValue(getChild(*data_object,
  // "do_quality_bl"))); ASSERT_EQ(0, getIntValue(getChild(*data_object,
  // "do_quality_sb"))); ASSERT_EQ(0, getIntValue(getChild(*data_object,
  // "do_quality_nt")));

  // delete storedReading;

  // CS101_ASDU newAsdu3 = CS101_ASDU_create(alParams, false,
  // CS101_COT_INTERROGATED_BY_STATION, 0, 41025, false, false);

  // io = (InformationObject) SinglePointInformation_create(NULL, 4206948, true,
  // IEC60870_QUALITY_INVALID | IEC60870_QUALITY_NON_TOPICAL);

  // CS101_ASDU_addInformationObject(newAsdu3, io);

  // InformationObject_destroy(io);

  // /* Add ASDU to slave event queue */
  // CS104_Slave_enqueueASDU(slave, newAsdu3);

  // CS101_ASDU_destroy(newAsdu3);

  // Thread_sleep(500);

  // ASSERT_EQ(ingestCallbackCalled, 3);
  // data_object = getObject(*storedReading, "data_object");
  // ASSERT_EQ(1, getIntValue(getChild(*data_object, "do_quality_iv")));
  // ASSERT_EQ(0, getIntValue(getChild(*data_object, "do_quality_bl")));
  // ASSERT_EQ(0, getIntValue(getChild(*data_object, "do_quality_sb")));
  // ASSERT_EQ(1, getIntValue(getChild(*data_object, "do_quality_nt")));

  // delete storedReading;

  // CS104_Slave_stop(slave);

  // CS104_Slave_destroy(slave);
}
