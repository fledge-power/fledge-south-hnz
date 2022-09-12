/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Lucas Barret, Colin Constans, Justin Facquet
 */

#include "include/hnz.h"

HNZ::HNZ()
    : m_hnz_conf(new HNZConf),
      m_is_running(false),
      m_connected(false),
      m_client(new HNZClient) {}

HNZ::~HNZ() {
  if (m_is_running) {
    stop();
  }
}

void HNZ::start() {
  Logger::getLogger()->setMinLevel(DEBUG_LEVEL);

  Logger::getLogger()->info("Starting HNZ south plugin...");

  m_receiving_thread = new thread(&HNZ::receive, this);
  m_is_running = true;
}

void HNZ::stop() {
  m_is_running = false;

  if (m_receiving_thread != nullptr) {
    if (m_connected) {
      m_client->stop();
    }
    Logger::getLogger()->info("Waiting for the receiving thread");
    m_receiving_thread->join();
    m_receiving_thread = nullptr;
  }
  Logger::getLogger()->info("Plugin stopped");
}

bool HNZ::setJsonConfig(const string &protocol_conf_json,
                        const string &msg_conf_json) {
  bool was_running = m_is_running;
  if (m_is_running) {
    Logger::getLogger()->info(
        "Configuration change requested, stopping the plugin");
    stop();
  }

  Logger::getLogger()->info("Reading json config string...");

  m_hnz_conf->importConfigJson(protocol_conf_json);
  m_hnz_conf->importExchangedDataJson(msg_conf_json);
  if (!m_hnz_conf->is_complete()) {
    Logger::getLogger()->fatal(
        "Unable to set Plugin configuration due to error with the json conf.");
    return false;
  }

  Logger::getLogger()->info("Json config parsed successsfully.");

  m_remote_address = m_hnz_conf->get_remote_station_addr();
  m_test_msg_receive = m_hnz_conf->get_test_msg_receive();
  m_hnz_connection = new HNZConnection(m_hnz_conf, m_client, this);

  if (was_running) {
    Logger::getLogger()->warn("Restarting the plugin...");
    start();
  }
  return true;
}

bool HNZ::connect() {
  int i = 1;
  while (((i <= RETRY_CONN_NUM) or (RETRY_CONN_NUM == -1)) and m_is_running) {
    Logger::getLogger()->info("Connecting to server ... [" + to_string(i) +
                              "/" + to_string(RETRY_CONN_NUM) + "]");

    m_connected = !(m_client->connect_Server(
        m_hnz_conf->get_ip_address().c_str(), m_hnz_conf->get_port()));

    if (m_connected) {
      Logger::getLogger()->info("Connected.");
      return true;
    } else {
      Logger::getLogger()->warn("Error in connection, retrying in " +
                                to_string(RETRY_CONN_DELAY) + "s ...");
      this_thread::sleep_for(std::chrono::seconds(RETRY_CONN_DELAY));
    }
    i++;
  }
  return false;
}

void HNZ::receive() {
  if (!m_hnz_conf->is_complete()) {
    return;
  }

  // Connect to the server
  if (!connect()) {
    Logger::getLogger()->fatal("Unable to connect to server, stopping ...");
    return;
  }

  m_hnz_connection->start();

  Logger::getLogger()->warn("Listening for data ...");

  while (m_is_running) {
    MSG_TRAME *frReceived;

    // Waiting for data
    frReceived = (m_client->receiveFr());
    if (frReceived != nullptr) {
      // Checking the CRC
      if (m_client->checkCRC(frReceived)) {
        Logger::getLogger()->debug("CRC is good");

        m_analyze_frame(frReceived);
      } else {
        Logger::getLogger()->warn("The CRC does not match");
      }
    } else {
      Logger::getLogger()->warn("No data available, checking connection ...");
      // Try to reconnect
      if (!connect()) {
        Logger::getLogger()->warn("Connection lost");
        // stop();
        m_is_running = false;
        m_client->stop();
      }
    }
  }

  m_hnz_connection->stop();
}

