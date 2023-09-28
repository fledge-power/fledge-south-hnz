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
  this->m_hnz_conf = hnz_conf;
  this->m_hnz_fledge = hnz_fledge;

  // Create the path needed
  if (m_hnz_conf->get_ip_address_A() != "") {
    // Parent if is mostly here for scope lock
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    m_active_path = std::make_shared<HNZPath>(m_hnz_conf, this, false);
    if (m_hnz_conf->get_ip_address_B() != "") {
      m_passive_path = std::make_shared<HNZPath>(m_hnz_conf, this, true);
    }
  }
  else {
    HnzUtility::log_fatal("Attempted to start HNZ connection with no IP configured, aborting");
  }

  // Set settings for GI
  this->gi_repeat_count_max = m_hnz_conf->get_gi_repeat_count();
  this->gi_time_max = m_hnz_conf->get_gi_time() * 1000;
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
}

void HNZConnection::start() {
  HnzUtility::log_debug("HNZ Connection starting...");
  m_messages_thread = std::make_shared<std::thread>(&HNZConnection::m_manageMessages, this);
}

void HNZConnection::stop() {
  HnzUtility::log_debug("HNZ Connection stopping...");
  // Stop the thread that manage the messages
  m_is_running = false;

  // Stop the path used (close the TCP connection and stop the threads that
  // manage HNZ connections)
  if (m_active_path != nullptr || m_passive_path != nullptr) {
    // Parent if is mostly here for scope lock
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    if (m_active_path != nullptr) m_active_path->disconnect();
    if (m_passive_path != nullptr) m_passive_path->disconnect();
  }

  // Wait for the end of the thread that manage the messages
  if (m_messages_thread != nullptr) {
    std::shared_ptr<std::thread> temp = m_messages_thread;
    m_messages_thread = nullptr;
    HnzUtility::log_debug("Waiting for the messages managing thread");
    temp->join();
  }

  HnzUtility::log_info("HNZ Connection stopped !");
}

void HNZConnection::checkGICompleted(bool success) { 
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  // GI is a success
  if (success) {
    m_hnz_fledge->GICompleted(true);
    return;
  }
  // GI not completed in time or last TS received with other missing TS
  if (m_active_path->gi_repeat > gi_repeat_count_max) {
    // GI failed
    m_hnz_fledge->GICompleted(false);
  } else {
    HnzUtility::log_warn("General Interrogation Timeout, repeat GI");
    // Clean queue in HNZ class
    m_hnz_fledge->resetGIQueue();
    // Send a new GI
    m_active_path->sendGeneralInterrogation();
  }
}

void HNZConnection::onGICompleted() { 
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  m_active_path->gi_repeat = 0;
}

void HNZConnection::m_manageMessages() {
  m_update_current_time();

  m_days_since_epoch = m_current / 86400000;
  int today_ms = m_current % 86400000;
  m_gi_scheduled_time = m_current - today_ms +
                        ((m_gi_schedule.hour * 60 + m_gi_schedule.min) * 60000);
  if (m_gi_schedule.activate) {
    // If GI schedule is activated, check if time scheduled is outdated
    HnzUtility::log_info("GI scheduled is set at " +
                              to_string(m_gi_schedule.hour) + "h" +
                              to_string(m_gi_schedule.min) + ".");
    if (m_current >= m_gi_scheduled_time) {
      HnzUtility::log_info(
          "No GI for today because the scheduled time has already passed.");
      m_gi_schedule_already_sent = true;
    } else {
      HnzUtility::log_info("GI scheduled is set for today");
    }
  }

  do {
    m_update_current_time();

    // Manage repeat/timeout for each path
    if (m_active_path || m_passive_path) {
      // Parent if is mostly here for scope lock
      std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
      m_check_timer(m_active_path);
      m_check_timer(m_passive_path);
    }

    // GI support
    m_check_GI();

    // Manage command ACK
    m_check_command_timer();

    // Manage quality update
    m_update_quality_update_timer();

    this_thread::sleep_for(milliseconds(100));
  } while (m_is_running);
}

void HNZConnection::m_check_timer(std::shared_ptr<HNZPath> path) const {
  if ((path != nullptr) && !path->msg_sent.empty() && path->isConnected()) {
    Message& msg = path->msg_sent.front();
    if (path->last_sent_time + m_repeat_timeout < m_current) {
      if (path->getRepeat() >= path->repeat_max) {
        // Connection disrupted, back to SARM
        HnzUtility::log_warn(path->getName() +
                                  " Connection disrupted, back to SARM");

        path->go_to_connection();
      } else {
        // Repeat the message
        HnzUtility::log_warn(
            path->getName() +
            " Timeout, sending back first unacknowledged message");
        path->sendBackInfo(msg);

        // Move other unacknowledged messages to waiting queue
        while (path->msg_sent.size() > 1) {
          path->msg_waiting.push_front(path->msg_sent.back());
          path->msg_sent.pop_back();
        }
      }
    }
  }
}

void HNZConnection::m_check_GI() {
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  // Check the status of an ongoing GI
  if (m_active_path->gi_repeat != 0) {
    if (m_active_path->gi_start_time + gi_time_max < m_current) {
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
    HnzUtility::log_warn("It's " + to_string(m_gi_schedule.hour) + "h" +
                              to_string(m_gi_schedule.min) + ". GI schedule.");
    m_active_path->sendGeneralInterrogation();
    m_gi_schedule_already_sent = true;
  }
}

void HNZConnection::m_check_command_timer() {
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  if (!m_active_path->command_sent.empty()) {
    list<Command_message>::iterator it = m_active_path->command_sent.begin();
    while (it != m_active_path->command_sent.end()) {
      if (it->timestamp_max < m_current) {
        HnzUtility::log_error(
            "A remote control was not acknowledged in time !");
        m_active_path->go_to_connection();
        it = m_active_path->command_sent.erase(it);
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
      std::chrono::duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
  m_elapsedTimeMs = m_current - prevTimeMs;
}

void HNZConnection::m_update_quality_update_timer() {
  m_hnz_fledge->updateQualityUpdateTimer(m_elapsedTimeMs);
}

void HNZConnection::switchPath() {
  if (m_passive_path != nullptr) {
    HnzUtility::log_warn("Switching active and passive path.");
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    // Permute path
    std::shared_ptr<HNZPath> temp = m_active_path;
    m_active_path = m_passive_path;
    m_passive_path = temp;
    m_active_path->setActivePath(true);
    m_passive_path->setActivePath(false);

    HnzUtility::log_info("New active path is " + m_active_path->getName());

    // When switching path, update connection status accordingly
    if (m_active_path->isHNZConnected()) {
      updateConnectionStatus(ConnectionStatus::STARTED);
    }
    else {
      updateConnectionStatus(ConnectionStatus::NOT_CONNECTED);
    }
  } else {
    // Redundancy isn't enable, can't switch to the other path
    HnzUtility::log_warn("Redundancy isn't enabled, can't switch to the other path");
  }
}

void HNZConnection::sendInitialGI() {
  std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
  m_active_path->gi_repeat = 0;
  m_active_path->sendGeneralInterrogation();
}

void HNZConnection::updateConnectionStatus(ConnectionStatus newState) {
  m_hnz_fledge->updateConnectionStatus(newState);
}

void HNZConnection::updateGiStatus(GiStatus newState) {
  m_hnz_fledge->updateGiStatus(newState);
}

GiStatus HNZConnection::getGiStatus() {
  return m_hnz_fledge->getGiStatus();
}