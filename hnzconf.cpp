#include "hnzconf.h"

using namespace rapidjson;

HNZConf::HNZConf() : m_is_complete(false) {}

HNZConf::HNZConf(const std::string &json_config) { import_json(json_config); }

HNZConf::~HNZConf() {}

void HNZConf::import_json(const std::string &json_config) {
  m_is_complete = false;

  Document document;

  if (document.Parse(const_cast<char *>(json_config.c_str())).HasParseError()) {
    Logger::getLogger()->fatal(
        "Parsing error in protocol_stack json, offset " +
        std::to_string((unsigned)document.GetErrorOffset()));
    // More details : GetParseError_En(document.GetParseErrorCode())
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
    if (!conn.HasMember(IP_ADDR) || !conn.IsString()) return;
    m_ip = conn[IP_ADDR].GetString();

    if (!conn.HasMember(IP_PORT)) {
      m_port = DEFAULT_PORT;
    } else if (!conn[IP_PORT].IsUint())
      return;
    m_port = conn[IP_PORT].GetUint();
  }

  if (!info.HasMember(APP_LAYER) || !info[APP_LAYER].IsObject()) return;

  const Value &conf = info[APP_LAYER];

  if (!conf.HasMember(REMOTE_ADDR) || !conf[REMOTE_ADDR].IsUint()) return;
  m_remote_station_addr = conf[REMOTE_ADDR].GetUint();
  if (m_remote_station_addr > 64) return;

  if (!conf.HasMember(LOCAL_ADDR) || !conf[LOCAL_ADDR].IsUint()) return;
  m_local_station_addr = conf[LOCAL_ADDR].GetUint();
  if (m_local_station_addr > 64) return;

  if (!conf.HasMember(REMOTE_ADDR_IN_LOCAL) ||
      !conf[REMOTE_ADDR_IN_LOCAL].IsUint())
    return;
  m_remote_addr_in_local_station = conf[REMOTE_ADDR_IN_LOCAL].GetUint();
  if (m_remote_addr_in_local_station > 2) return;

  if (!conf.HasMember(INACC_TIMEOUT)) {
    m_inacc_timeout = DEFAULT_INACC_TIMEOUT;
  } else if (!conf[INACC_TIMEOUT].IsUint())
    return;
  m_inacc_timeout = conf[INACC_TIMEOUT].GetUint();

  if (!conf.HasMember(MAX_SARM)) {
    m_max_sarm = DEFAULT_MAX_SARM;
  } else if (!conf[MAX_SARM].IsUint())
    return;
  m_max_sarm = conf[MAX_SARM].GetUint();

  if (!conf.HasMember(TO_SOCKET)) {
    // TODO : to check
    m_to_socket = DEFAULT_TO_SOCKET;
  } else if (!conf[TO_SOCKET].IsUint())
    return;
  m_to_socket = conf[TO_SOCKET].GetUint();

  if (!conf.HasMember(REPEAT_PATH_A)) {
    m_repeat_path_A = DEFAULT_REPEAT_PATH;
  } else if (!conf[REPEAT_PATH_A].IsUint())
    return;
  m_repeat_path_A = conf[REPEAT_PATH_A].GetUint();

  if (!conf.HasMember(REPEAT_PATH_B)) {
    m_repeat_path_B = DEFAULT_REPEAT_PATH;
  } else if (!conf[REPEAT_PATH_B].IsUint())
    return;
  m_repeat_path_B = conf[REPEAT_PATH_B].GetUint();

  if (!conf.HasMember(REPEAT_TIMEOUT)) {
    m_repeat_timeout = DEFAULT_REPEAT_TIMEOUT;
  } else if (!conf[REPEAT_TIMEOUT].IsUint())
    return;
  m_repeat_timeout = conf[REPEAT_TIMEOUT].GetUint();

  if (!conf.HasMember(ANTICIPATION)) {
    m_anticipation = DEFAULT_ANTICIPATION;
  } else if (!conf[ANTICIPATION].IsUint())
    return;
  m_anticipation = conf[ANTICIPATION].GetUint();

  if (!conf.HasMember(DEFAULT_MSG_PERIOD)) {
    // TODO : to check
    m_default_msg_period = DEFAULT_DEFAULT_MSG_PERIOD;
  } else if (!conf[DEFAULT_MSG_PERIOD].IsUint())
    return;
  m_default_msg_period = conf[DEFAULT_MSG_PERIOD].GetUint();

  if (!conf.HasMember(TST_MSG_SEND) || !conf[TST_MSG_SEND].IsString()) return;
  m_test_msg_send = conf[TST_MSG_SEND].GetString();

  if (!conf.HasMember(TST_MSG_RECEIVE) || !conf[TST_MSG_RECEIVE].IsString())
    return;
  m_test_msg_receive = conf[TST_MSG_RECEIVE].GetString();

  m_is_complete = true;
}