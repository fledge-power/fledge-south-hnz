#include <gtest/gtest.h>
#include <plugin_api.h>
#include <rapidjson/document.h>
#include <string.h>
#include "../include/hnzconf.h" 
#include "../include/hnzconnection.h" 
#include <string>

TEST(HNZConnection,ConfNotComplete) 
{
    HNZConf* conf = new HNZConf();
    HNZClient* hnz_client = new HNZClient();
    HNZ* hnz = new HNZ();
    HNZConnection* hnz_connection = new HNZConnection(conf,hnz_client,hnz);
    
}