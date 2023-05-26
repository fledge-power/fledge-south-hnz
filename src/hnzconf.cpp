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
      const Value &conn = transport[CONNECTIONS];
      if (conn.Size() == 1) {
        is_complete &= m_retrieve(conn[0], IP_ADDR, &m_ip_A);
        is_complete &= m_retrieve(conn[0], IP_PORT, &m_port_A, DEFAULT_PORT);
      } else if (conn.Size() == 2) {
        is_complete &= m_retrieve(conn[0], IP_ADDR, &m_ip_A);
        is_complete &= m_retrieve(conn[0], IP_PORT, &m_port_A, DEFAULT_PORT);
        is_complete &= m_retrieve(conn[1], IP_ADDR, &m_ip_B);
        is_complete &= m_retrieve(conn[1], IP_PORT, &m_port_B, DEFAULT_PORT);
      } else {
        string s = IP_ADDR;
        Logger::getLogger()->error(
            "Bad connections informations (needed one or two " + s + ").");
        is_complete = false;
      }
    }
  } else {
    is_complete = false;
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

    is_complete &= m_retrieve(conf, GI_SCHEDULE, &m_gi_schedule);

    is_complete &= m_retrieve(conf, GI_REPEAT_COUNT, &m_gi_repeat_count,
                              DEFAULT_GI_REPEAT_COUNT);

    is_complete &= m_retrieve(conf, GI_TIME, &m_gi_time, DEFAULT_GI_TIME);

    is_complete &=
        m_retrieve(conf, C_ACK_TIME, &m_c_ack_time, DEFAULT_C_ACK_TIME);

    is_complete &=
        m_retrieve(conf, CMD_RECV_TIMEOUT, &m_cmd_recv_timeout, DEFAULT_CMD_RECV_TIMEOUT);
  } else {
    is_complete = false;
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

string HNZConf::getLabel(const string &msg_code, const int msg_address) const {
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

unsigned long HNZConf::getNumberCG() const {
  unsigned long nb;
  try {
    nb = m_msg_list.at("TS").at(m_remote_station_addr).size();
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
  BulleFormat bulle;
  string temp;
  if (!json.HasMember(key)) {
    temp = DEFAULT_TST_MSG;
  } else {
    if (!json[key].IsString()) {
      string s = key;
      Logger::getLogger()->error(
          "Error with the field " + s +
          ", the value does not exist or is not a string.");
      return false;
    }

    temp = json[key].GetString();
    if (temp.size() != 4) {
      string s = key;
      Logger::getLogger()->error(
          "Error with the field " + s +
          ", the value isn't correct. Must be 4 digits.");
      return false;
    }
  }
  bulle.first = stoul(temp.substr(0, 2), nullptr, 16);
  bulle.second = stoul(temp.substr(2), nullptr, 16);

  *target = bulle;
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         GIScheduleFormat *target) {
  GIScheduleFormat time;
  time.activate = false;
  time.hour = 99;
  time.min = 99;
  if (json.HasMember(key)) {
    if (!json[key].IsString()) {
      string s = key;
      Logger::getLogger()->error(
          "Error with the field " + s +
          ", the value does not exist or is not a string.");
      return false;
    }

    string temp = json[key].GetString();

    if (temp != DEFAULT_GI_SCHEDULE) {
      if (temp.size() != 5 && temp.substr(2, 1) == ":") {
        string s = key;
        Logger::getLogger()->error(
            "Error with the field " + s +
            ", the value isn't correct. Format : 'HH:MM'.");
        return false;
      }

      time.hour = stoi(temp.substr(0, 2));
      time.min = stoi(temp.substr(3));

      if (time.hour >= 0 && time.hour < 24 && time.min >= 0 && time.min < 60) {
        time.activate = true;
      } else {
        string s = key;
        Logger::getLogger()->error(
            "Error with the field " + s +
            ", the value isn't correct. Example of correct "
            "value : 18:00, 07:25, 00:00.");
        return false;
      }
    }
  }

  *target = time;
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         long long int *target, long long int def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsInt64()) {
      string s = key;
      Logger::getLogger()->error("Error with the field " + s +
                                 ", the value is not a long long integer.");
      return false;
    }
    *target = json[key].GetInt64();
  }
  return true;
}