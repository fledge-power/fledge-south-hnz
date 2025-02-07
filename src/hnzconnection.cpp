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
#include "hnz.h"
#include "hnzpath.h"
#include "hnzconnection.h"

HNZConnection::HNZConnection(std::shared_ptr<HNZConf> hnz_conf, HNZ* hnz_fledge) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::HNZConnection -";
  
  this->m_hnz_conf = hnz_conf;
  this->m_hnz_fledge = hnz_fledge;

  bool ip_configured = false;
  for(auto& ip: m_hnz_conf->get_paths_ip()){
    if(ip != ""){
      ip_configured = true;
      break;
    }
  }
  if(!ip_configured) {
    HnzUtility::log_fatal("%s Attempted to start HNZ connection with no IP configured, aborting", beforeLog.c_str());
    return;
  }

  // Create the path needed
  for (int i = 0; i < MAXPATHS; i++)
  {
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    std::string pathLetter = i == 0 ? "A" : "B";
    if (m_hnz_conf->get_paths_ip()[i] != "") {
      m_paths[i] = new HNZPath(
                        m_hnz_conf,
                        this,
                        m_hnz_conf->get_paths_repeat()[i],
                        m_hnz_conf->get_paths_ip()[i],
                        m_hnz_conf->get_paths_port()[i],
                        pathLetter);
    }
    else {
      // Send initial path connection status audit
      HnzUtility::audit_info("SRVFL", hnz_fledge->getServiceName() + "-" + pathLetter + "-unused");
    }
  }

  // Send initial connection status audit
  HnzUtility::audit_fail("SRVFL", hnz_fledge->getServiceName() + "-disconnected");

  // Set settings for GI
  this->m_gi_repeat_count_max = m_hnz_conf->get_gi_repeat_count();
  this->m_gi_time_max = m_hnz_conf->get_gi_time() * 1000;
  this->m_gi_schedule = m_hnz_conf->get_gi_schedule();
  this->m_gi_schedule_already_sent = false;

  // Others settings
  this->m_repeat_timeout = m_hnz_conf->get_repeat_timeout();
  this->m_is_running = true;
}

HNZConnection::~HNZConnection() {
  if (m_is_running) {
    stop();
  }

  for (HNZPath* path: m_paths)
  {
    delete path;
  }
}

void HNZConnection::start() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::start -";
  HnzUtility::log_debug("%s HNZ Connection starting...", beforeLog.c_str());
  m_messages_thread = std::make_shared<std::thread>(&HNZConnection::m_manageMessages, this);
}

void HNZConnection::stop() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::stop -";
  HnzUtility::log_debug("%s HNZ Connection stopping...", beforeLog.c_str());
  // Stop the thread that manage the messages
  m_is_running = false;

  // Stop the path used (close the TCP connection and stop the threads that
  // manage HNZ connections)
  {
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    for (HNZPath* path: m_paths)
    {
      if(path != nullptr){
        path->disconnect();
      }
    }
  }

  // Wait for the end of the thread that manage the messages
  if (m_messages_thread != nullptr) {
    std::shared_ptr<std::thread> temp = m_messages_thread;
    m_messages_thread = nullptr;
    HnzUtility::log_debug("%s Waiting for the messages managing thread", beforeLog.c_str());
    temp->join();
  }

  HnzUtility::log_info("%s HNZ Connection stopped !", beforeLog.c_str());
}

void HNZConnection::checkGICompleted(bool success) { 
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::checkGICompleted -";
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  
  // GI is a success
  if (success) {
    m_hnz_fledge->GICompleted(true);
    return;
  }
  // GI not completed in time or last TS received with other missing TS
  if (m_gi_repeat > m_gi_repeat_count_max) {
    // GI failed
    HnzUtility::log_warn("%s Maximum GI repeat reached (%d)", beforeLog.c_str(), m_gi_repeat_count_max);
    m_hnz_fledge->GICompleted(false);
  } else {
    HnzUtility::log_warn("%s General Interrogation Timeout, repeat GI", beforeLog.c_str());
    // Clean queue in HNZ class
    m_hnz_fledge->resetGIQueue();

    if(!m_active_path){
      HnzUtility::log_warn("%s No active path was found to send a GI.", beforeLog.c_str());
      return;
    }
    // Send a new GI
    m_active_path->sendGeneralInterrogation();
  }
}

void HNZConnection::onGICompleted() { 
  m_gi_repeat = 0;
}

