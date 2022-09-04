/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#ifndef HNZConf_H
#define HNZConf_H

#include <map>
#include <sstream>

#include "logger.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#define JSON_CONF_NAME "protocol_stack"
#define NAME "name"
#define JSON_VERSION "version"
#define TRANSPORT_LAYER "transport_layer"
#define CONNECTIONS "connections"
#define IP_ADDR "srv_ip"
#define IP_PORT "port"
#define APP_LAYER "application_layer"
#define REMOTE_ADDR "remote_station_addr"
#define INACC_TIMEOUT "inacc_timeout"
#define MAX_SARM "max_sarm"
#define REPEAT_PATH_A "repeat_path_A"
#define REPEAT_PATH_B "repeat_path_B"
#define REPEAT_TIMEOUT "repeat_timeout"
#define ANTICIPATION_RATIO "anticipation_ratio"
#define TST_MSG_SEND "test_msg_send"
#define TST_MSG_RECEIVE "test_msg_receive"
#define GI_SCHEDULE "gi_schedule"
#define GI_REPEAT_COUNT "gi_repeat_count"
#define GI_TIME "gi_time"
#define C_ACK_TIME "c_ack_time"

#define JSON_EXCHANGED_DATA_NAME "exchanged_data"
#define DATAPOINTS "datapoints"
#define LABEL "label"
#define PIVOT_ID "pivot_id"
#define PIVOT_TYPE "pivot_type"
#define PROTOCOLS "protocols"
#define HNZ_NAME "hnz"
#define MESSAGE_CODE "message_code"
#define STATION_ADDRESS "station_address"
#define MESSAGE_ADDRESS "message_address"

// Default value
#define DEFAULT_PORT 6001
#define DEFAULT_INACC_TIMEOUT 180
#define DEFAULT_MAX_SARM 30
#define DEFAULT_REPEAT_PATH 3
#define DEFAULT_REPEAT_TIMEOUT 3000
#define DEFAULT_ANTICIPATION_RATIO 3
#define DEFAULT_TST_MSG "1304"
#define DEFAULT_GI_SCHEDULE "99:99"
#define DEFAULT_GI_REPEAT_COUNT 3
#define DEFAULT_GI_TIME 255
#define DEFAULT_C_ACK_TIME 10

#define DEBUG_LEVEL "debug"

#define RETRY_CONN_DELAY 5
#define RETRY_CONN_NUM 5

using namespace rapidjson;
using namespace std;

struct BulleFormat {
  unsigned char first;
  unsigned char second;
} typedef BulleFormat;

struct GIScheduleFormat {
  bool activate;
  int hour;
  int min;
} typedef GIScheduleFormat;

class HNZConf {
 public:
  HNZConf();
  HNZConf(const string &json_config, const string &json_exchanged_data);
  ~HNZConf();

  /**
   * Import the HNZ Protocol stack configuration JSON. If errors are detected
   * they are reported in the fledge logger.
   */
  void importConfigJson(const string &json);

  /**
   * Import the Exchanged data configuration JSON. If errors are detected
   * they are reported in the fledge logger.
   */
  void importExchangedDataJson(const string &json);

  /**
   * Allows you to know if the configuration was successful.
   */
  bool is_complete() {
    return m_config_is_complete && m_exchange_data_is_complete;
  }

  /**
   * Get the label related to a message. If this message is not defined in the
   * configuration, then the returned label is empty.
   */
  string getLabel(const string &msg_code, const int msg_address);

  /**
   * Get the number of CG. Used for the consistency check when GI.
   */
  int getNumberCG();

  string get_ip_address() { return m_ip; }
  unsigned int get_port() { return m_port; }
  unsigned int get_remote_station_addr() { return m_remote_station_addr; }
  unsigned int get_inacc_timeout() { return m_inacc_timeout; }
  unsigned int get_max_sarm() { return m_max_sarm; }
  unsigned int get_repeat_path_A() { return m_repeat_path_A; }
  unsigned int get_repeat_path_B() { return m_repeat_path_B; }
  unsigned int get_repeat_timeout() { return m_repeat_timeout; }
  unsigned int get_anticipation_ratio() { return m_anticipation_ratio; }
  unsigned int get_default_msg_period() { return m_default_msg_period; }
  BulleFormat get_test_msg_send() { return m_test_msg_send; }
  BulleFormat get_test_msg_receive() { return m_test_msg_receive; }
  GIScheduleFormat get_gi_schedule() { return m_gi_schedule; }
  unsigned int get_gi_repeat_count() { return m_gi_repeat_count; }
  unsigned int get_gi_time() { return m_gi_time; }
  unsigned int get_c_ack_time() { return m_c_ack_time; }

 private:
  string m_ip;
  unsigned int m_port;
  unsigned int m_remote_station_addr;
  unsigned int m_inacc_timeout;
  unsigned int m_max_sarm;
  unsigned int m_repeat_path_A;
  unsigned int m_repeat_path_B;
  unsigned int m_repeat_timeout;
  unsigned int m_anticipation_ratio;
  unsigned int m_default_msg_period;
  BulleFormat m_test_msg_send;
  BulleFormat m_test_msg_receive;
  GIScheduleFormat m_gi_schedule;
  unsigned int m_gi_repeat_count;
  unsigned int m_gi_time;
  unsigned int m_c_ack_time;

  map<string, map<unsigned int, map<unsigned int, string>>> m_msg_list;

  bool m_config_is_complete;
  bool m_exchange_data_is_complete;

  static bool m_check_string(const Value &json, const char *key);
  static bool m_check_array(const Value &json, const char *key);
  static bool m_check_object(const Value &json, const char *key);

  static bool m_retrieve(const Value &json, const char *key,
                         unsigned int *target);
  static bool m_retrieve(const Value &json, const char *key,
                         unsigned int *target, unsigned int def);
  static bool m_retrieve(const Value &json, const char *key, string *target);
  static bool m_retrieve(const Value &json, const char *key, string *target,
                         string def);
  static bool m_retrieve(const Value &json, const char *key,
                         BulleFormat *target);
  static bool m_retrieve(const Value &json, const char *key,
                         GIScheduleFormat *target);
};

#endif