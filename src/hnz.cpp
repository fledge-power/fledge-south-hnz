/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Lucas Barret, Colin Constans, Justin Facquet
 */

#include <set>

#include <hnz_server.h>

#include "hnzutility.h"
#include "hnz.h"
#include "hnzconnection.h"
#include "hnzpath.h"

HNZ::HNZ() {}

HNZ::~HNZ() {
  if (m_is_running) {
    stop();
  }
}

void HNZ::start() {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex);
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::start -";

  if (!m_hnz_conf->is_complete()) {
    HnzUtility::log_info("%s HNZ south plugin can't start because configuration is incorrect.", beforeLog.c_str());
    return;
  }

  HnzUtility::log_info("%s Starting HNZ south plugin...", beforeLog.c_str());

  m_is_running = true;

  m_sendAllTMQualityReadings(true, false);
  m_sendAllTSQualityReadings(true, false);

  auto pathPair = m_hnz_connection->getBothPath();
  m_receiving_thread_A = make_unique<thread>(&HNZ::receive, this, pathPair.first);
  if (pathPair.second != nullptr) {
    // Wait after getting the passive path pointer as connection init of active path may swap path
    this_thread::sleep_for(milliseconds(1000));
    // Path B is defined in the configuration
    m_receiving_thread_B = make_unique<thread>(&HNZ::receive, this, pathPair.second);
  }

  m_hnz_connection->start();
}

void HNZ::stop() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::stop -";
  HnzUtility::log_info("%s Starting shutdown of HNZ plugin", beforeLog.c_str());
  m_is_running = false;

  // Connection must be stopped before management threads of both path
  // or join on both receive threads will hang forever
  if (m_hnz_connection != nullptr) {
    m_hnz_connection->stop();
  }
  if (m_receiving_thread_A != nullptr) {
    HnzUtility::log_debug("%s Waiting for the receiving thread (path A)", beforeLog.c_str());
    m_receiving_thread_A->join();
    m_receiving_thread_A = nullptr;
  }
  if (m_receiving_thread_B != nullptr) {
    HnzUtility::log_debug("%s Waiting for the receiving thread (path B)", beforeLog.c_str());
    m_receiving_thread_B->join();
    m_receiving_thread_B = nullptr;
  }
  // Connection must be freed after management threads of both path
  // as HNZ::m_hnz_connection, HNZConnection::m_active_path and HNZConnection::m_passive_path
  // are used in HNZ::receive running on the threads
  if (m_hnz_connection != nullptr) {
    m_hnz_connection = nullptr;
  }
  HnzUtility::log_info("%s Plugin stopped !", beforeLog.c_str());
}

void HNZ::reconfigure(const ConfigCategory& config) {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex);
  std::string protocol_conf_json;
  if (config.itemExists("protocol_stack")) {
    protocol_conf_json = config.getValue("protocol_stack");
  }
  std::string msg_conf_json;
  if (config.itemExists("exchanged_data")) {
    msg_conf_json = config.getValue("exchanged_data");
  }

  setJsonConfig(protocol_conf_json, msg_conf_json);
}

void HNZ::setJsonConfig(const string& protocol_conf_json, const string& msg_conf_json) {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex);
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::setJsonConfig -";
  // If no new json configuration and the plugin is already in the correct running state, nothing to do
  if (protocol_conf_json.empty() && msg_conf_json.empty()) {
    HnzUtility::log_info("%s No new configuration provided to reconfigure, skipping", beforeLog.c_str());
    return;
  }
  bool was_running = m_is_running;
  if (m_is_running) {
    HnzUtility::log_info("%s Configuration change requested, stopping the plugin", beforeLog.c_str());
    stop();
  }

  HnzUtility::log_info("%s Reading json config string...", beforeLog.c_str());

  if (!protocol_conf_json.empty()) {
    m_hnz_conf->importConfigJson(protocol_conf_json);
  }
  if (!msg_conf_json.empty()) {
    m_hnz_conf->importExchangedDataJson(msg_conf_json);
  }
  if (!m_hnz_conf->is_complete()) {
    HnzUtility::log_fatal("%s Unable to set Plugin configuration due to error with the json conf.", beforeLog.c_str());
    return;
  }

  HnzUtility::log_info("%s Json config parsed successsfully.", beforeLog.c_str());

  m_remote_address = m_hnz_conf->get_remote_station_addr();
  m_test_msg_receive = m_hnz_conf->get_test_msg_receive();
  m_hnz_connection = make_unique<HNZConnection>(m_hnz_conf, this);

  if (was_running) {
    HnzUtility::log_warn("%s Restarting the plugin...", beforeLog.c_str());
    start();
  }
}