void HNZConnection::notifyGIsent(){
  if ((m_gi_repeat == 0) || (getGiStatus() != GiStatus::IN_PROGRESS)) {
    updateGiStatus(GiStatus::STARTED);
  }
  m_gi_repeat++;
  m_gi_start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now().time_since_epoch())
                      .count();
}

void HNZConnection::m_manageMessages() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::m_manageMessages -";
  m_update_current_time();

  m_days_since_epoch = m_current / 86400000;
  int today_ms = m_current % 86400000;
  m_gi_scheduled_time = m_current - today_ms +
                        ((m_gi_schedule.hour * 60 + m_gi_schedule.min) * 60000);
  if (m_gi_schedule.activate) {
    // If GI schedule is activated, check if time scheduled is outdated
    HnzUtility::log_info("%s GI scheduled is set at %dh%d.", beforeLog.c_str(), m_gi_schedule.hour, m_gi_schedule.min);
    if (m_current >= m_gi_scheduled_time) {
      HnzUtility::log_info("%s No GI for today because the scheduled time has already passed.", beforeLog.c_str());
      m_gi_schedule_already_sent = true;
    } else {
      HnzUtility::log_info("%s GI scheduled is set for today", beforeLog.c_str());
    }
  }

  do {
    m_update_current_time();

    // Manage repeat/timeout for each path
    {
      std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
      for (HNZPath* path: m_paths)
      {
        if(path != nullptr){
          m_check_timer(path);

          // Manage command ACK
          m_check_command_timer(path);
        }
      }
    }

    // GI support
    m_check_GI();

    // Manage quality update
    m_update_quality_update_timer();

    this_thread::sleep_for(std::chrono::milliseconds(100));
  } while (m_is_running);
}

void HNZConnection::m_check_timer(HNZPath* path) {
  if(path == nullptr) return;

  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::m_check_timer - " + path->getName();
  if (!path->msg_sent.empty() && path->isTCPConnected()) {
    Message& msg = path->msg_sent.front();
    if (path->last_sent_time + m_repeat_timeout < m_current) {
      HnzUtility::log_debug("%s last_sent_time=%lld, m_repeat_timeout=%d, m_current=%llu", beforeLog.c_str(), path->last_sent_time, m_repeat_timeout, m_current);
      if (path->getRepeat() >= path->repeat_max) {
        // Connection disrupted, back to SARM
        HnzUtility::log_warn("%s Connection disrupted, back to SARM", beforeLog.c_str());

        path->protocolStateTransition(ConnectionEvent::MAX_SEND);
      } else {
        // Repeat the message
        HnzUtility::log_warn("%s Timeout, sending back first unacknowledged message", beforeLog.c_str());
        path->sendBackInfo(msg);

        // Move other unacknowledged messages to waiting queue
        while (path->msg_sent.size() > 1) {
          path->msg_waiting.push_front(path->msg_sent.back());
          path->msg_sent.pop_back();
        }
      }
    }
  }

  if(path->getProtocolState() == ProtocolState::CONNECTED){
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    long long ms_since_connected = now - path->getLastConnected();
    if(path->getLastConnected() > 0 && ms_since_connected >= m_repeat_timeout){
      if(path->getConnectionState() == ConnectionState::ACTIVE && m_sendInitNexConnection){
        HnzUtility::log_debug("%s Sending init messages", beforeLog.c_str());
        path->sendInitMessages();
        m_sendInitNexConnection = false;
      } else {
        HnzUtility::log_debug("%s Discarding init messages", beforeLog.c_str());
        path->resetLastConnected();
      }
    }
  }
}

void HNZConnection::m_check_GI() {
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::m_check_GI -";
  // Check the status of an ongoing GI
  if (m_gi_repeat != 0) {
    if (m_gi_start_time + m_gi_time_max < m_current) {
      HnzUtility::log_warn("%s GI timeout (%d ms)", beforeLog.c_str(), m_gi_time_max);
      checkGICompleted(false);
    }
  }

  // Detect new days to set the next scheduled GI
  if (m_days_since_epoch != (m_current / 86400000)) {
    m_gi_schedule_already_sent = false;
    m_gi_scheduled_time =
        m_current - (m_current % 86400000) +
        ((m_gi_schedule.hour * 60 + m_gi_schedule.min) * 60000);

    // Update the current day
    m_days_since_epoch = m_current / 86400000;
  }

  // Scheduled GI
  if (m_gi_schedule.activate && !m_gi_schedule_already_sent &&
      (m_gi_scheduled_time <= m_current)) {
    if(m_active_path == nullptr){
      HnzUtility::log_warn("%s No active path on which to send scheduled GI.", beforeLog.c_str());
      return;
    }
    HnzUtility::log_warn("%s It's %dh%d. Executing scheduled GI.", beforeLog.c_str(), m_gi_schedule.hour, m_gi_schedule.min);
    m_active_path->sendGeneralInterrogation();
    m_gi_schedule_already_sent = true;
  }
}

