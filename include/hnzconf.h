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
#define CMD_RECV_TIMEOUT "cmd_recv_timeout"

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
#define DEFAULT_CMD_RECV_TIMEOUT 100000 // 100ms

#define DEBUG_LEVEL "debug"

#define RETRY_CONN_DELAY 5
#define RETRY_CONN_NUM 5

using namespace rapidjson;
using namespace std;

/**
 * @brief Structure containing the 2 bytes of a BULLE
 */
struct BulleFormat {
  unsigned char first;
  unsigned char second;
} typedef BulleFormat;

/**
 * @brief Structure containing the time for General Interrogation
 */
struct GIScheduleFormat {
  /// indicate if GI is enable
  bool activate;
  int hour;
  int min;
} typedef GIScheduleFormat;

/**
 * @brief Class used to manage the HNZ configuration.
 */
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

  /**
   * Get the IP address to remote IEC 104 server (A path)
   *
   * @return string
   */
  string get_ip_address_A() { return m_ip_A; }

  /**
   * Get the port number to remote IEC 104 server (A path)
   *
   * @return unsigned int
   */
  unsigned int get_port_A() { return m_port_A; }

  /**
   * Get the IP address to remote IEC 104 server (B path)
   *
   * @return string
   */
  string get_ip_address_B() { return m_ip_B; }

  /**
   * Get the port number to remote IEC 104 server (B path)
   *
   * @return unsigned int
   */
  unsigned int get_port_B() { return m_port_B; }

  /**
   * Get the remote server station address
   *
   * @return unsigned int
   */
  unsigned int get_remote_station_addr() { return m_remote_station_addr; }

  /**
   * Get the timeout before declaring the remote server unreachable
   *
   * @return unsigned int
   */
  unsigned int get_inacc_timeout() { return m_inacc_timeout; }

  /**
   * Get the max number of SARM messages before handing over to the passive path
   * (A/B)
   *
   * @return unsigned int
   */
  unsigned int get_max_sarm() { return m_max_sarm; }

  /**
   * Get the max number of authorized repeats for path A
   *
   * @return unsigned int
   */
  unsigned int get_repeat_path_A() { return m_repeat_path_A; }

  /**
   * Get the max number of authorized repeats for path B
   *
   * @return unsigned int
   */
  unsigned int get_repeat_path_B() { return m_repeat_path_B; }

  /**
   * Get the time allowed for the receiver to acknowledge a frame, after this
   * time, the sender repeats the frame.
   *
   * @return unsigned int
   */
  unsigned int get_repeat_timeout() { return m_repeat_timeout; }

  /**
   * Get the number of frames allowed to be received without acknowledgement.
   *
   * @return unsigned int
   */
  unsigned int get_anticipation_ratio() { return m_anticipation_ratio; }

  /**
   * Get the test message code in sending direction.
   *
   * @return unsigned int
   */
  BulleFormat get_test_msg_send() { return m_test_msg_send; }

  /**
   * Get the test message code in receiving direction.
   *
   * @return unsigned int
   */
  BulleFormat get_test_msg_receive() { return m_test_msg_receive; }

  /**
   * Get the scheduled time for General Interrogation sending.
   *
   * @return unsigned int
   */
  GIScheduleFormat get_gi_schedule() { return m_gi_schedule; }

  /**
   * Get the repeat GI for this number of times in case it is incomplete.
   *
   * @return unsigned int
   */
  unsigned int get_gi_repeat_count() { return m_gi_repeat_count; }

  /**
   * Get the time to wait for General Interrogation (GI) completion.
   *
   * @return unsigned int
   */
  unsigned int get_gi_time() { return m_gi_time; }

  /**
   * Get the time to wait before receving a acknowledgement for a control
   * command.
   *
   * @return unsigned int
   */
  unsigned int get_c_ack_time() { return m_c_ack_time; }

  /**
   * Get the timeout for socket recv blocking calls
   *
   * @return  unsigned int
   */
  long long int get_cmd_recv_timeout() { return m_cmd_recv_timeout; }

 private:
  string m_ip_A, m_ip_B = "";
  unsigned int m_port_A, m_port_B;
  unsigned int m_remote_station_addr;
  unsigned int m_inacc_timeout;
  unsigned int m_max_sarm;
  unsigned int m_repeat_path_A;
  unsigned int m_repeat_path_B;
  unsigned int m_repeat_timeout;
  unsigned int m_anticipation_ratio;
  BulleFormat m_test_msg_send;
  BulleFormat m_test_msg_receive;
  GIScheduleFormat m_gi_schedule;
  unsigned int m_gi_repeat_count;
  unsigned int m_gi_time;
  unsigned int m_c_ack_time;
  long long int m_cmd_recv_timeout;

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
  static bool m_retrieve(const Value &json, const char *key,
                         long long int *target, long long int def);
};

#endif