void HNZ::receive(std::shared_ptr<HNZPath> hnz_path_in_use) {
  if (m_hnz_conf) {
    // Parent if used only for scope lock
    std::lock_guard<std::recursive_mutex> guard(m_configMutex);
    if (!m_hnz_conf->is_complete()) {
      return;
    }
  }

  string path = hnz_path_in_use->getName();
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::receive - " + path;

  // Connect to the server
  hnz_path_in_use->connect();

  // Exit early if connection shutting down
  if(!m_is_running) {
    HnzUtility::log_info("%s Connection shutting down, exit", beforeLog.c_str());
    return;
  }

  HnzUtility::log_warn("%s Listening for data...", beforeLog.c_str());

  vector<vector<unsigned char>> messages;

  while (m_is_running) {
    // Waiting for data
    messages = hnz_path_in_use->getData();

    if (messages.empty() && !hnz_path_in_use->isConnected()) {
      HnzUtility::log_warn("%s Connection lost, reconnecting active path and switching to other path", beforeLog.c_str());
      // If connection lost, try to switch path
      if (hnz_path_in_use->isActivePath()) m_hnz_connection->switchPath();
      // Try to reconnect, unless thread is stopping
      if (m_is_running) {
        hnz_path_in_use->disconnect();
      }
      // Shutdown request may happen while disconnecting, if it does cancel reconnection
      if (m_is_running) {
        hnz_path_in_use->connect();
      }
    }
    else {
      // Push each message to fledge
      for (auto& msg : messages) {
        m_handle_message(msg);
      }
    }
    messages.clear();
  }
}

/* Helper function used to get a printable version of an address list */
std::string formatAddresses(const std::vector<unsigned int>& addresses) {
  std::string out = "[";
  for(auto address: addresses) {
    if(out.size() > 1) {
      out += ", ";
    }
    out += std::to_string(address);
  }
  out += "]";
  return out;
}

void HNZ::m_handle_message(const vector<unsigned char>& data) {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex);
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::m_handle_message -";
  unsigned char t = data[0];  // Payload type
  vector<Reading> readings;   // Contains data object to push to fledge

  switch (t) {
  case MODULO_CODE:
    HnzUtility::log_info("%s Received modulo time update", beforeLog.c_str());
    m_handleModuloCode(readings, data);
    break;
  case TM4_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TMA", beforeLog.c_str());
    m_handleTM4(readings, data);
    break;
  case TSCE_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TSCE", beforeLog.c_str());
    m_handleTSCE(readings, data);
    break;
  case TSCG_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TSCG", beforeLog.c_str());
    m_handleTSCG(readings, data);
    break;
  case TMN_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TMN", beforeLog.c_str());
    m_handleTMN(readings, data);
    break;
  case TCACK_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TC ACK", beforeLog.c_str());
    m_handleATC(readings, data);
    break;
  case TVCACK_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TVC ACK", beforeLog.c_str());
    m_handleATVC(readings, data);
    break;
  default:
    if (!(t == m_test_msg_receive.first &&
      data[1] == m_test_msg_receive.second)) {
      HnzUtility::log_error("%s Unknown message to push: %s", beforeLog.c_str(), frameToStr(data).c_str());
    }
    break;
  }

  if (!readings.empty()) {
    m_sendToFledge(readings);
  }
  // Check if GI is complete (make sure we only update the end of GI status after creating readings for all TS received)
  if ((t == TSCG_CODE) && !m_gi_addresses_received.empty()) {
    // Last expected TS received: GI succeeded
    if (m_gi_addresses_received.back() == m_hnz_conf->getLastTSAddress()) {  
      auto nbTSCG = m_hnz_conf->getNumberCG();
      // Mismatch in the number of TS received: Log CG as incomplete
      if (m_gi_addresses_received.size() != nbTSCG) {
        AddressesDiff TSAddressesDiff = m_getMismatchingTSCGAddresses();
        HnzUtility::log_warn("%s Received last TSCG but %lu TS received when %lu were expected: Missing %s, Extra %s",
                            beforeLog.c_str(), m_gi_addresses_received.size(), nbTSCG,
                            formatAddresses(TSAddressesDiff.missingAddresses).c_str(),
                            formatAddresses(TSAddressesDiff.extraAddresses).c_str());
      }
      m_hnz_connection->checkGICompleted(true);
    }
  }
}

