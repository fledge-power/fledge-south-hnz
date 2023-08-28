#ifndef HNZ_H
#define HNZ_H

/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Lucas Barret, Colin Constans, Justin Facquet
 */
#include <atomic>
#include <thread>
#include <mutex>

#include <reading.h>
#include <plugin_api.h>

#include "hnzconf.h"

class HNZConnection;
class HNZPath;

// Enums put outside of the HNZ class so that they can be forward declared
enum class ConnectionStatus
{
    STARTED,
    NOT_CONNECTED
};
enum class GiStatus
{
    IDLE,
    STARTED,
    IN_PROGRESS,
    FAILED,
    FINISHED
};

/**
 * @brief Class used to receive messages and push them to fledge.
 */
class HNZ {
 public:
  typedef void (*INGEST_CB)(void*, Reading);

  HNZ();
  ~HNZ();

  void setAssetName(const std::string& asset) { m_asset = asset; }

  /**
   * Start the HZN south plugin
   */
  void start();

  /**
   * Stop the HZN south plugin
   */
  void stop();

  /**
   * Set the configuration of the HNZ South Plugin. Two JSON configuration are
   * required.
   * @param protocol_conf_json Contain value to configure the protocol
   * @param msg_conf_json Describe the messages that the plugin can received
   */
  bool setJsonConfig(const std::string& protocol_conf_json,
                     const std::string& msg_configuration);

  /**
   * Save the callback function and its data
   * @param data   The Ingest function data
   * @param cb     The callback function to call
   */
  void registerIngest(void* data, void (*cb)(void*, Reading));

  /**
   * Reset the GI queue. Delete previous TSCG received.
   */
  void resetGIQueue() { m_gi_addresses_received.clear(); };

  /**
   * Called by Fledge to send a command message
   *
   * @param operation The command name
   * @param count Number of parameters
   * @param params Array of parameters
   */
  bool operation(const std::string& operation, int count,
                 PLUGIN_PARAMETER** params);

  /**
   * Utility function used to store the content of a frame as a human readable hex string
   *
   * @param frame The frame to format
   * @return a string representing the bytes of that frame in hexadecimal
   */
  static std::string frameToStr(std::vector<unsigned char> frame);

  /**
   * Utility function used to build a timestamp from the date of day
   *
   * @param dateTime Date of day (time information will be reset to 00:00:00 if any)
   * @param daySection Section of the day in groups of 10 minutes in current day [0..143]
   * @param ts Time in group of 10 milliseconds in current section of day
   * @return an epoch timestamp in milliseconds
   */
  static unsigned long getEpochMsTimestamp(std::chrono::time_point<std::chrono::system_clock> dateTime,
                                            unsigned char daySection, unsigned int ts);

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
   * GI is complete, send GI status and clear timer.
   * @param success Indicates if the GIT was completed with success (true) or if it failed (false)
   */
  void GICompleted(bool success);

  /**
   * Update the quality update timer and send quality update if it timed out
   * @param elapsedTimeMs Time elapsed in milliseconds
   */
  void updateQualityUpdateTimer(long elapsedTimeMs);

 protected:
  /**
   * Sends a CG request (reset counters if any was already in progress)
   */
  void sendInitialGI();

private:
  std::string m_asset;  // Plugin name in fledge
  std::atomic<bool> m_is_running{false};
  // Receiving threads
  std::unique_ptr<std::thread> m_receiving_thread_A;
  std::unique_ptr<std::thread> m_receiving_thread_B;
  // Contains all addressed of the TS received in response to a GI request
  std::vector<unsigned int> m_gi_addresses_received;  

  // Others HNZ related class
  std::shared_ptr<HNZConf> m_hnz_conf{std::make_shared<HNZConf>()}; // HNZ Configuration
  std::unique_ptr<HNZConnection> m_hnz_connection;                  // HNZ Connection handling

  // Fledge related
  INGEST_CB m_ingest;  // Callback function used to send data to south service
  void* m_data;        // Ingest function data

  // Configuration defined variables
  unsigned int m_remote_address;
  BulleFormat m_test_msg_receive;
  // Section of day (modulo 10 minutes)
  unsigned char m_daySection = 0;

  // Connection and GI status management
  ConnectionStatus m_connStatus = ConnectionStatus::NOT_CONNECTED;
  GiStatus m_giStatus = GiStatus::IDLE;
  std::recursive_mutex m_connexionGiMutex;
  long m_qualityUpdateTimer = 0;
  long m_qualityUpdateTimeoutMs = 500;

  /**
   * Waits for new messages and processes them
   */
  void receive(HNZPath* hnz_path_in_use);

  /**
   * Handle a message: translate the message and send it to Fledge.
   */
  void m_handle_message(const vector<unsigned char>& data);

