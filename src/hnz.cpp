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
    stop(true);
  }
}

void HNZ::start(bool requestedStart /*= false*/) {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex); //LCOV_EXCL_LINE
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::start -"; //LCOV_EXCL_LINE

  if (requestedStart) {
    m_should_run = true;
  }

  if (m_is_running) {
    HnzUtility::log_info("%s HNZ south plugin already started", beforeLog.c_str()); //LCOV_EXCL_LINE
    return;
  }

  if (!m_hnz_conf->is_complete()) {
    HnzUtility::log_info("%s HNZ south plugin can't start because configuration is incorrect.", beforeLog.c_str()); //LCOV_EXCL_LINE
    return;
  }

  HnzUtility::log_info("%s Starting HNZ south plugin...", beforeLog.c_str()); //LCOV_EXCL_LINE

  m_is_running = true;

  m_sendAllTMQualityReadings(true, false);
  m_sendAllTSQualityReadings(true, false);

  auto paths = m_hnz_connection->getPaths();
  m_receiving_thread_A = uniq::make_unique<thread>(&HNZ::receive, this, paths[0]);
  if (paths[1] != nullptr) {
    // Wait after getting the passive path pointer as connection init of active path may swap path
    this_thread::sleep_for(std::chrono::milliseconds(1000));
    // Path B is defined in the configuration
    m_receiving_thread_B = uniq::make_unique<thread>(&HNZ::receive, this, paths[1]);
  }

  m_hnz_connection->start();
}

void HNZ::stop(bool requestedStop /*= false*/) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::stop -"; //LCOV_EXCL_LINE
  HnzUtility::log_info("%s Starting shutdown of HNZ plugin", beforeLog.c_str()); //LCOV_EXCL_LINE
  m_is_running = false;

  if (requestedStop) {
    m_should_run = false;
  }

  // Connection must be stopped before management threads of both path
  // or join on both receive threads will hang forever
  if (m_hnz_connection != nullptr) {
    m_hnz_connection->stop();
  }
  if (m_receiving_thread_A != nullptr) {
    HnzUtility::log_debug("%s Waiting for the receiving thread (path A)", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_receiving_thread_A->join();
    m_receiving_thread_A = nullptr;
  }
  if (m_receiving_thread_B != nullptr) {
    HnzUtility::log_debug("%s Waiting for the receiving thread (path B)", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_receiving_thread_B->join();
    m_receiving_thread_B = nullptr;
  }
  // Connection must be freed after management threads of both path
  // as HNZ::m_hnz_connection and paths of HNZConnection
  // are used in HNZ::receive running on the threads
  if (m_hnz_connection != nullptr) {
    m_hnz_connection = nullptr;
  }
  HnzUtility::log_info("%s Plugin stopped !", beforeLog.c_str()); //LCOV_EXCL_LINE
}

void HNZ::reconfigure(const ConfigCategory& config) {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex); //LCOV_EXCL_LINE
  std::string protocol_conf_json;
  if (config.itemExists("protocol_stack")) {
    protocol_conf_json = config.getValue("protocol_stack");
  }
  std::string msg_conf_json;
  if (config.itemExists("exchanged_data")) {
    msg_conf_json = config.getValue("exchanged_data");
  }
  std::string service_name = config.getName();

  bool success = setJsonConfig(protocol_conf_json, msg_conf_json, service_name);
  if (!success) {
    // Resinitialize all connection state audits in case of failed configuration
    HnzUtility::audit_info("SRVFL", m_service_name + "-A-unused");
    HnzUtility::audit_info("SRVFL", m_service_name + "-B-unused");
    HnzUtility::audit_fail("SRVFL", m_service_name + "-disconnected");
  }
}

