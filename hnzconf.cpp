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

using namespace rapidjson;

HNZConf::HNZConf() : m_is_complete(false) {}

HNZConf::HNZConf(const std::string &json_config) { import_json(json_config); }

HNZConf::~HNZConf() {}

void HNZConf::import_json(const std::string &json_config) {
  m_is_complete = false;

  bool is_complete = true;

  Document document;

  if (document.Parse(const_cast<char *>(json_config.c_str())).HasParseError()) {
    Logger::getLogger()->fatal(
        "Parsing error in protocol_stack json, offset " +
        std::to_string((unsigned)document.GetErrorOffset()) + " " +
        GetParseError_En(document.GetParseError()));
    return;
  }
  if (!document.IsObject()) return;

  if (!document.HasMember(JSON_NAME) || !document[JSON_NAME].IsObject()) return;

  const Value &info = document[JSON_NAME];

  if (!info.HasMember(NAME) || !info[NAME].IsString()) return;

  if (!info.HasMember(JSON_VERSION) || !info[JSON_VERSION].IsString()) return;

  if (!info.HasMember(TRANSPORT_LAYER) || !info[TRANSPORT_LAYER].IsObject())
    return;

  const Value &transport = info[TRANSPORT_LAYER];

  if (!transport.HasMember(CONNECTIONS) || !transport[CONNECTIONS].IsArray())
    return;

  // If there are more than 2 objects, ignoring them
  for (SizeType i = 0; i < 2; i++) {
    const Value &conn = transport[CONNECTIONS][i];

    is_complete &= m_retrieve(conn, IP_ADDR, &m_ip);

    is_complete &= m_retrieve(conn, IP_PORT, &m_port, DEFAULT_PORT);
  }

  if (!info.HasMember(APP_LAYER) || !info[APP_LAYER].IsObject()) return;

  const Value &conf = info[APP_LAYER];

  is_complete &= m_retrieve(conf, REMOTE_ADDR, &m_remote_station_addr);
  if (m_remote_station_addr > 64) {
    std::string s = REMOTE_ADDR;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value is not on 6 bits.");
    return;
  }

  is_complete &= m_retrieve(conf, LOCAL_ADDR, &m_local_station_addr);
  if (m_local_station_addr > 64) {
    std::string s = LOCAL_ADDR;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value is not on 6 bits.");
    return;
  }

  is_complete &=
      m_retrieve(conf, REMOTE_ADDR_IN_LOCAL, &m_remote_addr_in_local_station);
  if (m_remote_addr_in_local_station > 2) {
    std::string s = REMOTE_ADDR_IN_LOCAL;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value is not equal to 0, 1 or 2.");
    return;
  }

  is_complete &=
      m_retrieve(conf, INACC_TIMEOUT, &m_inacc_timeout, DEFAULT_INACC_TIMEOUT);

  is_complete &= m_retrieve(conf, MAX_SARM, &m_max_sarm, DEFAULT_MAX_SARM);

  // TODO : to check
  is_complete &= m_retrieve(conf, TO_SOCKET, &m_to_socket, DEFAULT_TO_SOCKET);

  is_complete &=
      m_retrieve(conf, REPEAT_PATH_A, &m_repeat_path_A, DEFAULT_REPEAT_PATH);

  is_complete &=
      m_retrieve(conf, REPEAT_PATH_B, &m_repeat_path_B, DEFAULT_REPEAT_PATH);

  is_complete &= m_retrieve(conf, REPEAT_TIMEOUT, &m_repeat_timeout,
                            DEFAULT_REPEAT_TIMEOUT);

  is_complete &=
      m_retrieve(conf, ANTICIPATION, &m_anticipation, DEFAULT_ANTICIPATION);

  // TODO : to check
  is_complete &= m_retrieve(conf, DEFAULT_MSG_PERIOD, &m_default_msg_period,
                            DEFAULT_DEFAULT_MSG_PERIOD);

  is_complete &= m_retrieve(conf, TST_MSG_SEND, &m_test_msg_send);

  is_complete &= m_retrieve(conf, TST_MSG_RECEIVE, &m_test_msg_receive);

  m_is_complete = is_complete;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         unsigned int *target) {
  if (!json.HasMember(key) || !json[key].IsUint()) {
    std::string s = key;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value is not an unsigned integer.");
    return false;
  }
  *target = json[key].GetUint();
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         unsigned int *target, unsigned int def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else if (!json[key].IsUint()) {
    std::string s = key;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value is not an unsigned integer.");
    return false;
  }
  *target = json[key].GetUint();
  return true;
}

bool HNZConf::m_retrieve(const Value &json, const char *key,
                         std::string *target) {
  if (!json.HasMember(key) || !json[key].IsString()) {
    std::string s = key;
    Logger::getLogger()->error("Error with the field " + s +
                               ", the value is not a string.");
    return false;
  }
  *target = json[key].GetString();
  return true;
}