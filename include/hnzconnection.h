/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#ifndef HNZConnection_H
#define HNZConnection_H

#include <atomic>
#include <thread>
#include <mutex>

#include "hnzconf.h"

class HNZ;
class HNZPath;
enum class ConnectionStatus;
enum class GiStatus;

/**
 * @brief Class used to manage the HNZ connection. Manage one path (or two path,
 * if redundancy is enable). Manage alse the HNZ Protocol timers: handle message
 * resending, command ack, GI (General Interrogation).
 */
class HNZConnection {
 public:
  HNZConnection(std::shared_ptr<HNZConf> hnz_conf, HNZ* hnz_fledge);
  ~HNZConnection();

  /**
   * Set up the hnz connection but don't establish connection on the path(s),
   * this has to be done elsewhere.
   */
  void start();

  /**
   * Close the connection (on both path if redundancy enable).
   */
  void stop();

  /**
   * When a signle GI request is over, in case it failed, retry if there is any retry left.
   * If it succeeded mark the whole GI as finished, if there is no more retry left, mark the whole GI as failed.
   * @param success Indicates if the GI was completed with success (true) or if it failed (false)
   */
  void checkGICompleted(bool success);

  /**
   * GI is complete, stop timers.
   * @param success Indicates if the GI was completed with success (true) or if it failed (false)
   */
  void onGICompleted();

  /**
   * Get the path currently in use by HNZ to exchange messages (informations and
   * commands).
   * @return the active path
   */
  std::shared_ptr<HNZPath> getActivePath() {
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    return m_active_path;
  };

  /**
   * Get the path in stand-by.
   * @return the active path
   */
  std::shared_ptr<HNZPath> getPassivePath() {
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    return m_passive_path;
  };

  /**
   * Get both active and passive path (with a single lock)
   * @return a path pair (active_path, passive_path)
   */
  std::pair<std::shared_ptr<HNZPath>, std::shared_ptr<HNZPath>> getBothPath() {
    std::lock_guard<std::recursive_mutex> lock(m_path_mutex);
    return std::make_pair(m_active_path, m_passive_path);
  };

  /**
   * Switch between the active path and passive path. Must be called in case of
   * connection problem on the active path.
   */
  void switchPath();

  /**
   * Send the initial GI message (reset retry counter if it was in progress)
   */
  void sendInitialGI();

  /**
   * Called to update the current connection status
   *
   * @param newState New status for the connection
   */
  void updateConnectionStatus(ConnectionStatus newState);

  /**
   * Called to update the current GI status
   *
   * @param newState New status for the GI
   */
  void updateGiStatus(GiStatus newState);

  /**
   * Returns the current GI status
   */
  GiStatus getGiStatus();

  /**
   * Returns mutex used to protect the active and passive path
   */
  std::recursive_mutex& getPathMutex() { return m_path_mutex; }

   /**
   * Returns the running status of the connection
   */
  bool isRunning() const { return m_is_running; };

  /**
   * Returns the name of the Fledge service instanciating this plugin
   */
  inline const std::string& getServiceName() const { return m_hnz_fledge->getServiceName(); }

  
  /**
   * Setter for the day section (modulo calculation)
   */
  inline void setDaySection(unsigned char daySection) { m_hnz_fledge->setDaySection(daySection); }

 private:
  std::shared_ptr<HNZPath> m_active_path;
  std::shared_ptr<HNZPath> m_passive_path;
  std::recursive_mutex m_path_mutex;
  std::shared_ptr<std::thread> m_messages_thread;  // Main thread that monitors messages
  std::atomic<bool> m_is_running{false};  // If false, the connection thread will stop
  uint64_t m_current = 0;         // Store the last time requested
  uint64_t m_elapsedTimeMs = 0;   // Store elapsed time in milliseconds every time m_current is updated
  uint64_t m_days_since_epoch = 0;

  // Plugin configuration
  int gi_repeat_count_max = 0;  // time to wait for GI completion
  int gi_time_max = 0;          // repeat GI for this number of times in case it is
                            // incomplete
  GIScheduleFormat m_gi_schedule;

  int m_repeat_timeout = 0;  // time allowed for the receiver to acknowledge a frame

  bool m_gi_schedule_already_sent = false;  // True if scheduled GI was already
                                    // performed today
  long m_gi_scheduled_time = 0;  // Time of scheduled GI (in ms from midnight)

  /**
   * Manage the timers. Checks that : messages are acknowledged, command are
   * acknowledged, GI is done on time, trigger scheduled GI.
   */
  void m_manageMessages();

  /**
   * Checks that sent messages have been acknowledged and removes them from the
   * sent queue. Each path has its own files and must therefore be specified.
   * If a message is not acknowledged, then a retransmission request is sent.
   * @param path the related path
   */
  void m_check_timer(std::shared_ptr<HNZPath> path) const;

  /**
   * Check the state of ongoing GI (General Interrogation) and manage scheduled
   * GI.
   */
  void m_check_GI();

  /**
   * Checks that sent command messages have been acknowledged and removes them
   * from the sent queue.
   */
  void m_check_command_timer();

  /**
   * Update the current time and time elapsed since last call to this function
   */
  void m_update_current_time();

  /**
   * Update the timer for quality update
   */
  void m_update_quality_update_timer();

  std::shared_ptr<HNZConf> m_hnz_conf;  // HNZ Configuration
  HNZ* m_hnz_fledge = nullptr;          // HNZ Fledge
};

#endif