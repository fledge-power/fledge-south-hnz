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

constexpr char JSON_CONF_NAME[] = "protocol_stack";
constexpr char NAME[] = "name";
constexpr char JSON_VERSION[] = "version";
constexpr char TRANSPORT_LAYER[] = "transport_layer";
constexpr char CONNECTIONS[] = "connections";
constexpr char IP_ADDR[] = "srv_ip";
constexpr char IP_PORT[] = "port";
constexpr char APP_LAYER[] = "application_layer";
constexpr char REMOTE_ADDR[] = "remote_station_addr";
constexpr char INACC_TIMEOUT[] = "inacc_timeout";
constexpr char MAX_SARM[] = "max_sarm";
constexpr char REPEAT_PATH_A[] = "repeat_path_A";
constexpr char REPEAT_PATH_B[] = "repeat_path_B";
constexpr char REPEAT_TIMEOUT[] = "repeat_timeout";
constexpr char ANTICIPATION_RATIO[] = "anticipation_ratio";
constexpr char TST_MSG_SEND[] = "test_msg_send";
constexpr char TST_MSG_RECEIVE[] = "test_msg_receive";
constexpr char GI_SCHEDULE[] = "gi_schedule";
constexpr char GI_REPEAT_COUNT[] = "gi_repeat_count";
constexpr char GI_TIME[] = "gi_time";
constexpr char C_ACK_TIME[] = "c_ack_time";
constexpr char CMD_RECV_TIMEOUT[] = "cmd_recv_timeout";

constexpr char JSON_EXCHANGED_DATA_NAME[] = "exchanged_data";
constexpr char DATAPOINTS[] = "datapoints";
constexpr char LABEL[] = "label";
constexpr char PIVOT_ID[] = "pivot_id";
constexpr char PIVOT_TYPE[] = "pivot_type";
constexpr char PROTOCOLS[] = "protocols";
constexpr char HNZ_NAME[] = "hnzip";
constexpr char MESSAGE_CODE[] = "typeid";
constexpr char MESSAGE_ADDRESS[] = "address";

// Default value
constexpr unsigned int DEFAULT_PORT = 6001;
constexpr unsigned int DEFAULT_INACC_TIMEOUT = 180;
constexpr unsigned int DEFAULT_MAX_SARM = 30;
constexpr unsigned int DEFAULT_REPEAT_PATH = 3;
constexpr unsigned int DEFAULT_REPEAT_TIMEOUT = 3000;
constexpr unsigned int DEFAULT_ANTICIPATION_RATIO = 3;
constexpr char DEFAULT_TST_MSG[] = "1304";
constexpr char DEFAULT_GI_SCHEDULE[] = "99:99";
constexpr unsigned int DEFAULT_GI_REPEAT_COUNT = 3;
constexpr unsigned int DEFAULT_GI_TIME = 255;
constexpr unsigned int DEFAULT_C_ACK_TIME = 10;
constexpr long long int DEFAULT_CMD_RECV_TIMEOUT = 100000; // 100ms

constexpr char DEBUG_LEVEL[] = "debug";

constexpr int RETRY_CONN_DELAY = 5;
constexpr int RETRY_CONN_NUM = 5;

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
  bool is_complete() const {
    return m_config_is_complete && m_exchange_data_is_complete;
  }

  /**
   * Get the label related to a message. If this message is not defined in the
   * configuration, then the returned label is empty.
   */
  string getLabel(const string &msg_code, const int msg_address) const;

  /**
   * Get the number of CG. Used for the consistency check when GI.
   */
  unsigned long getNumberCG() const;

  /**
   * Get the IP address to remote IEC 104 server (A path)
   *
   * @return string
   */
  string get_ip_address_A() const { return m_ip_A; }

  /**
   * Get the port number to remote IEC 104 server (A path)
   *
   * @return unsigned int
   */
  unsigned int get_port_A() const { return m_port_A; }

  /**
   * Get the IP address to remote IEC 104 server (B path)
   *
   * @return string
   */
  string get_ip_address_B() const { return m_ip_B; }

  /**
   * Get the port number to remote IEC 104 server (B path)
   *
   * @return unsigned int
   */
  unsigned int get_port_B() const { return m_port_B; }

  /**
   * Get the remote server station address
   *
   * @return unsigned int
   */
  unsigned int get_remote_station_addr() const { return m_remote_station_addr; }

  /**
   * Get the timeout before declaring the remote server unreachable
   *
   * @return unsigned int
   */
  unsigned int get_inacc_timeout() const { return m_inacc_timeout; }

  /**
   * Get the max number of SARM messages before handing over to the passive path
   * (A/B)
   *
   * @return unsigned int
   */
  unsigned int get_max_sarm() const { return m_max_sarm; }

  /**
   * Get the max number of authorized repeats for path A
   *
   * @return unsigned int
   */
  unsigned int get_repeat_path_A() const { return m_repeat_path_A; }

  /**
   * Get the max number of authorized repeats for path B
   *
   * @return unsigned int
   */
  unsigned int get_repeat_path_B() const { return m_repeat_path_B; }

  /**
   * Get the time allowed for the receiver to acknowledge a frame, after this
   * time, the sender repeats the frame.
   *
   * @return unsigned int
   */
  unsigned int get_repeat_timeout() const { return m_repeat_timeout; }

  /**
   * Get the number of frames allowed to be received without acknowledgement.
   *
   * @return unsigned int
   */
  unsigned int get_anticipation_ratio() const { return m_anticipation_ratio; }

  /**
   * Get the test message code in sending direction.
   *
   * @return unsigned int
   */
  BulleFormat get_test_msg_send() const { return m_test_msg_send; }

  /**
   * Get the test message code in receiving direction.
   *
   * @return unsigned int
   */
  BulleFormat get_test_msg_receive() const { return m_test_msg_receive; }

  /**
   * Get the scheduled time for General Interrogation sending.
   *
   * @return unsigned int
   */
  GIScheduleFormat get_gi_schedule() const { return m_gi_schedule; }

  /**
   * Get the repeat GI for this number of times in case it is incomplete.
   *
   * @return unsigned int
   */
  unsigned int get_gi_repeat_count() const { return m_gi_repeat_count; }

  /**
   * Get the time to wait for General Interrogation (GI) completion.
   *
   * @return unsigned int
   */
  unsigned int get_gi_time() const { return m_gi_time; }

  /**
   * Get the time to wait before receving a acknowledgement for a control
   * command.
   *
   * @return unsigned int
   */
  unsigned int get_c_ack_time() const { return m_c_ack_time; }

  /**
   * Get the timeout for socket recv blocking calls
   *
   * @return  unsigned int
   */
  long long int get_cmd_recv_timeout() const { return m_cmd_recv_timeout; }

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

  static bool m_retrieve(const Value &json, const char *key, unsigned int *target);
  static bool m_retrieve(const Value &json, const char *key, unsigned int *target, unsigned int def);
  static bool m_retrieve(const Value &json, const char *key, string *target);
  static bool m_retrieve(const Value &json, const char *key, string *target, string def);
  static bool m_retrieve(const Value &json, const char *key, BulleFormat *target);
  static bool m_retrieve(const Value &json, const char *key, GIScheduleFormat *target);
  static bool m_retrieve(const Value &json, const char *key, long long int *target, long long int def);
};

#endif