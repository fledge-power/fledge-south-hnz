/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#include "hnzconf.h"

HNZConf::HNZConf()
    : m_config_is_complete(false), m_exchange_data_is_complete(false) {}

HNZConf::HNZConf(const string &json_config, const string &json_exchanged_data) {
  importConfigJson(json_config);
  importExchangedDataJson(json_exchanged_data);
}

HNZConf::~HNZConf() {}

void HNZConf::importConfigJson(const string &json) {
  m_config_is_complete = false;

  bool is_complete = true;

  Document document;

  if (document.Parse(const_cast<char *>(json.c_str())).HasParseError()) {
    Logger::getLogger()->fatal("Parsing error in protocol_stack json, offset " +
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

    if (m_check_array(transport, CONNECTIONS)) {
      // Use only the first ip/port (TODO : redundancy)
      const Value &conn = transport[CONNECTIONS];
      if (conn.Size() >= 1) {
        is_complete &= m_retrieve(conn[0], IP_ADDR, &m_ip);
        is_complete &= m_retrieve(conn[0], IP_PORT, &m_port, DEFAULT_PORT);
      } else {
        string s = IP_ADDR;
        Logger::getLogger()->error(
            "Missing connections informations (at least one " + s + ").");
        is_complete = false;
      }
    }
  }

  if (m_check_object(info, APP_LAYER)) {
    const Value &conf = info[APP_LAYER];

    is_complete &= m_retrieve(conf, REMOTE_ADDR, &m_remote_station_addr);
    if (m_remote_station_addr > 64) {
      string s = REMOTE_ADDR;
      Logger::getLogger()->error("Error with the field " + s +
                                 ", the value is not on 6 bits.");
      is_complete = false;
    }

    is_complete &= m_retrieve(conf, INACC_TIMEOUT, &m_inacc_timeout,
                              DEFAULT_INACC_TIMEOUT);

    is_complete &= m_retrieve(conf, MAX_SARM, &m_max_sarm, DEFAULT_MAX_SARM);

    is_complete &=
        m_retrieve(conf, REPEAT_PATH_A, &m_repeat_path_A, DEFAULT_REPEAT_PATH);

    is_complete &=
        m_retrieve(conf, REPEAT_PATH_B, &m_repeat_path_B, DEFAULT_REPEAT_PATH);

    is_complete &= m_retrieve(conf, REPEAT_TIMEOUT, &m_repeat_timeout,
                              DEFAULT_REPEAT_TIMEOUT);

    is_complete &= m_retrieve(conf, ANTICIPATION_RATIO, &m_anticipation_ratio,
                              DEFAULT_ANTICIPATION_RATIO);

    is_complete &= m_retrieve(conf, TST_MSG_SEND, &m_test_msg_send);

    is_complete &= m_retrieve(conf, TST_MSG_RECEIVE, &m_test_msg_receive);

    is_complete &=
        m_retrieve(conf, GI_SCHEDULE, &m_gi_schedule, DEFAULT_GI_SCHEDULE);

    is_complete &= m_retrieve(conf, GI_REPEAT_COUNT, &m_gi_repeat_count,
                              DEFAULT_GI_REPEAT_COUNT);

    is_complete &= m_retrieve(conf, GI_TIME, &m_gi_time, DEFAULT_GI_TIME);

    is_complete &=
        m_retrieve(conf, C_ACK_TIME, &m_c_ack_time, DEFAULT_C_ACK_TIME);
  }

  m_config_is_complete = is_complete;
}

void HNZConf::importExchangedDataJson(const string &json) {
  m_exchange_data_is_complete = false;
  bool is_complete = true;

  Document document;
  if (document.Parse(const_cast<char *>(json.c_str())).HasParseError()) {
    Logger::getLogger()->fatal("Parsing error in exchanged_data json, offset " +
                               to_string((unsigned)document.GetErrorOffset()) +
                               " " +
                               GetParseError_En(document.GetParseError()));
    return;
  }
  if (!document.IsObject()) return;

  if (!m_check_object(document, JSON_EXCHANGED_DATA_NAME)) return;

  const Value &info = document[JSON_EXCHANGED_DATA_NAME];

  is_complete &=
      m_check_string(info, NAME) && m_check_string(info, JSON_VERSION);

  if (!m_check_array(info, DATAPOINTS)) return;

  for (const Value &msg : info[DATAPOINTS].GetArray()) {
    if (!msg.IsObject()) return;

    string label;

    is_complete &= m_retrieve(msg, LABEL, &label) &&
                   m_check_string(msg, PIVOT_ID) &&
                   m_check_string(msg, PIVOT_TYPE);

    if (m_check_array(msg, PROTOCOLS)) {
      for (const Value &protocol : msg[PROTOCOLS].GetArray()) {
        if (!protocol.IsObject()) return;

        string protocol_name;

        is_complete &= m_retrieve(protocol, NAME, &protocol_name);

        if (protocol_name == HNZ_NAME) {
          unsigned int station_address;
          unsigned int msg_address;
          string msg_code;

          is_complete &=
              m_retrieve(protocol, STATION_ADDRESS, &station_address) &&
              m_retrieve(protocol, MESSAGE_ADDRESS, &msg_address) &&
              m_retrieve(protocol, MESSAGE_CODE, &msg_code);

          m_msg_list[msg_code][station_address][msg_address] = label;
        }
      }
    }
  }

  m_exchange_data_is_complete = is_complete;
}

string HNZConf::getLabel(const string &msg_code, const int msg_address) {
  string label;
  try {
    label = m_msg_list.at(msg_code).at(m_remote_station_addr).at(msg_address);
  } catch (const std::out_of_range &e) {
    string code = MESSAGE_CODE;
    string st_addr = STATION_ADDRESS;
    string msg_addr = MESSAGE_ADDRESS;
    label = "";
    Logger::getLogger()->warn(
        "The message received does not exist in the configuration (" + code +
        " : " + msg_code + ", " + st_addr + " : " +
        to_string(m_remote_station_addr) + " and " + msg_addr + " : " +
        to_string(msg_address) + ").");
  }
  return label;
}

int HNZConf::getNumberCG() {
  int nb;
  try {
    nb = m_msg_list.at("TSCG").at(m_remote_station_addr).size();
    Logger::getLogger()->debug(to_string(nb) + " TSCG in the configuration.");
  } catch (const std::out_of_range &e) {
    nb = 0;
    Logger::getLogger()->error("Error while retrieving the number of TSCG");
  }
  return nb;
}

bool HNZConf::m_check_string(const Value &json, const char *key) {
  if (!json.HasMember(key) || !json[key].IsString()) {
    string s = key;
    Logger::getLogger()->error(
        "Error with the field " + s +
        ", the value does not exist or is not a string.");
    return false;
  }
  return true;
}

bool HNZConf::m_check_array(const Value &json, const char *key) {
  if (!json.HasMember(key) || !json[key].IsArray()) {
    string s = key;
    Logger::getLogger()->error("The array " + s +
                               " is required but not found.");
    return false;
  }
  return true;
}

bool HNZConf::m_check_object(const Value &json, const char *key) {
  if (!json.HasMember(key) || !json[key].IsObject()) {
    string s = key;
    Logger::getLogger()->error("The object " + s +
                               " is required but not found.");
    return false;
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         unsigned int *target) {
  if (!json.HasMember(key) || !json[key].IsUint()) {
    string s = key;
    Logger::getLogger()->error(
        "Error with the field " + s +
        ", the value does not exist or is not an unsigned integer.");
    return false;
  }
  *target = json[key].GetUint();
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         unsigned int *target, unsigned int def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsUint()) {
      string s = key;
      Logger::getLogger()->error("Error with the field " + s +
                                 ", the value is not an unsigned integer.");
      return false;
    }
    *target = json[key].GetUint();
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, string *target) {
  if (!json.HasMember(key) || !json[key].IsString()) {
    string s = key;
    Logger::getLogger()->error(
        "Error with the field " + s +
        ", the value does not exist or is not a string.");
    return false;
  }
  *target = json[key].GetString();
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key, string *target,
                         string def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsString()) {
      string s = key;
      Logger::getLogger()->error("Error with the field " + s +
                                 ", the value is not a string.");
      return false;
    }
    *target = json[key].GetString();
  }
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         BulleFormat *target) {
  if (!json.HasMember(key) || !json[key].IsString()) {
    string s = key;
    Logger::getLogger()->error(
        "Error with the field " + s +
        ", the value does not exist or is not a string.");
    return false;
  }
  string temp = json[key].GetString();
  if (temp.size() != 4) {
    string s = key;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value isn't correct. Must be 4 digits.");
    return false;
  }
  BulleFormat bulle;
  bulle.first = stoul(temp.substr(0, 2), nullptr, 16);
  bulle.second = stoul(temp.substr(2), nullptr, 16);
  *target = bulle;
  return true;
}