void HNZ::m_handleModuloCode(vector<Reading>& readings, const vector<unsigned char>& data) {
  // No reading to send when reciving modulo code, but keep the parameter
  // to get a homogenous signature for all m_handle*() methods
  readings.clear();
  m_daySection = data[1];
}

void HNZ::m_handleTM4(vector<Reading>& readings, const vector<unsigned char>& data) const {
  string msg_code = "TM";
  for (int i = 0; i < 4; i++) {
    // 4 TM inside a TM cyclique
    auto msg_address = static_cast<unsigned int>(data[1] + i); // ADR + i
    string label = m_hnz_conf->getLabel(msg_code, msg_address);
    if (label.empty()) {
      continue;
    }

    int noctet = 2 + i;
    long int value = data[noctet]; // VALTMi
    if ((data[noctet] & 0x80) > 0) { // Ones' complement
      value = -(value ^ 0xFF);
    }
    unsigned int valid = (data[noctet] == 0xFF);  // Invalid if VALTMi = 0xFF

    ReadingParameters params;
    params.label = label;
    params.msg_code = msg_code;
    params.station_addr = m_remote_address;
    params.msg_address = msg_address;
    params.value = value;
    params.valid = valid;
    params.an = "TMA";
    readings.push_back(m_prepare_reading(params));
  }
}

void HNZ::m_handleTSCE(vector<Reading>& readings, const vector<unsigned char>& data) const {
  string msg_code = "TS";
  unsigned int msg_address = stoi(to_string((int)data[1]) +
    to_string((int)(data[2] >> 5)));  // AD0 + ADB

  string label = m_hnz_conf->getLabel(msg_code, msg_address);
  if (label.empty()) {
    return;
  }

  long int value = (data[2] >> 3) & 0x1;  // E bit
  unsigned int valid = (data[2] >> 4) & 0x1;  // V bit

  unsigned int ts = ((data[3] << 8) | data[4]); // Timestamp in 10 milliseconds in modulo of 10 minutes of current day
  unsigned int ts_iv = (data[2] >> 2) & 0x1;  // HNV bit
  unsigned int ts_s = data[2] & 0x1;          // S bit
  unsigned int ts_c = (data[2] >> 1) & 0x1;   // C bit
  unsigned long epochMs = getEpochMsTimestamp(std::chrono::system_clock::now(), m_daySection, ts);

  ReadingParameters params;
  params.label = label;
  params.msg_code = msg_code;
  params.station_addr = m_remote_address;
  params.msg_address = msg_address;
  params.value = value;
  params.valid = valid;
  params.ts = epochMs;
  params.ts_iv = ts_iv;
  params.ts_c = ts_c;
  params.ts_s = ts_s;
  params.cg = false;
  readings.push_back(m_prepare_reading(params));
}

void HNZ::m_handleTSCG(vector<Reading>& readings, const vector<unsigned char>& data) {
  string msg_code = "TS";
  for (size_t i = 0; i < 16; i++) {
    // 16 TS inside a TSCG
    unsigned int msg_address = stoi(
      to_string((int)data[1] + (int)i / 8) +
      to_string(i % 8));  // AD0 + i%8 for first 8, (AD0+1) + i%8 for others
    string label = m_hnz_conf->getLabel(msg_code, msg_address);
    if (label.empty()) {
      continue;
    }

    int noctet = 2 + (i / 4);
    int dep = (3 - (i % 4)) * 2;
    long int value = (data[noctet] >> dep) & 0x1;  // E
    unsigned int valid = ((data[noctet] >> dep) & 0x2) >> 1;  // V

    ReadingParameters params;
    params.label = label;
    params.msg_code = msg_code;
    params.station_addr = m_remote_address;
    params.msg_address = msg_address;
    params.value = value;
    params.valid = valid;
    params.cg = true;
    m_gi_addresses_received.push_back(msg_address);
    readings.push_back(m_prepare_reading(params));
  }
  if (getGiStatus() == GiStatus::STARTED) {
    updateGiStatus(GiStatus::IN_PROGRESS);
  }
}

