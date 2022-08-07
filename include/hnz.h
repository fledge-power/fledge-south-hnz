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

#include <logger.h>
#include <reading.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

#include "../../libhnz/src/inc/hnz_client.h"
#include "hnzconf.h"

class HNZFledge;

class HNZ {
 public:
  typedef void (*INGEST_CB)(void*, Reading);

  HNZ();
  ~HNZ() = default;

  void setAssetName(const std::string& asset) { m_asset = asset; }
  void restart();
  void start();
  void stop();
  void receive();
  int connect();
  void stop_loop();
  void analyze_frame(unsigned char* data, int size);
  bool analyze_info_frame(unsigned char* data, unsigned char station_addr,
                          int ns, int p, int nr, int size);
  void sendToFledge(std::string msg_code, unsigned char station_addr,
                    int msg_address, int value, int valid, int ts, int ts_iv,
                    int ts_c, int ts_s, std::string label, bool time);
  std::string convert_data_to_str(unsigned char* data, int len);

  // void ingest(Reading& reading);
  void ingest(std::string assetName, std::vector<Datapoint*>& points);
  void registerIngest(void* data, void (*cb)(void*, Reading));

  std::string m_asset;

  HNZClient* m_client;

  std::mutex loopLock;
  std::atomic<bool> loopActivated;
  std::thread loopThread;

  void setJsonConfig(const std::string& configuration,
                     const std::string& msg_configuration);

 private:
  // configuration
  HNZConf* m_hnz_conf;

  INGEST_CB m_ingest;  // Callback function used to send data to south service
  void* m_data;        // Ingest function data
  bool m_connected;
  HNZFledge* m_fledge;
  int frame_number, module10M;
};

class HNZFledge {
 public:
  explicit HNZFledge(HNZ* hnz) : m_hnz(hnz){};

  // ==================================================================== //
  // Note : The overloaded method addData is used to prevent the user from
  // giving value type that can't be handled. The real work is forwarded
  // to the private method m_addData

  // Sends the datapoints passed as Reading to Fledge
  void sendData(Datapoint* dp, const std::string& label);

  template <class T>
  Datapoint* m_addData(std::string message_type, unsigned char addr,
                       int info_adress, int value, int valid, int ts, int ts_iv,
                       int ts_c, int ts_s, bool time);

 private:
  template <class T>
  static Datapoint* m_createDatapoint(const std::string& dataname,
                                      const T value) {
    DatapointValue dp_value = DatapointValue(value);
    return new Datapoint(dataname, dp_value);
  }

  HNZ* m_hnz;
};

#endif