void HNZ::m_analyze_frame(MSG_TRAME *frReceived) {
  unsigned char *data = frReceived->aubTrame;
  int size = frReceived->usLgBuffer;
  unsigned char address = data[0] >> 2;  // remote address
  unsigned char c = data[1];             // Message type

  Logger::getLogger()->debug(convert_data_to_str(data, size));

  if (m_remote_address == address) {
    switch (c) {
      case UA:
        Logger::getLogger()->debug("Received UA");
        m_hnz_connection->receivedUA();
        break;
      case SARM:
        Logger::getLogger()->debug("Received SARM");
        m_hnz_connection->receivedSARM();
        module10M = 0;
        break;
      default:
        // Get NR, P/F ans NS field
        int ns = (c >> 1) & 0x07;
        int pf = (c >> 4) & 0x01;
        int nr = (c >> 5) & 0x07;
        if ((c & 0x01) == 0) {
          // Information frame
          Logger::getLogger()->warn("Data received (ns = " + to_string(ns) +
                                    ", p = " + to_string(pf) +
                                    ", nr = " + to_string(nr) + ")");

          int payloadSize = size - 4;  // Remove address, type, CRC (2 bytes)
          analyze_info_frame(data + 2, payloadSize);
          // Computing the frame number & sending RR
          m_hnz_connection->sendRR(pf == 1, ns, nr);
        } else {
          // Supervision frame
          Logger::getLogger()->warn("RR received (f = " + to_string(pf) +
                                    ", nr = " + to_string(nr) + ")");
          m_hnz_connection->receivedRR(nr, pf == 1);
        }

        break;
    }
  } else {
    Logger::getLogger()->warn("The address don't match the configuration!");
  }
}

string HNZ::convert_data_to_str(unsigned char *data, int len) {
  string s = "";
  for (int i = 0; i < len; i++) {
    s += to_string(data[i]);
    if (i < len - 1) s += " ";
  }
  return s;
}

void HNZ::analyze_info_frame(unsigned char *data, int payloadSize) {
  int len = 0;                // Length of message to push in Fledge
  unsigned char t = data[0];  // Payload type
  vector<Reading> readings;   // Contains data object to push to fledge

  switch (t) {
    case TM4:
      Logger::getLogger()->info("Received TMA");
      m_handleTM4(readings, data);
      len = 6;
      break;
    case TSCE:
      Logger::getLogger()->info("Received TSCE");
      m_handleTSCE(readings, data);
      len = 5;
      break;
    case TSCG:
      Logger::getLogger()->info("Received TSCG");
      m_handleTSCG(readings, data);
      len = 6;
      break;
    case TMN:
      Logger::getLogger()->info("Received TMN");
      m_handleTMN(readings, data);
      len = 7;
      break;
    case 0x0F:
      module10M = (int)data[1];
      Logger::getLogger()->info("Received Modulo 10mn");
      len = 2;
      break;
    case 0x09:
      Logger::getLogger()->info("Received TC ACK");
      m_handleATC(readings, data);
      len = 3;
      break;
    case 0x0A:
      Logger::getLogger()->info("Received TVC ACK");
      m_handleATVC(readings, data);
      len = 3;
      break;
    default:
      if (t == m_test_msg_receive.first &&
          data[1] == m_test_msg_receive.second) {
        Logger::getLogger()->info("Received BULLE");
        m_hnz_connection->receivedBULLE();
        len = 2;
      } else {
        Logger::getLogger()->info("Received an unknown type");
      }
      break;
  }

  if (len != 0) {
    Logger::getLogger()->debug("[" + convert_data_to_str(data, len) + "]");

    if (!readings.empty()) {
      sendToFledge(readings);
    }

    // Check the length of the payload
    // There can be several messages in the same frame
    if (len != payloadSize) {
      // Analyze the rest of the payload
      analyze_info_frame(data + len, payloadSize - len);
    }
  } else {
    Logger::getLogger()->info("Unknown message");
  }
}

void HNZ::m_handleTM4(vector<Reading> &readings, unsigned char *data) {
  string msg_code = "TMA";
  for (size_t i = 0; i < 4; i++) {
    // 4 TM inside a TM cyclique
    unsigned int msg_address =
        stoi(to_string((int)data[1]) + to_string(i));  // ADTM + i
    string label = m_hnz_conf->getLabel(msg_code, msg_address);

    if (!label.empty()) {
      int noctet = 2 + i;
      int value =
          (((data[noctet] >> 7) == 0x1) ? (-1 * ((int)data[noctet] ^ 0xFF) - 1)
                                        : data[noctet]);  // VALTMi
      unsigned int valid = (data[noctet] == 0xFF);  // Invalid if VALTMi = 0xFF

      readings.push_back(m_prepare_reading(label, msg_code, m_remote_address,
                                           msg_address, value, valid, 0, 0, 0,
                                           0, false));
    }
  }
}