bool HNZ::setJsonConfig(const string& protocol_conf_json, const string& msg_conf_json, const string& service_name) {
  std::lock_guard<std::recursive_mutex> guard(m_configMutex); //LCOV_EXCL_LINE
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::setJsonConfig -"; //LCOV_EXCL_LINE
  // If no new json configuration and the plugin is already in the correct running state, nothing to do
  if (protocol_conf_json.empty() && msg_conf_json.empty() && service_name.empty()) {
    HnzUtility::log_info("%s No new configuration provided to reconfigure, skipping", beforeLog.c_str()); //LCOV_EXCL_LINE
    return true;
  }

  if (m_is_running) {
    HnzUtility::log_info("%s Configuration change requested, stopping the plugin", beforeLog.c_str()); //LCOV_EXCL_LINE
    stop();
  }

  if (!service_name.empty()) {
    m_service_name = service_name;
  }

  HnzUtility::log_info("%s Reading json config string...", beforeLog.c_str()); //LCOV_EXCL_LINE

  // Reset configuration info
  m_hnz_conf = std::make_shared<HNZConf>();
  if (!protocol_conf_json.empty()) {
    m_hnz_conf->importConfigJson(protocol_conf_json);
  }
  if (!msg_conf_json.empty()) {
    m_hnz_conf->importExchangedDataJson(msg_conf_json);
  }
  if (!m_hnz_conf->is_complete()) {
    HnzUtility::log_fatal("%s Unable to set Plugin configuration due to error with the json conf", beforeLog.c_str()); //LCOV_EXCL_LINE
    return false;
  }

  HnzUtility::log_info("%s Json config parsed successsfully.", beforeLog.c_str()); //LCOV_EXCL_LINE

  m_remote_address = m_hnz_conf->get_remote_station_addr();
  m_test_msg_receive = m_hnz_conf->get_test_msg_receive();
  m_hnz_connection = uniq::make_unique<HNZConnection>(m_hnz_conf, this);
  if (!m_hnz_connection->isRunning()) {
    HnzUtility::log_fatal("%s Unable to start HNZ Connection", beforeLog.c_str()); //LCOV_EXCL_LINE
    return false;
  }

  if (m_should_run) {
    HnzUtility::log_info("%s Restarting the plugin...", beforeLog.c_str()); //LCOV_EXCL_LINE
    start();
  }

  return true;
}

