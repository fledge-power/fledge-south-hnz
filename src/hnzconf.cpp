/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#include "hnzutility.h"
#include "hnzconf.h"
#include "rapidjson/error/en.h"

HNZConf::HNZConf(const string &json_config, const string &json_exchanged_data) {
  importConfigJson(json_config);
  importExchangedDataJson(json_exchanged_data);
}

void HNZConf::importConfigJson(const string &json) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::importConfigJson - ";
  m_config_is_complete = false;

  bool is_complete = true;

  Document document;

  if (document.Parse(const_cast<char *>(json.c_str())).HasParseError()) {
    HnzUtility::log_fatal(beforeLog + "Parsing error in protocol_stack json, offset " +
                               to_string((unsigned)document.GetErrorOffset()) +
                               " " +
                               GetParseError_En(document.GetParseError()));
    return;
  }
  if (!document.IsObject()) return;

  if (!m_check_object(document, JSON_CONF_NAME)) return;

  const Value &info = document[JSON_CONF_NAME];

  is_complete &=
      m_check_string(info, NAME) && m_check_string(info, JSON_VERSION);

  if (m_check_object(info, TRANSPORT_LAYER)) {
    const Value &transport = info[TRANSPORT_LAYER];
    is_complete &= m_importTransportLayer(transport);
  } else {
    is_complete = false;
  }

  if (m_check_object(info, APP_LAYER)) {
    const Value &conf = info[APP_LAYER];
    is_complete &= m_importApplicationLayer(conf);
    // Use UTC time instead of local time, default false.
    m_retrieve(conf, MODULO_USE_UTC, &m_use_utc, false);
  } else {
    is_complete = false;
  }

  if (m_check_object(info, SOUTH_MONITORING)) {
    const Value &conf = info[SOUTH_MONITORING];

    if(m_check_string(conf, SOUTH_MONITORING_ASSET)) {
      is_complete &= m_retrieve(conf, SOUTH_MONITORING_ASSET, &m_connx_status);
    }
  }

  m_config_is_complete = is_complete;
}

bool HNZConf::m_importTransportLayer(const Value &transport) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_importTransportLayer - ";
  bool is_complete = true;
  if (m_check_array(transport, CONNECTIONS)) {
    const Value &conn = transport[CONNECTIONS];
    if(conn.Size() == 0 || conn.Size() > MAXPATHS){
      string s = IP_ADDR;
      HnzUtility::log_error(beforeLog + "Bad connections informations (needed one or two " + s + ").");
      return false;
    }
    for (int i = 0; i < conn.Size(); i++)
    {
      if (!conn[i].IsObject()) {
        HnzUtility::log_error(beforeLog + "Bad connections informations (one array element is not an object).");
        return false;
      }
      is_complete &= m_retrieve(conn[i], IP_ADDR, &(m_paths_ip[i]));
      is_complete &= m_retrieve(conn[i], IP_PORT, &(m_paths_port[i]), DEFAULT_PORT);
    }
  }
  return is_complete;
}


bool HNZConf::m_importApplicationLayer(const Value &conf) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_importApplicationLayer - ";
  bool is_complete = true;
  is_complete &= m_retrieve(conf, REMOTE_ADDR, &m_remote_station_addr);
  if (m_remote_station_addr > 64) {
    string s = REMOTE_ADDR;
    HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value is not on 6 bits.");
    is_complete = false;
  }

  is_complete &= m_retrieve(conf, INACC_TIMEOUT, &m_inacc_timeout,
                            DEFAULT_INACC_TIMEOUT);

  is_complete &= m_retrieve(conf, MAX_SARM, &m_max_sarm, DEFAULT_MAX_SARM);

  is_complete &=
      m_retrieve(conf, REPEAT_PATH_A, &(m_paths_repeat[0]), DEFAULT_REPEAT_PATH);

  is_complete &=
      m_retrieve(conf, REPEAT_PATH_B, &(m_paths_repeat[1]), DEFAULT_REPEAT_PATH);

  is_complete &= m_retrieve(conf, REPEAT_TIMEOUT, &m_repeat_timeout,
                            DEFAULT_REPEAT_TIMEOUT);

  is_complete &= m_retrieve(conf, ANTICIPATION_RATIO, &m_anticipation_ratio,
                            DEFAULT_ANTICIPATION_RATIO);

  is_complete &= m_retrieve(conf, TST_MSG_SEND, &m_test_msg_send);

  is_complete &= m_retrieve(conf, TST_MSG_RECEIVE, &m_test_msg_receive);

  is_complete &= m_retrieve(conf, GI_SCHEDULE, &m_gi_schedule);

  is_complete &= m_retrieve(conf, GI_REPEAT_COUNT, &m_gi_repeat_count,
                            DEFAULT_GI_REPEAT_COUNT);

  is_complete &= m_retrieve(conf, GI_TIME, &m_gi_time, DEFAULT_GI_TIME);

  is_complete &=
      m_retrieve(conf, C_ACK_TIME, &m_c_ack_time, DEFAULT_C_ACK_TIME);

  is_complete &=
      m_retrieve(conf, CMD_RECV_TIMEOUT, &m_cmd_recv_timeout, DEFAULT_CMD_RECV_TIMEOUT);

  is_complete &= m_retrieve(conf, BULLE_TIME, &m_bulle_time,
                          DEFAULT_BULLE_TIME);
  return is_complete;
}