void HNZConnection::m_check_command_timer(HNZPath* path) {
  if(path == nullptr) return;
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::m_check_command_timer -";
  if (!path->command_sent.empty()) {
    list<Command_message>::iterator it = path->command_sent.begin();
    while (it != path->command_sent.end()) {
      if (it->timestamp_max < m_current) {
        HnzUtility::log_warn("%s A remote control (%s addr=%d) was not acknowledged in time !", beforeLog.c_str(),
                            it->type.c_str(), it->addr);
        it = path->command_sent.erase(it);
        // DF.GLOB.TC : nothing to do in HNZ
      } else {
        ++it;
      }
    }
  }
}

void HNZConnection::m_update_current_time() {
  uint64_t prevTimeMs = m_current;
  m_current =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  m_elapsedTimeMs = m_current - prevTimeMs;
}

void HNZConnection::m_update_quality_update_timer() {
  m_hnz_fledge->updateQualityUpdateTimer(m_elapsedTimeMs);
}

void HNZConnection::pathConnectionChanged(HNZPath* path, bool isReady) {
  if(!path) return;
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::pathConnectionChanged -";
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  HnzUtility::log_debug("%s Path %s changed connection state : %s.", beforeLog.c_str(), path->getName().c_str(), isReady ? "ready" : "not ready");

  if(m_first_input_connected == nullptr && (path->getProtocolState() == ProtocolState::INPUT_CONNECTED || path->getProtocolState() == ProtocolState::CONNECTED)){
    HnzUtility::log_debug("%s Path %s has INPUT_CONNECTED first.", beforeLog.c_str(), path->getName().c_str());
    m_first_input_connected = path;
  } else if (m_first_input_connected == path && (path->getProtocolState() == ProtocolState::CONNECTION || path->getProtocolState() == ProtocolState::OUTPUT_CONNECTED)) {
    m_first_input_connected = nullptr;
  }

  if(isReady){
    if(!m_active_path){
      m_active_path = path;
      HnzUtility::log_info("%s New active path is %s", beforeLog.c_str(), m_active_path->getName().c_str());
      path->setConnectionState(ConnectionState::ACTIVE);
      updateConnectionStatus(ConnectionStatus::STARTED);
    }
  } else {
    if(path == m_active_path){
      // We lost the connection on the active path
      m_active_path = nullptr;
      m_gi_repeat = 0;
      // Check for other available paths
      for (HNZPath* otherPath: m_paths)
      {
        if(otherPath != nullptr && otherPath != path && otherPath->getConnectionState() == ConnectionState::PASSIVE){
          m_active_path = otherPath;
          otherPath->setConnectionState(ConnectionState::ACTIVE);
          HnzUtility::log_warn("%s Switching active and passive path.", beforeLog.c_str());
          HnzUtility::log_info("%s New active path is %s", beforeLog.c_str(), m_active_path->getName().c_str());
          updateConnectionStatus(ConnectionStatus::STARTED);
          return;
        }
      }
      // No suitable path found !
      updateConnectionStatus(ConnectionStatus::NOT_CONNECTED);
    }
    // else : we lost the connection on a passive path, do nothing
  }
}

#ifdef UNIT_TEST
void HNZConnection::sendInitialGI() {
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZConnection::sendInitialGI -";
  if(!m_active_path){
    HnzUtility::log_error("%s No active path was found !", beforeLog.c_str());
    return;
  }
  m_gi_repeat = 0;
  m_active_path->sendGeneralInterrogation();
}
#endif

void HNZConnection::updateConnectionStatus(ConnectionStatus newState) {
  m_hnz_fledge->updateConnectionStatus(newState);
  if(newState == ConnectionStatus::NOT_CONNECTED){
    m_sendInitNexConnection = true;
  }
}

void HNZConnection::updateGiStatus(GiStatus newState) {
  m_hnz_fledge->updateGiStatus(newState);
}

GiStatus HNZConnection::getGiStatus() {
  return m_hnz_fledge->getGiStatus();
}