  /**
   * Handle Modulo code messages: store the latest modulo for timestamp computation
   */
  void m_handleModuloCode(vector<Reading>& readings, const vector<unsigned char>& data);
  
  /**
   * Handle TM4 messages: analyse them and returns readings for export to
   * fledge.
   */
  void m_handleTM4(vector<Reading>& readings, const vector<unsigned char>& data) const;

  /**
   * Handle TSCE messages: analyse them and returns one reading for export to
   * fledge.
   */
  void m_handleTSCE(vector<Reading>& readings, const vector<unsigned char>& data) const;

  /**
   * Handle TSCG messages: analyse them and returns one reading for export to
   * fledge.
   */
  void m_handleTSCG(vector<Reading>& readings, const vector<unsigned char>& data);

  /**
   * Handle TMN messages: analyse them and returns readings for export to
   * fledge.
   */
  void m_handleTMN(vector<Reading>& readings, const vector<unsigned char>& data) const;

  /**
   * Handle TVC ACK messages: analyse them and returns readings for export to
   * fledge.
   */
  void m_handleATVC(vector<Reading>& readings, const vector<unsigned char>& data) const;

  /**
   * Handle TC ACK messages: analyse them and returns readings for export to
   * fledge.
   */
  void m_handleATC(vector<Reading>& readings, const vector<unsigned char>& data) const;

  // Dedicated structure used to store parameters passed to m_prepare_reading.
  // This prevents "too many parameters" warning from Sonarqube (cpp:S107).
  struct ReadingParameters {
    // Those are mandatory parameters
    std::string label;
    std::string msg_code;
    unsigned int station_addr = 0;
    unsigned int msg_address = 0;
    long int value = 0;
    unsigned int valid = 0;
    // Those are optional parameters
    // TSCE only
    unsigned long ts = 0;
    unsigned int ts_iv = 0;
    unsigned int ts_c = 0;
    unsigned int ts_s = 0;
    // TS only
    bool cg = false;
    // TM only
    std::string an = "";
    // TS and TN only
    bool outdated = false;
    bool qualityUpdate = false;
  };
  /**
   * Create a reading from the values given in argument.
   */
  static Reading m_prepare_reading(const ReadingParameters& params);

  /**
   * Sends the datapoints passed as Reading to Fledge
   * @param readings Vector of one or more Reading depending on the received
   * message
   */
  void m_sendToFledge(vector<Reading>& readings);

  /**
   * Create a datapoint.
   * @param name
   * @param value
   */
  template <class T>
  static Datapoint* m_createDatapoint(const string& name, const T value) {
    DatapointValue dp_value = DatapointValue(value);
    return new Datapoint(name, dp_value);
  }

  /**
   * Called when a data changed event is received. This calls back to the
   * south service and adds the points to the readings queue to send.
   *
   * @param reading The reading to push to fledge
   */
  void ingest(Reading& reading);

  /**
   * Send the updated values of connection and GI states
   */
  void m_sendConnectionStatus();

  /**
   * Send the updated values of connection and GI states if the corresponding parameter is true
   * 
   * @param connxStatus If true, updates the connexion status part of the reading
   * @param giStatus If true, updates the GI status part of the reading
   */
  void m_sendSouthMonitoringEvent(bool connxStatus, bool giStatus);

  /**
   * Create a quality reading for each TM available and send them to fledge
   * @param invalid Send reading with do_valid = 1 if true and do_valid = 0 if false
   * @param outdated Send reading with do_outdated = 1 if true and do_outdated = 0 if false
   * @param rejectFilter Only the TM with an address NOT listed in this filter will be sent
   */
  void m_sendAllTMQualityReadings(bool invalid, bool outdated, const vector<unsigned int>& rejectFilter = {});

  /**
   * Create a quality reading for each TS available and send them to fledge
   * @param invalid Send reading with do_valid = 1 if true and do_valid = 0 if false
   * @param outdated Send reading with do_outdated = 1 if true and do_outdated = 0 if false
   * @param rejectFilter Only the TS with an address NOT listed in this filter will be sent
   */
  void m_sendAllTSQualityReadings(bool invalid, bool outdated, const vector<unsigned int>& rejectFilter = {});

  /**
   * Create a quality reading for each TI available with type defined by paramsTemplate.msg_code and send them to fledge
   * @param paramsTemplate Template of reading parameters to send, it must define any parameter necessary
   * for the type of TI selected except label and msg_address.
   * @param rejectFilter Only the TI with an address NOT listed in this filter will be sent
   */
  void m_sendAllTIQualityReadings(const ReadingParameters& paramsTemplate, const vector<unsigned int>& rejectFilter = {});

};

#endif