void HNZ::m_handleTMN(vector<Reading>& readings, const vector<unsigned char>& data) const {
  string msg_code = "TM";
  // If TMN can contain either 4 TMs of 8bits (TM8) or 2 TMs of 16bits (TM16)
  bool isTM8 = ((data[6] >> 7) == 1);
  unsigned int nbrTM = isTM8 ? 4 : 2;
  for (int i = 0; i < nbrTM; i++) {
    // 2 or 4 TM inside a TMn
    auto addressOffset = static_cast<unsigned char>(isTM8 ? i : i * 2); // For TM16 contains TMs with ADR+0 and ADR+2
    unsigned int msg_address = data[1] + addressOffset;
    string label = m_hnz_conf->getLabel(msg_code, msg_address);
    if (label.empty()) {
      continue;
    }

    long int value;
    unsigned int valid;

    if (isTM8) {
      int noctet = 2 + i;

      value = (data[noctet]);        // Vi
      valid = (data[6] >> i) & 0x1;  // Ii
    }
    else {
      int noctet = 2 + (i * 2);

      value = (data[noctet + 1] << 8 | data[noctet]); // Concat V1/V2 and V3/V4
      // Make negative values actual negatives in two's complement
      if ((value & 0x8000) > 0) {
        value &= 0x7FFF;
        value -= 32768;
      }
      valid = (data[6] >> (i * 2)) & 0x1;             // I1 or I3
    }

    ReadingParameters params;
    params.label = label;
    params.msg_code = msg_code;
    params.station_addr = m_remote_address;
    params.msg_address = msg_address;
    params.value = value;
    params.valid = valid;
    params.an = isTM8 ? "TM8" : "TM16";
    readings.push_back(m_prepare_reading(params));
  }
}

void HNZ::m_handleATVC(vector<Reading>& readings, const vector<unsigned char>& data) const {
  string msg_code = "TVC";

  unsigned int msg_address = data[1] & 0x1F;  // AD0

  m_hnz_connection->getActivePath()->receivedCommandACK("TVC", msg_address);

  string label = m_hnz_conf->getLabel(msg_code, msg_address);
  if (label.empty()) {
    return;
  }

  unsigned int a = (data[1] >> 6) & 0x1; // A
  long int value = data[2] & 0x7F;
  if (((data[3] >> 7) & 0x1) == 1) {
    value *= -1;  // S
  }

  ReadingParameters params;
  params.label = label;
  params.msg_code = msg_code;
  params.station_addr = m_remote_address;
  params.msg_address = msg_address;
  params.value = value;
  params.valid = a;
  readings.push_back(m_prepare_reading(params));
}

void HNZ::m_handleATC(vector<Reading>& readings, const vector<unsigned char>& data) const {
  string msg_code = "TC";

  unsigned int msg_address = stoi(to_string((int)data[1]) +
    to_string((int)(data[2] >> 5)));  // AD0 + ADB

  m_hnz_connection->getActivePath()->receivedCommandACK("TC", msg_address);

  string label = m_hnz_conf->getLabel(msg_code, msg_address);
  if (label.empty()) {
    return;
  }

  long int value = (data[2] >> 3) & 0x3;
  unsigned int CR = data[2] & 0x7;
  unsigned int valid = (CR == 0x1) ? 0 : 1;

  ReadingParameters params;
  params.label = label;
  params.msg_code = msg_code;
  params.station_addr = m_remote_address;
  params.msg_address = msg_address;
  params.value = value;
  params.valid = valid;
  readings.push_back(m_prepare_reading(params));
}

