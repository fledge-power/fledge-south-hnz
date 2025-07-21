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
#include <array>

#include "hnzconf.h"

class HNZ;
class HNZPath;
enum class ConnectionStatus;
enum class GiStatus;
enum class ConnectionState : unsigned char;

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
  HNZPath* getActivePath() {
    std::lock_guard<std::recursive_mutex> lock(m_active_path_mutex);
    return m_active_path;
  };

  /**
   * Get a pointer to the first passive path found, if any.
   * @return a passive path or nullptr
   */
  HNZPath* getPassivePath() {
    std::lock_guard<std::recursive_mutex> lock(m_active_path_mutex);
    for (std::shared_ptr<HNZPath> path: m_paths)
    {
      auto rawPath = path.get();
      if(rawPath != nullptr && rawPath != m_active_path) return rawPath;
    }
    return nullptr;
  };

  /**
   * Get both active and passive path as an array of pointers (with a single lock)
   * @return array of existing paths
   */
  const std::array<std::shared_ptr<HNZPath>, MAXPATHS>& getPaths() const {
    return m_paths;
  };

  /**
   * Manages the connection state of the different paths.
   * HNZConnection decide if transition is allowed or not and make the actual state update.
   * @param path Path requesting a state change
   * @param newState New state requested
   */
  void requestConnectionState(HNZPath* path, ConnectionState newState);

  /**
   * Tries to make a path other than the input path active
   * @param path Path that cannot be activated
   * @return true if a path to activate was found, else false
   */
  bool tryActivateOtherPath(const HNZPath* path);

#ifdef UNIT_TEST
  /**
   * Send the initial GI message (reset retry counter if it was in progress)
   */
  void sendInitialGI();
#endif

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
   * Returns mutex used to protect the active path
   */
  std::recursive_mutex& getActivePathMutex() { return m_active_path_mutex; }

   /**
   * Returns the running status of the connection
   */
  bool isRunning() const { return m_is_running; };

  /**
   * A running path can extract messages only if it is active, or has at least its input connected.
   * Ensures that only one of the two possible path will extract the message at all times.
   */
  bool canPathExtractMessage(HNZPath* path){
    std::lock_guard<std::recursive_mutex> lock(m_active_path_mutex);
    if(path == nullptr) return false;
    if(path == m_active_path){
      return true;
    }
    return path == m_first_input_connected && m_active_path == nullptr;
  }

  /**
   * Returns the name of the Fledge service instanciating this plugin
   */
  inline const std::string& getServiceName() const { return m_hnz_fledge->getServiceName(); }

  
  /**
   * Setter for the day section (modulo calculation)
   */
  inline void setDaySection(unsigned char daySection) { m_hnz_fledge->setDaySection(daySection); }

  /**
   * Allows a path to notify that a GI request has been sent
   */
  void notifyGIsent();

 private:
  HNZPath* m_active_path = nullptr;
  // First path in protocol state INPUT_CONNECTED, covers edge cases of the protocol
  HNZPath* m_first_input_connected = nullptr;
  std::array<std::shared_ptr<HNZPath>, MAXPATHS> m_paths = {nullptr, nullptr};
  std::recursive_mutex m_active_path_mutex;
  std::shared_ptr<std::thread> m_messages_thread;  // Main thread that monitors messages
  std::atomic<bool> m_is_running{false};  // If false, the connection thread will stop
  uint64_t m_current = 0;         // Store the last time requested
  uint64_t m_elapsedTimeMs = 0;   // Store elapsed time in milliseconds every time m_current is updated
  uint64_t m_days_since_epoch = 0;
  bool m_sendInitNexConnection = true; // The init messages (time/date/GI) have to be sent

  int m_gi_repeat = 0;
  long m_gi_start_time = 0;
  // Plugin configuration
  int m_gi_repeat_count_max = 0;  // time to wait for GI completion
  int m_gi_time_max = 0;          // repeat GI for this number of times in case it is incomplete
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
  void m_check_timer(std::shared_ptr<HNZPath> path);

  /**
   * Check the state of ongoing GI (General Interrogation) and manage scheduled
   * GI.
   */
  void m_check_GI();

  /**
   * Checks that sent command messages have been acknowledged and removes them
   * from the sent queue.
   */
  void m_check_command_timer(std::shared_ptr<HNZPath> path) const;

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