void HNZConf::importExchangedDataJson(const string &json) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::importExchangedDataJson - ";
  m_exchange_data_is_complete = false;
  bool is_complete = true;

  Document document;
  if (document.Parse(const_cast<char *>(json.c_str())).HasParseError()) {
    HnzUtility::log_fatal(beforeLog + "Parsing error in exchanged_data json, offset " +
                               to_string((unsigned)document.GetErrorOffset()) +
                               " " +
                               GetParseError_En(document.GetParseError()));
    return;
  }
  if (!document.IsObject()) return;

  if (!m_check_object(document, JSON_EXCHANGED_DATA_NAME)) return;

  const Value &info = document[JSON_EXCHANGED_DATA_NAME];

  is_complete &= m_check_string(info, NAME);
  is_complete &= m_check_string(info, JSON_VERSION);

  if (!m_check_array(info, DATAPOINTS)) return;

  m_lastTSAddr = 0;
  for (const Value &msg : info[DATAPOINTS].GetArray()) {
    is_complete &= m_importDatapoint(msg);
  }

  m_exchange_data_is_complete = is_complete;
}

bool HNZConf::m_importDatapoint(const Value &msg) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_importDatapoint - ";

  if (!msg.IsObject()) return false;

  string label;

  bool is_complete = true;
  is_complete &= m_retrieve(msg, LABEL, &label);
  is_complete &= m_check_string(msg, PIVOT_ID);
  is_complete &= m_check_string(msg, PIVOT_TYPE);

  bool isGiTriggeringTs = m_isGiTriggeringTs(msg);

  if (!m_check_array(msg, PROTOCOLS)) return false;

  for (const Value &protocol : msg[PROTOCOLS].GetArray()) {
    if (!protocol.IsObject()) return false;

    string protocol_name;

    is_complete &= m_retrieve(protocol, NAME, &protocol_name);

    if (protocol_name != HNZ_NAME) continue;

    std::string address;
    std::string msg_code;

    is_complete &= m_retrieve(protocol, MESSAGE_ADDRESS, &address, "");
    is_complete &= m_retrieve(protocol, MESSAGE_CODE, &msg_code, "");
    
    unsigned long tmp = 0;
    try {
      tmp = std::stoul(address);
    } catch (const std::invalid_argument &e) {
      HnzUtility::log_error(beforeLog + "Cannot convert address '" + address + "' to integer: " + typeid(e).name() + ": " + e.what());
      is_complete = false;
    } catch (const std::out_of_range &e) {
      HnzUtility::log_error(beforeLog + "Cannot convert address '" + address + "' to integer: " + typeid(e).name() + ": " + e.what());
      is_complete = false;
    }
    unsigned int msg_address = 0;
    // Check if number is in range for unsigned int
    if (tmp > static_cast<unsigned int>(-1)) {
      is_complete = false;
    } else {
      msg_address = static_cast<unsigned int>(tmp);
    }
    m_msg_list[msg_code][m_remote_station_addr][msg_address] = label;
    if (msg_address > m_lastTSAddr) {
      m_lastTSAddr = msg_address;
    }

    if (isGiTriggeringTs) {
      HnzUtility::log_debug(beforeLog + " Storing address " + to_string(msg_address) + " for GI triggering");
      m_cgTriggeringTsAdresses.insert(msg_address);
    }
  }

  return is_complete;
}

bool HNZConf::m_isGiTriggeringTs(const Value &msg) const {
  if (msg.HasMember(PIVOT_SUBTYPES) && msg[PIVOT_SUBTYPES].IsArray()) {
    for (const Value &subtype : msg[PIVOT_SUBTYPES].GetArray()) {
      if (subtype.IsString() && subtype.GetString() == string(TRIGGER_SOUTH_GI_PIVOT_SUBTYPE)) {
        return true;
      }
    }
  }
  return false;
}

string HNZConf::getLabel(const string &msg_code, const int msg_address) const {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::getLabel - ";
  string label;
  try {
    label = m_msg_list.at(msg_code).at(m_remote_station_addr).at(msg_address);
  } catch (const std::out_of_range &e) {
    string code = MESSAGE_CODE;
    string st_addr = REMOTE_ADDR;
    string msg_addr = MESSAGE_ADDRESS;
    label = "";
    HnzUtility::log_warn(beforeLog +
        "The message received does not exist in the configuration (" + code +
        " : " + msg_code + ", " + st_addr + " : " +
        to_string(m_remote_station_addr) + " and " + msg_addr + " : " +
        to_string(msg_address) + ").");
  }
  return label;
}