Reading HNZ::m_prepare_reading(const ReadingParameters& params) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::m_prepare_reading - ";
  bool isTS = (params.msg_code == "TS");
  bool isTSCE = isTS && !params.cg;
  bool isTM = (params.msg_code == "TM");
  std::string debugStr = beforeLog + "Send to fledge " + params.msg_code +
    " with station address = " + to_string(params.station_addr) +
    ", message address = " + to_string(params.msg_address) +
    ", value = " + to_string(params.value) + ", valid = " + to_string(params.valid);
  if (isTS) {
    debugStr += ", cg= " + to_string(params.cg);
  }
  if (isTM) {
    debugStr += ", an= " + params.an;
  }
  if (isTM || isTS) {
    debugStr += ", outdated= " + to_string(params.outdated);
    debugStr += ", qualityUpdate= " + to_string(params.qualityUpdate);
  }
  if (isTSCE) {
    debugStr += ", ts = " + to_string(params.ts) + ", iv = " + to_string(params.ts_iv) +
      ", c = " + to_string(params.ts_c) + ", s" + to_string(params.ts_s);
  }

  HnzUtility::log_debug(debugStr);

  auto* measure_features = new vector<Datapoint*>;
  measure_features->push_back(m_createDatapoint("do_type", params.msg_code));
  measure_features->push_back(m_createDatapoint("do_station", static_cast<long int>(params.station_addr)));
  measure_features->push_back(m_createDatapoint("do_addr", static_cast<long int>(params.msg_address)));
  // Do not send value when creating a quality update reading
  if (!params.qualityUpdate) {
    measure_features->push_back(m_createDatapoint("do_value", params.value));
  }
  measure_features->push_back(m_createDatapoint("do_valid", static_cast<long int>(params.valid)));

  if (isTM) {
    measure_features->push_back(m_createDatapoint("do_an", params.an));
  }
  if (isTS) {
    // Casting "bool" to "long int" result in true => 1 / false => 0
    measure_features->push_back(m_createDatapoint("do_cg", static_cast<long int>(params.cg)));
  }
  if (isTM || isTS) {
    // Casting "bool" to "long int" result in true => 1 / false => 0
    measure_features->push_back(m_createDatapoint("do_outdated", static_cast<long int>(params.outdated)));
  }
  if (isTSCE) {
    // Casting "unsigned long" into "long" for do_ts in order to match implementation of iec104 plugin
    measure_features->push_back(m_createDatapoint("do_ts", static_cast<long int>(params.ts)));
    measure_features->push_back(m_createDatapoint("do_ts_iv", static_cast<long int>(params.ts_iv)));
    measure_features->push_back(m_createDatapoint("do_ts_c", static_cast<long int>(params.ts_c)));
    measure_features->push_back(m_createDatapoint("do_ts_s", static_cast<long int>(params.ts_s)));
  }

  DatapointValue dpv(measure_features, true);

  Datapoint* dp = new Datapoint("data_object", dpv);

  return Reading(params.label, dp);
}

void HNZ::m_sendToFledge(vector<Reading>& readings) {
  for (Reading& reading : readings) {
    ingest(reading);
  }
}

void HNZ::ingest(Reading& reading) { 
  if (!m_ingest) {
    std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::ingest -";
    HnzUtility::log_error("%s Ingest callback is not defined", beforeLog.c_str());
    return;
  }
  (*m_ingest)(m_data, reading);
}

void HNZ::registerIngest(void* data, INGEST_CB cb) {
  m_ingest = cb;
  m_data = data;
}

/* Utility function used to print an array of PLUGIN_PARAMETER pointers in json format */
std::string paramsToStr(PLUGIN_PARAMETER** params, int count) {
  std::string out = "[";
  for (int i = 0; i < count; i++){
    if (i > 0) {
      out += ", ";
    }
    out += R"({"name": ")" + params[i]->name + R"(", "value": ")" + params[i]->value + R"("})";
  }
  out += "]";
  return out;
}