void HNZ::receive(std::shared_ptr<HNZPath> hnz_path_in_use) {
  if(!hnz_path_in_use){
    HnzUtility::log_info(HnzUtility::NamePlugin + " - HNZ::receive - No path to use, exit"); //LCOV_EXCL_LINE
    return;
  }
  {
    std::lock_guard<std::recursive_mutex> guard(m_configMutex); //LCOV_EXCL_LINE
    if (!m_hnz_conf->is_complete()) {
      return;
    }
  }

  string path = hnz_path_in_use->getName();
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::receive - " + path; //LCOV_EXCL_LINE

  // Connect to the server
  hnz_path_in_use->connect();

  // Exit early if connection shutting down
  if(!m_is_running) {
    HnzUtility::log_info("%s Connection shutting down, exit", beforeLog.c_str()); //LCOV_EXCL_LINE
    return;
  }

  HnzUtility::log_info("%s Listening for data...", beforeLog.c_str()); //LCOV_EXCL_LINE

  vector<vector<unsigned char>> messages;

  while (m_is_running) {
    // Waiting for data
    messages = hnz_path_in_use->getData();

    if (messages.empty() && !hnz_path_in_use->isTCPConnected()) {
      // Refresh beforeLog as path state will change over time
      beforeLog = HnzUtility::NamePlugin + " - HNZ::receive - " + hnz_path_in_use->getName();
      HnzUtility::log_warn("%s Connection lost, reconnecting path.", beforeLog.c_str()); //LCOV_EXCL_LINE
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
  std::lock_guard<std::recursive_mutex> guard(m_configMutex); //LCOV_EXCL_LINE
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::m_handle_message -"; //LCOV_EXCL_LINE
  unsigned char t = data[0];  // Payload type
  vector<Reading> readings;   // Contains data object to push to fledge
  
  switch (t) {
  case MODULO_CODE:
    HnzUtility::log_info("%s Received modulo time update", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleModuloCode(readings, data);
    break; //LCOV_EXCL_LINE
  case TM4_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TMA", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleTM4(readings, data);
    break; //LCOV_EXCL_LINE
  case TSCE_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TSCE", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleTSCE(readings, data);
    break; //LCOV_EXCL_LINE
  case TSCG_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TSCG", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleTSCG(readings, data);
    break; //LCOV_EXCL_LINE
  case TMN_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TMN", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleTMN(readings, data);
    break; //LCOV_EXCL_LINE
  case TCACK_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TC ACK", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleATC(readings, data);
    break; //LCOV_EXCL_LINE
  case TVCACK_CODE:
    HnzUtility::log_info("%s Pushing to Fledge a TVC ACK", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_handleATVC(readings, data);
    break; //LCOV_EXCL_LINE
  default:
    if (!(t == m_test_msg_receive.first &&
      data[1] == m_test_msg_receive.second)) {
      HnzUtility::log_error("%s Unknown message to push: %s", beforeLog.c_str(), frameToStr(data).c_str()); //LCOV_EXCL_LINE
    }
    break; //LCOV_EXCL_LINE
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
        HnzUtility::log_warn("%s Received last TSCG but %lu TS received when %lu were expected: Missing %s, Extra %s", //LCOV_EXCL_LINE
                            beforeLog.c_str(), m_gi_addresses_received.size(), nbTSCG, //LCOV_EXCL_LINE
                            formatAddresses(TSAddressesDiff.missingAddresses).c_str(), //LCOV_EXCL_LINE
                            formatAddresses(TSAddressesDiff.extraAddresses).c_str()); //LCOV_EXCL_LINE
      }
      m_hnz_connection->checkGICompleted(true);
    }
  }
}

void HNZ::m_handleModuloCode(vector<Reading>& readings, const vector<unsigned char>& data) {
  // No reading to send when reciving modulo code, but keep the parameter
  // to get a homogenous signature for all m_handle*() methods
  readings.clear();
  setDaySection(data[1]);
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

void HNZ::m_handleTSCE(vector<Reading>& readings, const vector<unsigned char>& data) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::m_handleTSCE - "; //LCOV_EXCL_LINE
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

  auto now_timepoint = std::chrono::high_resolution_clock::now();
  long int ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now_timepoint.time_since_epoch()).count();
  // Using C time (C++11 limitations conversion to local time)
  time_t now = static_cast<time_t>(ms_since_epoch / 1000);
  auto time_struct_real = tm();
  m_hnz_conf->get_use_utc() ? gmtime_r(&now, &time_struct_real) : localtime_r(&now, &time_struct_real);
  auto epochMs = getEpochMsTimestamp(now_timepoint, m_daySection, ts);

  // Always timestamp with UTC time --------
  auto time_struct_utc = tm();
  gmtime_r(&now, &time_struct_utc);
  auto real_utc_offset_s =
    (time_struct_real.tm_hour * 3600 + time_struct_real.tm_min * 60 + time_struct_real.tm_sec) -
    (time_struct_utc.tm_hour * 3600 + time_struct_utc.tm_min * 60 + time_struct_utc.tm_sec);
  // ----------------------------------------

  ReadingParameters params;
  params.label = label;
  params.msg_code = msg_code;
  params.station_addr = m_remote_address;
  params.msg_address = msg_address;
  params.value = value;
  params.valid = valid;
  params.ts = epochMs - real_utc_offset_s * 1000;
  params.ts_iv = ts_iv;
  params.ts_c = ts_c;
  params.ts_s = ts_s;
  params.cg = false;

  // In stateINPUT_CONNECTED, timestamp mod10 might be uninitialized
  if(m_hnz_connection->getActivePath() != nullptr && m_hnz_connection->getActivePath()->getProtocolState() == ProtocolState::INPUT_CONNECTED){
    HnzUtility::log_info("%s TSCE discarded in path protocol state INPUT_CONNECTED.", beforeLog.c_str()); //LCOV_EXCL_LINE
    params.empty_timestamp = true;
  }

  readings.push_back(m_prepare_reading(params));

  int isTsTriggering = m_hnz_conf->isTsAddressCgTriggering(msg_address);
  if ( isTsTriggering != -1) {
    if (params.value == isTsTriggering) {
      if(m_hnz_connection->getActivePath() == nullptr) {
        HnzUtility::log_debug(beforeLog + "GI triggering TS received but no active path available => GI skipped"); //LCOV_EXCL_LINE
      }
      else {
        if (m_giStatus == GiStatus::STARTED || m_giStatus == GiStatus::IN_PROGRESS) {
          HnzUtility::log_debug(beforeLog + "GI triggering TS received but GI already running, scheduling another GI after the current one"); //LCOV_EXCL_LINE
          m_giInQueue = true;
        } else {
          HnzUtility::log_info(beforeLog + "GI triggering TS received, start a GI"); //LCOV_EXCL_LINE
          m_hnz_connection->getActivePath()->sendGeneralInterrogation();
        }
      }
    }
  }
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

  // Acknowledge the TVC on the path from which it was sent
  // Unexpected partial disconnection of the active path can generate ill states if the message is not acknowledged
  for (auto& path: m_hnz_connection->getPaths())
  {
    if(path == nullptr) continue;
    path->receivedCommandACK("TVC", msg_address);
  }

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

  // Acknowledge the TC on the path from which it was sent
  // Unexpected partial disconnection of the active path can generate ill states if the message is not acknowledged
  for (auto& path: m_hnz_connection->getPaths())
  {
    if(path == nullptr) continue;
    path->receivedCommandACK("TC", msg_address);
  }

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
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::m_prepare_reading - "; //LCOV_EXCL_LINE
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

  HnzUtility::log_debug(debugStr); //LCOV_EXCL_LINE

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
    if(!params.empty_timestamp) measure_features->push_back(m_createDatapoint("do_ts", static_cast<long int>(params.ts)));
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
    std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::ingest -"; //LCOV_EXCL_LINE
    HnzUtility::log_error("%s Ingest callback is not defined", beforeLog.c_str()); //LCOV_EXCL_LINE
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
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::operation -"; //LCOV_EXCL_LINE
  HnzUtility::log_info("%s Operation %s: %s", beforeLog.c_str(), operation.c_str(), paramsToStr(params, count).c_str()); //LCOV_EXCL_LINE

  if (operation == "HNZCommand") {
    int res = processCommandOperation(count, params);
    if(res == 0) {
      // Only return on success so that all parameters are displayed by final error log in case of syntax error
      return true;
    }
    else if (res == 2) {
      // Network issue, only log a warning
      HnzUtility::log_warn("%s Connection with HNZ device is not ready, could not send operation %s: %s", beforeLog.c_str(), operation.c_str(), paramsToStr(params, count).c_str()); //LCOV_EXCL_LINE
      return false;
    }
  }
  else if (operation == "request_connection_status") {
    HnzUtility::log_info("%s Received request_connection_status", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_sendConnectionStatus();
    return true;
  } else if (operation == "north_status") {
    // The north service notifies its status and awaits a response in mode "accept_if_south_connx_started".
    // A GI is triggered once the north service is fully started.
    if(count != 1) {
      HnzUtility::log_error("%s Invalid number of parameters for north_status operation: %d", beforeLog.c_str(), count); //LCOV_EXCL_LINE
      return false;
    }
    if (params[0]->value == "init_socket_finished") {
      if (m_hnz_connection->getActivePath() == nullptr) {
        HnzUtility::log_debug("%s Received operation \"init_socket_finished\" but no active path available to request GI => GI skipped", beforeLog.c_str()); //LCOV_EXCL_LINE
        return false;
      }
      if (m_giStatus == GiStatus::STARTED || m_giStatus == GiStatus::IN_PROGRESS) {
        HnzUtility::log_debug("%s Received operation \"init_socket_finished\" but a GI is already running, scheduling another GI after the current one", beforeLog.c_str()); //LCOV_EXCL_LINE
        m_giInQueue = true;
      } else {
        HnzUtility::log_info("%s Received operation \"init_socket_finished\", start a GI", beforeLog.c_str()); //LCOV_EXCL_LINE
        m_hnz_connection->getActivePath()->sendGeneralInterrogation();
      }
      return true;
    } else {
      HnzUtility::log_error("%s Unrecognised parameter for north_status operation: %s", beforeLog.c_str(), params[0]->value.c_str()); //LCOV_EXCL_LINE
      return false;
    }
  }

  HnzUtility::log_error("%s Unrecognised operation %s with %d parameters: %s", beforeLog.c_str(), operation.c_str(), count, paramsToStr(params, count).c_str()); //LCOV_EXCL_LINE
  return false;
}

int HNZ::processCommandOperation(int count, PLUGIN_PARAMETER** params) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::processCommandOperation -"; //LCOV_EXCL_LINE
  
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
    }
    else {
      HnzUtility::log_warn("%s Unknown parameter '%s' in HNZCommand", beforeLog.c_str(), paramName.c_str()); //LCOV_EXCL_LINE
    }
  }

  for (const auto &kvp : commandParams) {
    if (kvp.second == "") {
      HnzUtility::log_error("%s Received HNZCommand with missing '%s' parameter", beforeLog.c_str(), kvp.first.c_str()); //LCOV_EXCL_LINE
      return 1;
    }
  }

  const std::string& type = commandParams["co_type"];
  const std::string& addrStr = commandParams["co_addr"];
  const std::string& valStr = commandParams["co_value"];

  int address = 0;
  try {
    address = std::stoi(addrStr);
  } catch (const std::invalid_argument &e) {
    HnzUtility::log_error("%s Cannot convert co_addr '%s' to integer: %s: %s", beforeLog.c_str(), addrStr.c_str(), typeid(e).name(), e.what()); //LCOV_EXCL_LINE
    return 1;
  } catch (const std::out_of_range &e) {
    HnzUtility::log_error("%s Cannot convert co_addr '%s' to integer: %s: %s", beforeLog.c_str(), addrStr.c_str(), typeid(e).name(), e.what()); //LCOV_EXCL_LINE
    return 1;
  }

  int value = 0;
  try {
    value = std::stoi(valStr);
  } catch (const std::invalid_argument &e) {
    HnzUtility::log_error("%s Cannot convert co_value '%s' to integer: %s: %s", beforeLog.c_str(), valStr.c_str(), typeid(e).name(), e.what()); //LCOV_EXCL_LINE
    return 1;
  } catch (const std::out_of_range &e) {
    HnzUtility::log_error("%s Cannot convert co_value '%s' to integer: %s: %s", beforeLog.c_str(), valStr.c_str(), typeid(e).name(), e.what()); //LCOV_EXCL_LINE
    return 1;
  }

  if(m_hnz_connection->getActivePath() == nullptr){
    return 2;
  }

  if (type == "TC") {
    bool success = m_hnz_connection->getActivePath()->sendTCCommand(address, static_cast<unsigned char>(value));
    return success ? 0 : 2;
  }
  else if (type == "TVC") {
    bool success = m_hnz_connection->getActivePath()->sendTVCCommand(static_cast<unsigned char>(address), value);
    return success ? 0 : 2;
  }
  else {
    HnzUtility::log_error("%s Unknown co_type '%s' in HNZCommand", beforeLog.c_str(), type.c_str()); //LCOV_EXCL_LINE
  }
  return 1;
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
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex); //LCOV_EXCL_LINE
  std::string newStateSTR = newState == ConnectionStatus::NOT_CONNECTED ? "NOT CONNECTED" : "STARTED";
  std::string m_connStatusSTR = m_connStatus == ConnectionStatus::NOT_CONNECTED ? "NOT CONNECTED" : "STARTED";
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

  // Send audit for connection status
  if (m_connStatus == ConnectionStatus::STARTED) {
    HnzUtility::audit_success("SRVFL", m_service_name + "-connected");
  }
  else {
    HnzUtility::audit_fail("SRVFL", m_service_name + "-disconnected");
  }
}

