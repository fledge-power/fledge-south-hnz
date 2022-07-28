#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hnz.h>
#include <plugin_api.h>
#include <string.h>

#include <iostream>
#include <string>

using namespace std;
using namespace nlohmann;
#define PROTOCOL_STACK_DEF

//protocol stack 

typedef struct
{
    string protocol_stack = QUOTE({       \
    "protocol_stack":{                    \
    "name":"hnzclient",                   \
    "version":"1.0",                      \
    "transport_layer":{                   \
    "connections":                        \
            {                             \
               "srv_ip":"192.168.1.3",    \
               "port":6001                \
            },                            \
         "llevel":1,                      \
         "retry_number":5,                \
         "retry_delay":5                  \
      },                                  \
      "application_layer":{               \
         "remote_station_addr":12,        \
         "local_station_addr":12,         \
         "remote_addr_in_local_station":0,\
         "inacc_timeout":180,             \
         "max_sarm":30,                   \
         "to_socket":1,                   \
         "repeat_path_A":3,               \
         "repeat_path_B":3,               \
         "repeat_timeout":3000,           \
         "anticipation":3,                \
         "default_msg_period":0,          \
         "Test_msg_send":"1304",          \
         "Test_msg_receive":"1304"        \
      }                                   \
   }                                      \
    });

    string exchanged_data = QUOTE({
        "exchanged_data" : {               \
        "name" : "hnzclient",               \
        "version" : "1.0",                  \
        "msg_list" : [                      \
            {                               \
                "station_address" : 1,      \
                "message_code" : "0B",      \
                "label" : "7AT761_AL.ECHAU",\
                "info_address" : 511,       \
                "internal_id" : "ID001836"  \
            },                              \
            {                               \
                "station_address" : 1,      \
                "message_code" : "0B",      \
                "label" : "7AT761_AL.TEMP", \
                "info_address" : 324,       \
                "internal_id" : "ID001026"  \
            }                               \
        ]                                   \
    }                                       \
    });

    string protocol_translation = QUOTE({
    "protocol_translation" : {                   \
        "name" : "hnz_to_pivot",                 \
        "version" : "1.0",                       \
        "mapping" : {                            \
                "data_object_header" : {         \
                    "doh_type" : "message_code", \
                    "doh_name" : "internal_id"   \
                },                               \
                "data_object_item" : {           \
                    "doi_value" : "value",       \
                    "doi_quality" : "quality",   \
                    "doi_ts" : "timestamp",      \
                    "doi_ts_qual" : "ts_qual"    \
                }                                \
            }                                    \
    }                                            \
    });



}json_config;


TEST(HNZ,PluginStackParametersTestsNoThrow){
    HNZ hnz;
    json_config config;
    hnz.setJsonConfig(config.protocol_stack, config.exchanged_data, config.protocol_translation);
    hnz.PrepareParameters();
    testing::GTEST_FLAG(repeat) = 1;
    testing::GTEST_FLAG(shuffle) = true;
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    ASSERT_EQ(hnz.m_llevel, 1);
    ASSERT_EQ(hnz.m_retry_delay, 5);
    ASSERT_EQ(hnz.m_retry_number, 5);
    ASSERT_EQ(hnz.m_remote_station_addr, 12);
    ASSERT_EQ(hnz.m_local_station_addr, 12);
    ASSERT_EQ(hnz.m_remote_addr_in_local_station, 0);
    ASSERT_EQ(hnz.m_inacc_timeout, 180);
    ASSERT_EQ(hnz.m_max_sarm, 30);
    ASSERT_EQ(hnz.m_to_socket, 1);
    ASSERT_EQ(hnz.m_repeat_path_A, 3);
    ASSERT_EQ(hnz.m_repeat_path_B, 3);
    ASSERT_EQ(hnz.m_repeat_timeout, 3000);
    ASSERT_EQ(hnz.m_anticipation, 3);
    ASSERT_EQ(hnz.m_default_msg_period, 0);
    ASSERT_EQ(hnz.m_Test_msg_send, "1304");
    ASSERT_EQ(hnz.m_Test_msg_receive, "1304");
}
