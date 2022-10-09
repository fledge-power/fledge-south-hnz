// #include <gtest/gtest.h>
// #include <plugin_api.h>
// #include <rapidjson/document.h>
// #include <string.h>
// #include "../include/hnzconf.h" 
// #include <string>

// using namespace std;
// using namespace rapidjson;


// TEST(HNZCONF, EmptyConf)
// {
//     HNZConf* conf = new HNZConf();
//     EXPECT_FALSE(conf->is_complete()); 
// }

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

// TEST(HNZCONF,ConfComplete) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     EXPECT_TRUE(conf->is_complete());
    
// }

// TEST(HNZCONF,GetIPAdress) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_ip_address());
    
// }

// TEST(HNZCONF,GetPort) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_port());

// }

// TEST(HNZCONF,GetRemoteStationAddr) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_remote_station_addr());
    
// }

// TEST(HNZCONF,GetInaccTimeout) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_inacc_timeout());

// }

// TEST(HNZCONF,GetMaxSARM) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_max_sarm());
// }


// TEST(HNZCONF,GetRepeatPathAB) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_repeat_path_A());
//     ASSERT_EQ(conf->get_repeat_path_B());


// }

// TEST(HNZCONF,GetRepeatTimeout) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_repeat_timeout());

// }

// TEST(HNZCONF,GetAnticipationRatio) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_anticipation_ratio());

// }

// TEST(HNZCONF,GetDefaultMsgPeriod) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_default_msg_period());

// }

// TEST(HNZCONF,GetTestMsgSend) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_test_msg_send());
// }

// TEST(HNZCONF,GetTestMsgReceive) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_test_msg_receive());


// }

// TEST(HNZCONF,GetGISchedule) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_gi_schedule());

// }


// TEST(HNZCONF,GetGIRepeatCount) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_gi_repeat_count());


// }

// TEST(HNZCONF,GetGITime) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_gi_time());

// }

// TEST(HNZCONF,GetCAckTime) 
// {
//     HNZConf* conf = new HNZConf();
//     conf->importConfigJson();
//     conf->importExchangedDataJson();
//     ASSERT_EQ(conf->get_c_ack_time());

// }