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
#include <config_category.h>
#include <logger.h>
#include <reading.h>

#include <atomic>
#include <sstream>
#include <thread>

#include "../../libhnz/src/inc/hnz_client.h"
#include "hnzconf.h"
#include "hnzconnection.h"

using namespace std;
using namespace std::chrono;

class HNZConnection;

class HNZ {
 public:
  typedef void (*INGEST_CB)(void*, Reading);

  HNZ();
  ~HNZ();

  void setAssetName(const string& asset) { m_asset = asset; }

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
   * @param msg_conf_json describe the messages that the plugin can received
   */
  bool setJsonConfig(const string& protocol_conf_json,
                     const string& msg_configuration);

  /**
   * Save the callback function and its data
   * @param data   The Ingest function data
   * @param cb     The callback function to call
   */
  void registerIngest(void* data, void (*cb)(void*, Reading));

  /**
   * Sends the datapoints passed as Reading to Fledge
   * @param readings Vector of one or more Reading depending on the received
   * message
   */
  void sendToFledge(vector<Reading>& readings);

  /**
   * Reset the GI queue. Delete previous TSCG received.
   */
  void resetGIQueue() { m_gi_readings_temp.clear(); };

 private:
  string m_asset;  // Plugin name in fledge

  HNZConf* m_hnz_conf;              // HNZ Configuration
  HNZClient* m_client;              // HNZ Client (lib hnz)
  HNZConnection* m_hnz_connection;  // HNZ Connection handling

  thread* m_receiving_thread;

  atomic<bool> m_is_running;

  INGEST_CB m_ingest;  // Callback function used to send data to south service
  void* m_data;        // Ingest function data
  bool m_connected;
  int m_remote_address;

  int module10M;  // HNZ Protocol related vars

  vector<Reading> m_gi_readings_temp;  // Contains all Reading of GI waiting for
                                       // the completeness check

  /**
   * Waits for new messages and processes them
   */
  void receive();

  /**
   * Connect (or re-connect) to the HNZ RTU
   */
  bool connect();

  /**
   * Analyzes the received frame.
   * @param frReceived
   */
  void m_analyze_frame(MSG_TRAME* frReceived);

  /**
   * Analyze an information frame
   * @return frame is understood
   */
  void analyze_info_frame(unsigned char* data, int size);

  /**
   * Handle TM4 messages: analyse them and returns readings for export to
   * fledge.
   */
  void m_handleTM4(vector<Reading>& reading, unsigned char* data);

  /**
   * Handle TSCE messages: analyse them and returns one reading for export to
   * fledge.
   */
  void m_handleTSCE(vector<Reading>& reading, unsigned char* data);

  /**
   * Handle TSCG messages: analyse them and returns one reading for export to
   * fledge.
   */
  void m_handleTSCG(vector<Reading>& reading, unsigned char* data);

  /**
   * Handle TMN messages: analyse them and returns readings for export to
   * fledge.
   */
  void m_handleTMN(vector<Reading>& reading, unsigned char* data);

  /**
   * Create a reading from the values given in argument.
   */
  static Reading m_prepare_reading(string label, string msg_code,
                                   unsigned char station_addr, int msg_address,
                                   int value, int valid, int ts, int ts_iv,
                                   int ts_c, int ts_s, bool time);
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

  string convert_data_to_str(unsigned char* data, int len);
};

#endif