void HNZ::m_handleTSCE(vector<Reading> &readings, unsigned char *data) {
  string msg_code = "TSCE";
  unsigned int msg_address = stoi(to_string((int)data[1]) +
                                  to_string((int)(data[2] >> 5)));  // AD0 + ADB

  string label = m_hnz_conf->getLabel(msg_code, msg_address);

  if (!label.empty()) {
    unsigned int value = (int)(data[2] >> 3) & 0x1;  // E bit
    unsigned int valid = (int)(data[2] >> 4) & 0x1;  // V bit

    unsigned int ts = (int)((data[3] << 8) | data[4]);
    unsigned int ts_iv = (int)(data[2] >> 2) & 0x1;  // HNV bit
    unsigned int ts_s = (int)data[2] & 0x1;          // S bit
    unsigned int ts_c = (int)(data[2] >> 1) & 0x1;   // C bit

    readings.push_back(m_prepare_reading(label, msg_code, m_remote_address,
                                         msg_address, value, valid, ts, ts_iv,
                                         ts_c, ts_s, true));
  }
}

void HNZ::m_handleTSCG(vector<Reading> &readings, unsigned char *data) {
  string msg_code = "TSCG";
  for (size_t i = 0; i < 16; i++) {
    // 16 TS inside a TSCG
    unsigned int msg_address = stoi(
        to_string((int)data[1] + (int)i / 8) +
        to_string(i % 8));  // AD0 + i%8 for first 8, (AD0+1) + i%8 for others
    string label = m_hnz_conf->getLabel(msg_code, msg_address);

    if (!label.empty()) {
      int noctet = 2 + (i / 4);
      int dep = (3 - (i % 4)) * 2;
      unsigned int value = (int)(data[noctet] >> dep) & 0x1;  // E
      unsigned int valid = (int)(data[noctet] >> dep) & 0x2;  // V

      m_gi_readings_temp.push_back(
          m_prepare_reading(label, msg_code, m_remote_address, msg_address,
                            value, valid, 0, 0, 0, 0, false));
    }
  }

  // Check if GI is complete
  if (!m_gi_readings_temp.empty() &&
      (m_gi_readings_temp.size() == m_hnz_conf->getNumberCG())) {
    Logger::getLogger()->info("GI completed, push data to fledge.");
    m_hnz_connection->GI_completed();
    sendToFledge(m_gi_readings_temp);
    m_gi_readings_temp.clear();
  }
}

void HNZ::m_handleTMN(vector<Reading> &readings, unsigned char *data) {
  string msg_code = "TMN";
  // 2 or 4 TM inside a TMn
  unsigned int nbrTM = ((data[6] >> 7) == 1) ? 4 : 2;
  for (size_t i = 0; i < nbrTM; i++) {
    // 2 or 4 TM inside a TMn
    unsigned int msg_address =
        stoi(to_string((int)data[1]) + to_string(i * 4));  // ADTM + i*4
    string label = m_hnz_conf->getLabel(msg_code, msg_address);

    if (!label.empty()) {
      unsigned int value;
      unsigned int valid;

      if (nbrTM == 4) {
        int noctet = 2 + i;

        value = (int)(data[noctet]);        // Vi
        valid = (int)(data[6] >> i) & 0x1;  // Ii
      } else {
        int noctet = 2 + (i * 2);

        value = (int)(data[noctet + 1] << 8 |
                      data[noctet]);            // Concat V1/V2 and V3/V4
        valid = (int)(data[6] >> i * 2) & 0x1;  // I1 or I3
      }

      readings.push_back(m_prepare_reading(label, msg_code, m_remote_address,
                                           msg_address, value, valid, 0, 0, 0,
                                           0, false));
    }
  }
}

void HNZ::m_handleATVC(vector<Reading> &readings, unsigned char *data) {
  string msg_code = "ACK_TVC";

  unsigned int msg_address = data[1] & 0x1F;  // AD0

  m_hnz_connection->receivedCommandACK("TVC", msg_address);

  string label = m_hnz_conf->getLabel(msg_code, msg_address);

  if (!label.empty()) {
    unsigned int value_coding = (data[1] >> 5) & 0x1;  // X
    unsigned int a = (data[1] >> 6) & 0x1;             // A
    int value;

    if (value_coding == 1) {
      value = ((data[3] & 0xF) << 8) | data[2];
    } else {
      value = data[2] & 0x7F;
    }

    if (((data[3] >> 7) & 0x1) == 1) -1 * value;  // S

    readings.push_back(m_prepare_reading(label, msg_code, m_remote_address,
                                         msg_address, value, value_coding,
                                         true));
  }
}