/* Utility function used to tell if a string ends with another string */
static bool endsWith(const std::string& str, const std::string& suffix)
{
  return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

bool HNZ::operation(const std::string& operation, int count, PLUGIN_PARAMETER** params) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::operation -";
  HnzUtility::log_info("%s Operation %s: %s", beforeLog.c_str(), operation.c_str(), paramsToStr(params, count).c_str());

  // Workaround until the following ticket is fixed: https://github.com/fledge-iot/fledge/issues/1239
  // if (operation == "HNZCommand") {
  if (endsWith(operation, "Command")) {
    if(processCommandOperation(count, params)) {
      // Only return on success so that all parameters are displayed by final error log in case of error
      return true;
    }
  }
  else if (operation == "request_connection_status") {
    HnzUtility::log_info("%s Received request_connection_status", beforeLog.c_str());
    m_sendConnectionStatus();
    return true;
  }

  HnzUtility::log_error("%s Unrecognised operation %s with %d parameters: %s", beforeLog.c_str(), operation.c_str(), count, paramsToStr(params, count).c_str());
  return false;
}

bool HNZ::processCommandOperation(int count, PLUGIN_PARAMETER** params) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::processCommandOperation -";
  
  std::map<std::string, std::string> commandParams = {
    {"co_type", ""},
    {"co_addr", ""},
    {"co_value", ""},
  };

  for (int i=0 ; i<count ; i++) {
    const std::string& paramName = params[i]->name;
    const std::string& paramValue = params[i]->value;
    if (commandParams.count(paramName) > 0) {
      commandParams[paramName] = paramValue;
      // Workaround until the following ticket is fixed: https://github.com/fledge-iot/fledge/issues/1240
      if(paramValue.at(0) == '"'){
        commandParams[paramName] = paramValue.substr(1,paramValue.length()-2);
      }
    }
    else {
      HnzUtility::log_warn("%s Unknown parameter '%s' in HNZCommand", beforeLog.c_str(), paramName.c_str());
    }
  }

  for (const auto &kvp : commandParams) {
    if (kvp.second == "") {
      HnzUtility::log_error("%s Received HNZCommand with missing '%s' parameter", beforeLog.c_str(), kvp.first.c_str());
      return false;
    }
  }

  const std::string& type = commandParams["co_type"];
  const std::string& addrStr = commandParams["co_addr"];
  const std::string& valStr = commandParams["co_value"];

  int address = 0;
  try {
    address = std::stoi(addrStr);
  } catch (const std::invalid_argument &e) {
    HnzUtility::log_error("%s Cannot convert co_addr '%s' to integer: %s: %s", beforeLog.c_str(), addrStr.c_str(), typeid(e).name(), e.what());
    return false;
  } catch (const std::out_of_range &e) {
    HnzUtility::log_error("%s Cannot convert co_addr '%s' to integer: %s: %s", beforeLog.c_str(), addrStr.c_str(), typeid(e).name(), e.what());
    return false;
  }

  int value = 0;
  try {
    value = std::stoi(valStr);
  } catch (const std::invalid_argument &e) {
    HnzUtility::log_error("%s Cannot convert co_value '%s' to integer: %s: %s", beforeLog.c_str(), valStr.c_str(), typeid(e).name(), e.what());
    return false;
  } catch (const std::out_of_range &e) {
    HnzUtility::log_error("%s Cannot convert co_value '%s' to integer: %s: %s", beforeLog.c_str(), valStr.c_str(), typeid(e).name(), e.what());
    return false;
  }

  if (type == "TC") {
    return m_hnz_connection->getActivePath()->sendTCCommand(static_cast<unsigned char>(address), static_cast<unsigned char>(value));
  }
  else if (type == "TVC") {
    return m_hnz_connection->getActivePath()->sendTVCCommand(static_cast<unsigned char>(address), value);
  }
  else {
    HnzUtility::log_error("%s Unknown co_type '%s' in HNZCommand", beforeLog.c_str(), type.c_str());
  }
  return false;
}

std::string HNZ::frameToStr(std::vector<unsigned char> frame) {
  std::stringstream stream;
  stream << "\n[";
  for (int i = 0; i < frame.size(); i++) {
    if (i > 0) {
      stream << ", ";
    }
    stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(frame[i]);
  }
  stream << "]";
  return stream.str();
}