unsigned long HNZConf::getNumberCG() const {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::getNumberCG - ";
  unsigned long nb;
  try {
    nb = m_msg_list.at("TS").at(m_remote_station_addr).size();
    HnzUtility::log_debug(beforeLog + to_string(nb) + " TSCG in the configuration.");
  } catch (const std::out_of_range &e) {
    nb = 0;
    HnzUtility::log_error(beforeLog + "Error while retrieving the number of TSCG");
  }
  return nb;
}

unsigned int HNZConf::getLastTSAddress() const {
  return m_lastTSAddr;
}

bool HNZConf::m_check_string(const Value &json, const char *key) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_check_string - ";
  if (!json.HasMember(key) || !json[key].IsString()) {
    string s = key;
    HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value does not exist or is not a string.");
    return false;
  }
  return true;
}

bool HNZConf::m_check_array(const Value &json, const char *key) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_check_array - ";
  if (!json.HasMember(key) || !json[key].IsArray()) {
    string s = key;
    HnzUtility::log_error(beforeLog + "The array " + s + " is required but not found.");
    return false;
  }
  return true;
}

bool HNZConf::m_check_object(const Value &json, const char *key) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_check_object - ";
  if (!json.HasMember(key) || !json[key].IsObject()) {
    string s = key;
    HnzUtility::log_error(beforeLog + "The object " + s + " is required but not found.");
    return false;
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, unsigned int *target) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  if (!json.HasMember(key) || !json[key].IsUint()) {
    string s = key;
    HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value does not exist or is not an unsigned integer.");
    return false;
  }
  *target = json[key].GetUint();
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, unsigned int *target, unsigned int def) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsUint()) {
      string s = key;
      HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value is not an unsigned integer.");
      return false;
    }
    *target = json[key].GetUint();
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, string *target) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  if (!json.HasMember(key) || !json[key].IsString()) {
    string s = key;
    HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value does not exist or is not a string.");
    return false;
  }
  *target = json[key].GetString();
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, string *target, const string& def) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsString()) {
      string s = key;
      HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value is not a string.");
      return false;
    }
    *target = json[key].GetString();
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, BulleFormat *target) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  BulleFormat bulle;
  string temp;
  string s = key;
  if (!json.HasMember(key)) {
    temp = DEFAULT_TST_MSG;
  } else {
    if (!json[key].IsString()) {
      HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value does not exist or is not a string.");
      return false;
    }

    temp = json[key].GetString();
    if (temp.size() != 4) {
      HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value isn't correct. Must be 4 digits.");
      return false;
    }
  }
  
  try {
    bulle.first = stoul(temp.substr(0, 2), nullptr, 16);
    bulle.second = stoul(temp.substr(2), nullptr, 16);
  } catch (const std::invalid_argument &e) {
    HnzUtility::log_error(beforeLog + "Error with the field " + s + ", cannot convert hex string to int: " + typeid(e).name() + ": " + e.what());
    return false;
  } catch (const std::out_of_range &e) {
    HnzUtility::log_error(beforeLog + "Error with the field " + s + ", cannot convert hex string to int: " + typeid(e).name() + ": " + e.what());
    return false;
  }

  *target = bulle;
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, GIScheduleFormat *target) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  GIScheduleFormat time;
  time.activate = false;
  time.hour = 99;
  time.min = 99;
  if (json.HasMember(key)) {
    if (!json[key].IsString()) {
      string s = key;
      HnzUtility::log_error(beforeLog +  "Error with the field " + s + ", the value does not exist or is not a string.");
      return false;
    }

    string temp = json[key].GetString();

    if (temp != DEFAULT_GI_SCHEDULE) {
      if ((temp.size() != 5) || (temp.substr(2, 1) != ":")) {
        string s = key;
        HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value isn't correct. Format : 'HH:MM'.");
        return false;
      }

      try {
        time.hour = stoi(temp.substr(0, 2));
        time.min = stoi(temp.substr(3));
      } catch (const std::invalid_argument &e) {
        HnzUtility::log_error(beforeLog + "Cannot convert time '" + temp + "' to integers: " + typeid(e).name() + ": " + e.what());
        return false;
      } catch (const std::out_of_range &e) {
        HnzUtility::log_error(beforeLog + "Cannot convert time '" + temp + "' to integers: " + typeid(e).name() + ": " + e.what());
        return false;
      }

      if (time.hour >= 0 && time.hour < 24 && time.min >= 0 && time.min < 60) {
        time.activate = true;
      } else {
        string s = key;
        HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value isn't correct. " +
                                          "Example of correct value : 18:00, 07:25, 00:00.");
        return false;
      }
    }
  }

  *target = time;
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, long long int *target, long long int def) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsInt64()) {
      string s = key;
      HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value is not a long long integer.");
      return false;
    }
    *target = json[key].GetInt64();
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, bool *target, bool def) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConf::m_retrieve - ";
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsBool()) {
      string s = key;
      HnzUtility::log_error(beforeLog + "Error with the field " + s + ", the value is not a bool.");
      return false;
    }
    *target = json[key].GetBool();
  }
  return true;
}