void HNZ::m_handleATC(vector<Reading> &readings, unsigned char *data) {
  string msg_code = "ACK_TC";

  unsigned int msg_address = stoi(to_string((int)data[1]) +
                                  to_string((int)(data[2] >> 5)));  // AD0 + ADB

  m_hnz_connection->receivedCommandACK("TC", msg_address);

  string label = m_hnz_conf->getLabel(msg_code, msg_address);

  if (!label.empty()) {
    int value = data[2] & 0x7;

    readings.push_back(m_prepare_reading(label, msg_code, m_remote_address,
                                         msg_address, value, 0, false));
  }
}

Reading HNZ::m_prepare_reading(string label, string msg_code,
                               unsigned char station_addr, int msg_address,
                               int value, int valid, int ts, int ts_iv,
                               int ts_c, int ts_s, bool time) {
  Logger::getLogger()->debug(
      "Send to fledge " + msg_code +
      " with station address = " + to_string(station_addr) +
      ", message address = " + to_string(msg_address) +
      ", value = " + to_string(value) + ", valid = " + to_string(valid) +
      (time ? ("ts = " + to_string(ts) + ", iv = " + to_string(ts_iv) +
               ", c = " + to_string(ts_c) + ", s" + to_string(ts_s))
            : ""));

  auto *measure_features = new vector<Datapoint *>;
  measure_features->push_back(m_createDatapoint("do_type", msg_code));
  measure_features->push_back(
      m_createDatapoint("do_station", (long int)station_addr));
  measure_features->push_back(
      m_createDatapoint("do_addr", (long int)msg_address));
  measure_features->push_back(m_createDatapoint("do_value", (long int)value));
  measure_features->push_back(m_createDatapoint("do_valid", (long int)valid));

  if (time) {
    measure_features->push_back(m_createDatapoint("do_ts", (long int)ts));
    measure_features->push_back(m_createDatapoint("do_ts_iv", (long int)ts_iv));
    measure_features->push_back(m_createDatapoint("do_ts_c", (long int)ts_c));
    measure_features->push_back(m_createDatapoint("do_ts_s", (long int)ts_s));
  }

  DatapointValue dpv(measure_features, true);

  Datapoint *dp = new Datapoint("data_object", dpv);

  return Reading(label, dp);
}

Reading HNZ::m_prepare_reading(string label, string msg_code,
                               unsigned char station_addr, int msg_address,
                               int value, int value_coding, bool coding) {
  Logger::getLogger()->debug(
      "Send to fledge " + msg_code +
      " with station address = " + to_string(station_addr) +
      ", message address = " + to_string(msg_address) +
      ", value = " + to_string(value) +
      (coding ? ("value coding = " + to_string(value_coding)) : ""));

  auto *measure_features = new vector<Datapoint *>;
  measure_features->push_back(m_createDatapoint("do_type", msg_code));
  measure_features->push_back(
      m_createDatapoint("do_station", (long int)station_addr));
  measure_features->push_back(
      m_createDatapoint("do_addr", (long int)msg_address));
  measure_features->push_back(m_createDatapoint("do_value", (long int)value));

  if (coding) {
    // TODO : Review the name
    measure_features->push_back(
        m_createDatapoint("do_val_coding", (long int)value_coding));
  }

  DatapointValue dpv(measure_features, true);

  Datapoint *dp = new Datapoint("data_object", dpv);

  return Reading(label, dp);
}

void HNZ::sendToFledge(vector<Reading> &readings) {
  for (Reading &reading : readings) {
    ingest(reading);
  }
}

void HNZ::ingest(Reading &reading) { (*m_ingest)(m_data, reading); }

void HNZ::registerIngest(void *data, INGEST_CB cb) {
  m_ingest = cb;
  m_data = data;
}

bool HNZ::operation(const std::string &operation, int count,
                    PLUGIN_PARAMETER **params) {
  Logger::getLogger()->error("Operation %s", operation.c_str());

  if (operation.compare("TC") == 0) {
    int address = atoi(params[1]->value.c_str());
    int value = atoi(params[2]->value.c_str());

    m_hnz_connection->sendTCCommand(address, value);
    return true;
  } else if (operation.compare("TVC") == 0) {
    int address = atoi(params[1]->value.c_str());
    int value = atoi(params[2]->value.c_str());
    int val_coding = atoi(params[3]->value.c_str());

    m_hnz_connection->sendTVCCommand(address, value, val_coding);
    return true;
  }

  Logger::getLogger()->error("Unrecognised operation %s", operation.c_str());
  return false;
}