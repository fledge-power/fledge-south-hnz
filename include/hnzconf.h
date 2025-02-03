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
#include <memory>
#include <unordered_set>
#include "rapidjson/document.h"

// Local definition of make_unique as it is only available since C++14 and right now fledge-south-hnz is built with C++11
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

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
constexpr char SOUTH_MONITORING[] = "south_monitoring";
constexpr char SOUTH_MONITORING_ASSET[] = "asset";

constexpr char JSON_EXCHANGED_DATA_NAME[] = "exchanged_data";
constexpr char DATAPOINTS[] = "datapoints";
constexpr char LABEL[] = "label";
constexpr char PIVOT_ID[] = "pivot_id";
constexpr char PIVOT_TYPE[] = "pivot_type";
constexpr char PIVOT_SUBTYPES[] = "pivot_subtypes";
constexpr char TRIGGER_SOUTH_GI_PIVOT_SUBTYPE[] = "trigger_south_gi";
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

constexpr int RETRY_CONN_DELAY = 5;

using namespace rapidjson;
using namespace std;

/**
 * @brief Structure containing the 2 bytes of a BULLE
 */
struct BulleFormat {
  unsigned char first = 0;
  unsigned char second = 0;
};

/**
 * @brief Structure containing the time for General Interrogation
 */
struct GIScheduleFormat {
  /// indicate if GI is enable
  bool activate = false;
  int hour = 0;
  int min = 0;
};

/**
 * @brief Class used to manage the HNZ configuration.
 */
class HNZConf {
 public:
  HNZConf() = default;
  HNZConf(const string &json_config, const string &json_exchanged_data);

  /**
   * Import the HNZ Protocol stack configuration JSON. If errors are detected
   * they are reported in the fledge logger.
   * @param json json string containing the protocol_stack to import
   */
  void importConfigJson(const string &json);

  /**
   * Import the Exchanged data configuration JSON. If errors are detected
   * they are reported in the fledge logger.
   * @param json json string containing the exchanged_data to import
   */
  void importExchangedDataJson(const string &json);

  /**
   * Allows you to know if the configuration was successful.
   * @return True if the configuration contains all required information, else false
   */
  bool is_complete() const {
    return m_config_is_complete && m_exchange_data_is_complete;
  }

  /**
   * Get the label related to a message.
   * @param msg_code message code to search in the configuration
   * @param json message address to search in the configuration
   * @return Label for the given message code ans address, or empty string if not found
   */
  string getLabel(const string &msg_code, const int msg_address) const;

  /**
   * Get the number of CG. Used for the consistency check when GI.
   * @return Number of TS expected to be received when doing a CG.
   */
  unsigned long getNumberCG() const;

  /**
   * Get the address of the last TS expected when doing a CG.
   * This adress is the one with the highest value among all addresses configured
   * because, during a CG, TS are always sent in ascending order of their addresses.
   * Used to avoid waiting for full timeout when somme TS are missing during a CG.
   * @return Address of the last TSCG.
   */
  unsigned int getLastTSAddress() const;

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

  /**
   * Get the "asset" name for south plugin monitoring event
   *
   * @return  string
   */
  std::string get_connx_status_signal() const { return m_connx_status; }

  /**
   * Get the list of all messages loaded from exchange_data
   *
   * @return Nested map of messages defined in configuration
   */
  const map<string, map<unsigned int, map<unsigned int, string>>>& get_all_messages() const { return m_msg_list; }

  /**
   * Check if an address is in the CG triggering TS address set
   *
   * @return True if the given address is present in the address set
   */
  bool isTsAddressCgTriggering(int address) { return m_cgTriggeringTsAdresses.find(address) != m_cgTriggeringTsAdresses.end(); }

 private:
  /**
   * Import data from a json value representing the transport_layer
   * 
   * @param transport json configuration object
   * @return True if the import was successful, else false
   */
  bool m_importTransportLayer(const Value &transport);

  /**
   * Import data from a json value representing the application_layer
   * 
   * @param conf json configuration object
   * @return True if the import was successful, else false
   */
  bool m_importApplicationLayer(const Value &conf);

  /**
   * Import data from a json value representing an exchanged_data datapoint
   * 
   * @param msg json configuration object
   * @return True if the import was successful, else false
   */
  bool m_importDatapoint(const Value &msg);

  string m_ip_A, m_ip_B = "";
  unsigned int m_port_A = 0;
  unsigned int m_port_B = 0;
  unsigned int m_remote_station_addr = 0;
  unsigned int m_inacc_timeout = 0;
  unsigned int m_max_sarm = 0;
  unsigned int m_repeat_path_A = 0;
  unsigned int m_repeat_path_B = 0;
  unsigned int m_repeat_timeout = 0;
  unsigned int m_anticipation_ratio = 0;
  BulleFormat m_test_msg_send;
  BulleFormat m_test_msg_receive;
  GIScheduleFormat m_gi_schedule;
  unsigned int m_gi_repeat_count = 0;
  unsigned int m_gi_time = 0;
  unsigned int m_c_ack_time = 0;
  long long int m_cmd_recv_timeout = 0;

  std::string m_connx_status = "";
  unsigned int m_lastTSAddr = 0;
  // Nested map of msg_code, remote_station_addr and msg_address
  map<string, map<unsigned int, map<unsigned int, string>>> m_msg_list;

  // Set of TS addresses that triggers a CG if the TS value is 0
  std::unordered_set<int> m_cgTriggeringTsAdresses;

  bool m_config_is_complete = false;
  bool m_exchange_data_is_complete = false;

  static bool m_check_string(const Value &json, const char *key);
  static bool m_check_array(const Value &json, const char *key);
  static bool m_check_object(const Value &json, const char *key);

  static bool m_retrieve(const Value &json, const char *key, unsigned int *target);
  static bool m_retrieve(const Value &json, const char *key, unsigned int *target, unsigned int def);
  static bool m_retrieve(const Value &json, const char *key, string *target);
  static bool m_retrieve(const Value &json, const char *key, string *target, const string& def);
  static bool m_retrieve(const Value &json, const char *key, BulleFormat *target);
  static bool m_retrieve(const Value &json, const char *key, GIScheduleFormat *target);
  static bool m_retrieve(const Value &json, const char *key, long long int *target, long long int def);
};

#endif