void HNZ::updateGiStatus(GiStatus newState) {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex); //LCOV_EXCL_LINE
  if (m_giStatus == newState) return;

  m_giStatus = newState;

  m_sendSouthMonitoringEvent(false, true);
}

GiStatus HNZ::getGiStatus() {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex); //LCOV_EXCL_LINE
  return m_giStatus;
}

void HNZ::updateQualityUpdateTimer(long elapsedTimeMs) {
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex); //LCOV_EXCL_LINE
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
  std::lock_guard<std::recursive_mutex> lock(m_connexionGiMutex); //LCOV_EXCL_LINE
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
      break; //LCOV_EXCL_LINE

    case ConnectionStatus::STARTED:
      eventDp = m_createDatapoint("connx_status", "started");
      break; //LCOV_EXCL_LINE
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
      break; //LCOV_EXCL_LINE

    case GiStatus::IN_PROGRESS:
      eventDp = m_createDatapoint("gi_status", "in progress");
      break; //LCOV_EXCL_LINE

    case GiStatus::FAILED:
      eventDp = m_createDatapoint("gi_status", "failed");
      break; //LCOV_EXCL_LINE

    case GiStatus::FINISHED:
      eventDp = m_createDatapoint("gi_status", "finished");
      break; //LCOV_EXCL_LINE

    case GiStatus::IDLE:
      eventDp = m_createDatapoint("gi_status", "idle");
      break; //LCOV_EXCL_LINE
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
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZ::GICompleted -"; //LCOV_EXCL_LINE
  m_hnz_connection->onGICompleted();
  if (success) {
    HnzUtility::log_info("%s General Interrogation completed.", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_sendAllTSQualityReadings(true, false, m_gi_addresses_received);
    updateGiStatus(GiStatus::FINISHED);
  }
  else {
    HnzUtility::log_warn("%s General Interrogation FAILED !", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_sendAllTSQualityReadings(true, false, m_gi_addresses_received);
    updateGiStatus(GiStatus::FAILED);
  }
  resetGIQueue();

  if (m_giInQueue) {
    HnzUtility::log_info("%s Starting delayed GI", beforeLog.c_str()); //LCOV_EXCL_LINE
    m_hnz_connection->getActivePath()->sendGeneralInterrogation();
    m_giInQueue = false;
  }
}

#ifdef UNIT_TEST
void HNZ::sendInitialGI() {
  m_hnz_connection->sendInitialGI();
}
#endif

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