unsigned long HNZ::getEpochMsTimestamp(std::chrono::time_point<std::chrono::system_clock> dateTime,
  unsigned char daySection, unsigned int ts)
{
  // Convert timestamp to epoch milliseconds
  static const unsigned long oneHourMs = 3600000; // 60 * 60 * 1000
  static const unsigned long oneDayMs = 86400000; // 24 * 60 * 60 * 1000
  static const unsigned long tenMinMs = 600000;   // 10 * 60 * 1000
  static const auto oneDay = std::chrono::hours{ 24 };
  // Get the date of the start of the day in epoch milliseconds
  auto days = dateTime - (dateTime.time_since_epoch() % oneDay);
  unsigned long epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(days.time_since_epoch()).count();
  // Add or remove one day if we are at edge of day and day section is on the other day
  long int ms_today = std::chrono::duration_cast<std::chrono::milliseconds>(dateTime.time_since_epoch()).count() % oneDayMs;
  long int hours = (ms_today / oneHourMs) % 24;
  // Remote section of day is after midnight but local clock is before midnight: add one day
  if ((daySection == 0) && (hours == 23)) {
    epochMs += oneDayMs;
  }
  // Remote section of day is before midnight but local clock is after midnight: remove one day
  if ((daySection == 143) && (hours == 0)) {
    epochMs -= oneDayMs;
  }
  // Add the time since day start (blocks of 10 min)
  epochMs += daySection * tenMinMs;
  // Add the time since section of day start (blocks of 10 ms)
  epochMs += ts * 10;
  return epochMs;
}

void HNZ::updateConnectionStatus(ConnectionStatus newState) {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex);
  if (m_connStatus == newState) return;

  m_connStatus = newState;

  // When connection lost, start timer to update all readings quality
  if (m_connStatus == ConnectionStatus::NOT_CONNECTED) {
    m_qualityUpdateTimer = m_qualityUpdateTimeoutMs;
  }
  else {
    m_qualityUpdateTimer = 0;
  }

  m_sendSouthMonitoringEvent(true, false);
}

void HNZ::updateGiStatus(GiStatus newState) {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex);
  if (m_giStatus == newState) return;

  m_giStatus = newState;

  m_sendSouthMonitoringEvent(false, true);
}

GiStatus HNZ::getGiStatus() {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex);
  return m_giStatus;
}

void HNZ::updateQualityUpdateTimer(long elapsedTimeMs) {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex);
  // If timer is running
  if (m_qualityUpdateTimer > 0) {
    m_qualityUpdateTimer -= elapsedTimeMs;
    // If timer expired, update quality of all TM and TS to outdated
    if (m_qualityUpdateTimer <= 0) {
      m_qualityUpdateTimer = 0;
      m_sendAllTMQualityReadings(false, true);
      m_sendAllTSQualityReadings(false, true);
    }
  }
}

void HNZ::m_sendConnectionStatus() {
  m_sendSouthMonitoringEvent(true, true);
}

void HNZ::m_sendSouthMonitoringEvent(bool connxStatus, bool giStatus) {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex);
  std::string asset = m_hnz_conf->get_connx_status_signal();
  if (asset.empty()) return;

  if ((connxStatus == false) && (giStatus == false)) return;

  auto* attributes = new vector<Datapoint*>;

  if (connxStatus) {
    Datapoint* eventDp = nullptr;

    switch (m_connStatus)
    {
    case ConnectionStatus::NOT_CONNECTED:
      eventDp = m_createDatapoint("connx_status", "not connected");
      break;

    case ConnectionStatus::STARTED:
      eventDp = m_createDatapoint("connx_status", "started");
      break;
    }

    if (eventDp) {
      attributes->push_back(eventDp);
    }
  }

  if (giStatus) {
    Datapoint* eventDp = nullptr;

    switch (m_giStatus)
    {
    case GiStatus::STARTED:
      eventDp = m_createDatapoint("gi_status", "started");
      break;

    case GiStatus::IN_PROGRESS:
      eventDp = m_createDatapoint("gi_status", "in progress");
      break;

    case GiStatus::FAILED:
      eventDp = m_createDatapoint("gi_status", "failed");
      break;

    case GiStatus::FINISHED:
      eventDp = m_createDatapoint("gi_status", "finished");
      break;

    case GiStatus::IDLE:
      eventDp = m_createDatapoint("gi_status", "idle");
      break;
    }

    if (eventDp) {
      attributes->push_back(eventDp);
    }
  }

  DatapointValue dpv(attributes, true);

  auto* southEvent = new Datapoint("south_event", dpv);
  std::vector<Reading> status_readings = { Reading(asset, southEvent) };
  m_sendToFledge(status_readings);
}

void HNZ::GICompleted(bool success) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::GICompleted -";
  m_hnz_connection->onGICompleted();
  if (success) {
    HnzUtility::log_info("%s General Interrogation completed.", beforeLog.c_str());
    m_sendAllTSQualityReadings(true, false, m_gi_addresses_received);
    updateGiStatus(GiStatus::FINISHED);
  }
  else {
    HnzUtility::log_error("%s General Interrogation FAILED !", beforeLog.c_str());
    m_sendAllTSQualityReadings(true, false, m_gi_addresses_received);
    updateGiStatus(GiStatus::FAILED);
  }
  resetGIQueue();
}

void HNZ::sendInitialGI() {
  m_hnz_connection->sendInitialGI();
}

void HNZ::m_sendAllTMQualityReadings(bool invalid, bool outdated, const vector<unsigned int>& rejectFilter /*= {}*/) {
  ReadingParameters paramsTemplate;
  paramsTemplate.msg_code = "TM";
  paramsTemplate.station_addr = m_remote_address;
  paramsTemplate.valid = static_cast<unsigned int>(invalid);
  paramsTemplate.outdated = outdated;
  paramsTemplate.an = "TMA";
  paramsTemplate.qualityUpdate = true;
  m_sendAllTIQualityReadings(paramsTemplate, rejectFilter);
}

void HNZ::m_sendAllTSQualityReadings(bool invalid, bool outdated, const vector<unsigned int>& rejectFilter /*= {}*/) {
  ReadingParameters paramsTemplate;
  unsigned long epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  paramsTemplate.msg_code = "TS";
  paramsTemplate.station_addr = m_remote_address;
  paramsTemplate.valid = static_cast<unsigned int>(invalid);
  paramsTemplate.outdated = outdated;
  paramsTemplate.cg = false;
  paramsTemplate.ts = epochMs;
  paramsTemplate.qualityUpdate = true;
  m_sendAllTIQualityReadings(paramsTemplate, rejectFilter);
}

void HNZ::m_sendAllTIQualityReadings(const ReadingParameters& paramsTemplate, const vector<unsigned int>& rejectFilter /*= {}*/) {
  set<unsigned int> hashFilter(rejectFilter.begin(), rejectFilter.end());
  vector<Reading> readings;
  const auto& allMessages = m_hnz_conf->get_all_messages();
  const auto& allTMs = allMessages.at(paramsTemplate.msg_code).at(paramsTemplate.station_addr);
  for (auto const& kvp : allTMs) {
    unsigned int msg_address = kvp.first;
    // Skip messages that are part of the reject filter
    if (hashFilter.count(msg_address) > 0) continue;
    // Complete the reading param infos
    ReadingParameters params(paramsTemplate);
    params.label = kvp.second;
    params.msg_address = msg_address;
    readings.push_back(m_prepare_reading(params));
  }
  if (!readings.empty()) {
    m_sendToFledge(readings);
  }
}

HNZ::AddressesDiff HNZ::m_getMismatchingTSCGAddresses() const {
  std::set<unsigned int> missingAddresses;
  std::set<unsigned int> extraAddresses;
  // Fill missingAddresses with all known addresses
  const auto& allMessages = m_hnz_conf->get_all_messages();
  const auto& allTSs = allMessages.at("TS").at(m_remote_address);
  for (auto const& kvp : allTSs) {
    unsigned int msg_address = kvp.first;
    missingAddresses.insert(msg_address);
  }
  // Remove addresses received in missingAddresses / store unknown addresses in extraAddresses
  for(auto address: m_gi_addresses_received) {
    if(missingAddresses.count(address) == 0) {
      extraAddresses.insert(address);
    } else {
      missingAddresses.erase(address);
    }
  }
  return AddressesDiff{
    std::vector<unsigned int>(missingAddresses.begin(), missingAddresses.end()),
    std::vector<unsigned int>(extraAddresses.begin(), extraAddresses.end())};
}