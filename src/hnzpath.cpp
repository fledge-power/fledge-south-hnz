/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#include "hnzpath.h"

HNZPath::HNZPath(const HNZConf* hnz_conf, HNZConnection* hnz_connection, bool secondary):
                  // Path settings
                  m_hnz_client(make_unique<HNZClient>()),
                  m_hnz_connection(hnz_connection),
                  repeat_max((secondary ? hnz_conf->get_repeat_path_B() : hnz_conf->get_repeat_path_A())-1),
                  m_ip(secondary ? hnz_conf->get_ip_address_B() : hnz_conf->get_ip_address_A()),
                  m_port(secondary ? hnz_conf->get_port_B() : hnz_conf->get_port_A()),
                  m_timeoutUs(hnz_conf->get_cmd_recv_timeout()),
                  m_path_name(secondary ? "Path B" : "Path A"),
                  // Global connection settings
                  m_remote_address(hnz_conf->get_remote_station_addr()),
                  m_address_PA(static_cast<unsigned char>((m_remote_address << 2) + 1)),
                  m_address_ARP(static_cast<unsigned char>((m_remote_address << 2) + 3)),
                  m_max_sarm(hnz_conf->get_max_sarm()),
                  m_inacc_timeout(hnz_conf->get_inacc_timeout()),
                  m_repeat_timeout(hnz_conf->get_repeat_timeout()),
                  m_anticipation_ratio(hnz_conf->get_anticipation_ratio()),
                  m_test_msg_receive(hnz_conf->get_test_msg_receive()),
                  m_test_msg_send(hnz_conf->get_test_msg_send()),
                  // Command settings
                  c_ack_time_max(hnz_conf->get_c_ack_time() * 1000)
{
  setActivePath(!secondary);
  go_to_connection();
}

HNZPath::~HNZPath() {
  if (m_is_running) {
    disconnect();
  }
}

/**
 * Helper method to convert payload (unsigned char* with size) into a vector of
 * unsigned char.
 */
vector<unsigned char> convertPayloadToVector(unsigned char* data, int size) {
  vector<unsigned char> msg;
  for (size_t i = 0; i < size; i++) {
    msg.push_back(data[i]);
  }
  return msg;
}

/**
 * Helper method to convert payload into something readable for logs.
 */
string convert_data_to_str(unsigned char* data, int len) {
  string s = "";
  for (int i = 0; i < len; i++) {
    s += to_string(data[i]);
    if (i < len - 1) s += " ";
  }
  return s;
}

bool HNZPath::connect() {
  int i = 1;
  while (((i <= RETRY_CONN_NUM) or (RETRY_CONN_NUM == -1)) and m_is_running) {
    Logger::getLogger()->info(
        m_name_log + " Connecting to PA on " + m_ip + " (" + to_string(m_port) +
        ")... [" + to_string(i) + "/" + to_string(RETRY_CONN_NUM) + "]");

    // Establish TCP connection with the PA
    m_connected = !(m_hnz_client->connect_Server(m_ip.c_str(), m_port, m_timeoutUs));

    if (m_connected) {
      Logger::getLogger()->info(m_name_log + "Connected to " + m_ip + " (" +
                                to_string(m_port) + ").");

      if (m_connection_thread == nullptr) {
        // Start the thread that manage the HNZ connection
        m_connection_thread =
            new thread(&HNZPath::m_manageHNZProtocolConnection, this);
      }

      return true;
    } else {
      Logger::getLogger()->warn(m_name_log +
                                "Error in connection, retrying in " +
                                to_string(RETRY_CONN_DELAY) + "s ...");
      this_thread::sleep_for(std::chrono::seconds(RETRY_CONN_DELAY));
    }
    i++;
  }

  return false;
}

void HNZPath::disconnect() {
  Logger::getLogger()->debug(m_name_log + " HNZ Connection stopping...");

  m_is_running = false;
  m_connected = false;
  m_hnz_client->stop();

  if (m_connection_thread != nullptr) {
    // To avoid to be here at the same time, we put m_connection_thread =
    // nullptr
    thread* temp = m_connection_thread;
    m_connection_thread = nullptr;
    Logger::getLogger()->debug(m_name_log +
                               " Waiting for the connection thread");
    temp->join();
    delete temp;
  }

  Logger::getLogger()->info(m_name_log + " stopped !");
}

void HNZPath::m_manageHNZProtocolConnection() {
  milliseconds sleep = milliseconds(1000);
  long now;

  Logger::getLogger()->debug(m_name_log +
                             " HNZ Connection Management thread running");

  do {
    now = time(nullptr);

    switch (m_protocol_state) {
      case CONNECTION:
        // Must have received a SARM and an UA (in response to our SARM) from
        // the PA to be connected.
        if (!sarm_ARP_UA || !sarm_PA_received) {
          if (now - m_last_msg_time <= m_inacc_timeout) {
            if (m_nbr_sarm_sent == m_max_sarm) {
              Logger::getLogger()->warn(
                  m_name_log + " The maximum number of SARM was reached.");
              // If the path is the active one, switch to passive path if
              // available
              if (m_is_active_path) m_hnz_connection->switchPath();
              m_nbr_sarm_sent = 0;
            }
            // Send SARM and wait
            m_sendSARM();
            sleep = milliseconds(m_repeat_timeout);

          } else {
            // Inactivity timer reached
            Logger::getLogger()->error(m_name_log + " Inacc timeout !");
            // DF.GLOB.TS : nothing to do in HNZ
          }
        } else {
          m_protocol_state = CONNECTED;
          sleep = milliseconds(10);
        }
        break;
      case CONNECTED:
        if (now - m_last_msg_time <= m_inacc_timeout) {
          m_sendBULLE();
          sleep = milliseconds(10000);
        } else {
          Logger::getLogger()->warn(
              m_name_log +
              " Inactivity timer reached, a message or a BULLE were not "
              "received on time, back to SARM");
          go_to_connection();
          sleep = milliseconds(10);
        }
        break;
      default:
        Logger::getLogger()->debug(m_name_log + " STOP state");
        m_is_running = false;
        sleep = milliseconds(10);
        break;
    }

    this_thread::sleep_for(sleep);
  } while (m_is_running);

  Logger::getLogger()->debug(
      m_name_log + " HNZ Connection Management thread is shutting down...");
}

void HNZPath::go_to_connection() {
  Logger::getLogger()->warn(
      m_name_log + " Going to HNZ connection state... Waiting for a SARM.");
  m_protocol_state = CONNECTION;

  // Initialize internal variable
  sarm_PA_received = false;
  sarm_ARP_UA = false;
  m_nr = 0;
  m_ns = 0;
  m_NRR = 0;
  m_nbr_sarm_sent = 0;
  m_repeat = 0;
  m_last_msg_time = time(nullptr);
  gi_repeat = 0;
  gi_start_time = 0;

  // Put unacknowledged messages in the list of messages waiting to be sent
  if (!msg_sent.empty()) {
    while (!msg_sent.empty()) {
      msg_waiting.push_front(msg_sent.back());
      msg_sent.pop_back();
    }
  }
}

void HNZPath::m_go_to_connected() {
  m_protocol_state = CONNECTED;
  Logger::getLogger()->debug(m_name_log + " HNZ Connection initialized !!");

  if (m_is_active_path) {
    m_send_date_setting();
    m_send_time_setting();
    sendGeneralInterrogation();
  }
}

vector<vector<unsigned char>> HNZPath::getData() {
  vector<vector<unsigned char>> messages;

  // Receive an hnz frame, this call is blocking
  MSG_TRAME* frReceived = (m_hnz_client->receiveFr());
  if (frReceived != nullptr) {
    // Checking the CRC
    if (m_hnz_client->checkCRC(frReceived)) {
      messages = m_analyze_frame(frReceived);
    } else {
      Logger::getLogger()->warn(m_name_log + " The CRC does not match");
    }
  }

  return messages;
}

vector<vector<unsigned char>> HNZPath::m_analyze_frame(MSG_TRAME* frReceived) {
  vector<vector<unsigned char>> messages;
  unsigned char* data = frReceived->aubTrame;
  int size = frReceived->usLgBuffer;
  unsigned char address = data[0] >> 2;  // remote address
  unsigned char type = data[1];          // Message type

  Logger::getLogger()->debug(m_name_log + " " +
                             convert_data_to_str(data, size));

  if (m_remote_address == address) {
    switch (type) {
      case UA_CODE:
        Logger::getLogger()->info(m_name_log + " Received UA");
        m_receivedUA();
        break;
      case SARM_CODE:
        Logger::getLogger()->info(m_name_log + " Received SARM");
        m_receivedSARM();
        break;
      default:
        if (m_protocol_state != CONNECTION) {
          // Get NR, P/F ans NS field
          int ns = (type >> 1) & 0x07;
          int pf = (type >> 4) & 0x01;
          int nr = (type >> 5) & 0x07;
          if ((type & 0x01) == 0) {
            // Information frame
            Logger::getLogger()->info(
                m_name_log +
                " Received an information frame (ns = " + to_string(ns) +
                ", p = " + to_string(pf) + ", nr = " + to_string(nr) + ")");

            if (m_is_active_path) {
              // Only the messages on the active path are extracted. The
              // passive path does not need them.
              int payloadSize =
                  size - 4;  // Remove address, type, CRC (2 bytes)
              messages = m_extract_messages(data + 2, payloadSize);
            }

            // Computing the frame number & sending RR
            m_sendRR(pf == 1, ns, nr);
          } else {
            // Supervision frame
            Logger::getLogger()->warn(m_name_log +
                                      " RR received (f = " + to_string(pf) +
                                      ", nr = " + to_string(nr) + ")");
            m_receivedRR(nr, pf == 1);
          }
        }

        break;
    }
  } else {
    Logger::getLogger()->warn(m_name_log +
                              " The address don't match the configuration!");
  }
  return messages;
}

vector<vector<unsigned char>> HNZPath::m_extract_messages(unsigned char* data,
                                                          int payloadSize) {
  vector<vector<unsigned char>> messages;
  int len = 0;                // Length of message to push in Fledge
  unsigned char t = data[0];  // Payload type

  switch (t) {
    case TM4_CODE:
      Logger::getLogger()->info(m_name_log + " Received TMA");
      len = 6;
      break;
    case TSCE_CODE:
      Logger::getLogger()->info(m_name_log + " Received TSCE");
      len = 5;
      break;
    case TSCG_CODE:
      Logger::getLogger()->info(m_name_log + " Received TSCG");
      len = 6;
      break;
    case TMN_CODE:
      Logger::getLogger()->info(m_name_log + " Received TMN");
      len = 7;
      break;
    case MODULO_CODE:
      module10M = (int)data[1];
      Logger::getLogger()->info(m_name_log + " Received Modulo 10mn");
      len = 2;
      break;
    case TCACK_CODE:
      Logger::getLogger()->info(m_name_log + " Received TC ACK");
      len = 3;
      break;
    case TVCACK_CODE:
      Logger::getLogger()->info(m_name_log + " Received TVC ACK");
      len = 3;
      break;
    default:
      if (t == m_test_msg_receive.first &&
          data[1] == m_test_msg_receive.second) {
        Logger::getLogger()->info(m_name_log + " Received BULLE");
        m_receivedBULLE();
        len = 2;
      } else {
        Logger::getLogger()->info(m_name_log + "Received an unknown type");
      }
      break;
  }

  if (len != 0) {
    Logger::getLogger()->debug(m_name_log + " [" +
                               convert_data_to_str(data, len) + "]");

    // Extract the message from unsigned char* to vector<unsigned char>
    messages.push_back(convertPayloadToVector(data, len));

    // Check the length of the payload
    // There can be several messages in the same frame
    if (len != payloadSize) {
      // Analyze the rest of the payload
      vector<vector<unsigned char>> rest =
          m_extract_messages(data + len, payloadSize - len);

      messages.insert(messages.end(), rest.begin(), rest.end());
    }
  }
  return messages;
}

void HNZPath::m_receivedSARM() {
  if (m_protocol_state == CONNECTED) {
    // Reset HNZ protocol variables
    go_to_connection();
  }
  sarm_PA_received = true;
  sarm_ARP_UA = false;
  m_sendUA();
  module10M = 0;
}

void HNZPath::m_receivedUA() {
  sarm_ARP_UA = true;
  if (sarm_PA_received) {
    m_go_to_connected();
  }
}

void HNZPath::m_receivedBULLE() { m_last_msg_time = time(nullptr); }

void HNZPath::m_receivedRR(int nr, bool repetition) {
  if (nr != m_NRR) {
    int frameOk = (nr - m_NRR + 7) % 8 + 1;
    if (frameOk <= m_anticipation_ratio) {
      if (!repetition || (m_repeat > 0)) {
        // valid NR, message(s) well received
        // remove them from msg sent list
        for (size_t i = 0; i < frameOk; i++) {
          if (!msg_sent.empty()) msg_sent.pop_front();
        }

        m_NRR = nr;
        m_repeat = 0;

        // Waiting for other RR, set timer
        if (!msg_sent.empty())
          last_sent_time = duration_cast<milliseconds>(
                               system_clock::now().time_since_epoch())
                               .count();

        // Sent message in waiting queue
        while (!msg_waiting.empty() &&
               (msg_sent.size() < m_anticipation_ratio)) {
          m_sendInfoImmediately(msg_waiting.front());
          msg_waiting.pop_front();
        }
      } else {
        Logger::getLogger()->warn(
            m_name_log + " Received an unexpected repeated RR, ignoring it");
      }
    } else {
      // invalid NR
      Logger::getLogger()->warn(
          m_name_log + " Ignoring the RR, NR (=" + to_string(nr) +
          ") is invalid. Current NRR : " + to_string(m_NRR + 1));
    }
  }
}

void HNZPath::m_sendSARM() {
  unsigned char msg[1]{SARM_CODE};
  m_hnz_client->createAndSendFr(m_address_ARP, msg, sizeof(msg));
  Logger::getLogger()->info(m_name_log + " SARM sent [" +
                            to_string(m_nbr_sarm_sent + 1) + " / " +
                            to_string(m_max_sarm) + "]");
  m_nbr_sarm_sent++;
}

void HNZPath::m_sendUA() {
  unsigned char msg[1]{UA_CODE};
  m_hnz_client->createAndSendFr(m_address_PA, msg, sizeof(msg));
  Logger::getLogger()->info(m_name_log + " UA sent");
}

void HNZPath::m_sendBULLE() {
  unsigned char msg[2]{m_test_msg_send.first, m_test_msg_send.second};
  m_sendInfo(msg, sizeof(msg));
  Logger::getLogger()->info(m_name_log + " BULLE sent");
}

void HNZPath::m_sendRR(bool repetition, int ns, int nr) {
  // use NR to validate frames sent
  m_receivedRR(nr, 0);

  // send RR message
  if (ns == m_nr) {
    m_nr = (m_nr + 1) % 8;

    unsigned char msg[1];
    if (repetition) {
      msg[0] = 0x01 + m_nr * 0x20 + 0x10;
      Logger::getLogger()->info(m_name_log + " RR sent with repeated=1");
    } else {
      msg[0] = 0x01 + m_nr * 0x20;
      Logger::getLogger()->info(m_name_log + " RR sent");
    }

    m_hnz_client->createAndSendFr(m_address_PA, msg, sizeof(msg));
  } else {
    if (repetition) {
      // Repeat the last RR
      unsigned char msg[1];
      msg[0] = 0x01 + m_nr * 0x20 + 0x10;
      m_hnz_client->createAndSendFr(m_address_PA, msg, sizeof(msg));
      Logger::getLogger()->info(m_name_log + " Repeat the last RR sent");
    } else {
      Logger::getLogger()->warn(
          m_name_log + " The NS of the received frame is not the expected one");
    }
  }

  // Update timer
  m_last_msg_time = time(nullptr);
}

void HNZPath::m_sendInfo(unsigned char* msg, unsigned long size) {
  Message message;
  message.payload = vector<unsigned char>(msg, msg + size);

  if (msg_sent.size() < m_anticipation_ratio) {
    m_sendInfoImmediately(message);
  } else {
    msg_waiting.push_back(message);
  }
}

void HNZPath::m_sendInfoImmediately(Message message) {
  unsigned char* msg = &message.payload[0];
  int size = message.payload.size();

  unsigned char msgWithNrNs[size + 1];
  memcpy(msgWithNrNs + 1, msg, size);

  msgWithNrNs[0] = m_nr * 0x20 + m_ns * 0x2;
  m_hnz_client->createAndSendFr(m_address_ARP, msgWithNrNs,
                                sizeof(msgWithNrNs));

  // Set timer if there is not other message sent waiting for confirmation
  if (msg_sent.empty())
    last_sent_time =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count();

  message.ns = m_ns;
  msg_sent.push_back(message);

  m_ns = (m_ns + 1) % 8;
}

void HNZPath::sendBackInfo(Message& message) {
  unsigned char* msg = &message.payload[0];
  int size = message.payload.size();

  unsigned char msgWithNrNs[size + 1];
  memcpy(msgWithNrNs + 1, msg, size);

  m_repeat++;
  msgWithNrNs[0] = m_nr * 0x20 + 0x10 + message.ns * 0x2;
  m_hnz_client->createAndSendFr(m_address_ARP, msgWithNrNs,
                                sizeof(msgWithNrNs));

  last_sent_time =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
}

void HNZPath::m_send_date_setting() {
  unsigned char msg[4];
  time_t now = time(0);
  tm* time_struct = gmtime(&now);
  msg[0] = SETDATE_CODE;
  msg[1] = time_struct->tm_mday;
  msg[2] = time_struct->tm_mon + 1;
  msg[3] = time_struct->tm_year % 100;
  m_sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn(
      m_name_log + " Time setting sent : " + to_string((int)msg[1]) + "/" +
      to_string((int)msg[2]) + "/" + to_string((int)msg[3]));
}

void HNZPath::m_send_time_setting() {
  unsigned char msg[5];
  long int ms_since_epoch, mod10m, frac;
  ms_since_epoch = duration_cast<milliseconds>(
                       high_resolution_clock::now().time_since_epoch())
                       .count();
  long int ms_today = ms_since_epoch % 86400000;
  mod10m = ms_today / 600000;
  frac = (ms_today - (mod10m * 600000)) / 10;
  msg[0] = SETTIME_CODE;
  msg[1] = mod10m & 0xFF;
  msg[2] = frac >> 8;
  msg[3] = frac & 0xff;
  msg[4] = 0x00;
  m_sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn(
      m_name_log + " Time setting sent : mod10m = " + to_string(mod10m) +
      " and 10ms frac = " + to_string(frac) + " (" + to_string(mod10m / 6) +
      "h" + to_string((mod10m % 6) * 10) + "m and " + to_string(frac / 100) +
      "s " + to_string(frac % 100) + "ms");
}

void HNZPath::sendGeneralInterrogation() {
  unsigned char msg[2]{0x13, 0x01};
  m_sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn(m_name_log +
                            " GI (General Interrogation) request sent");
  gi_repeat++;
  gi_start_time = duration_cast<milliseconds>(
                      high_resolution_clock::now().time_since_epoch())
                      .count();
}

bool HNZPath::sendTVCCommand(unsigned char address, int value,
                             unsigned char val_coding) {
  unsigned char msg[4];
  msg[0] = TVC_CODE;
  msg[1] = (address & 0x1F) | ((val_coding & 0x1) << 5);
  if ((val_coding & 0x1) == 1) {
    msg[2] = value & 0xFF;
    msg[3] = ((value >= 0) ? 0 : 0x80) | value & 0xF00;
  } else {
    msg[2] = value & 0x7F;
    msg[3] = (value >= 0) ? 0 : 0x80;
  }

  m_sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn(m_name_log +
                            " TVC sent (address = " + to_string(address) +
                            ", value = " + to_string(value) +
                            " and value coding = " + to_string(val_coding));

  // Add the command in the list of commend sent (to check ACK later)
  Command_message cmd;
  cmd.timestamp_max = duration_cast<milliseconds>(
                          high_resolution_clock::now().time_since_epoch())
                          .count() +
                      c_ack_time_max;
  cmd.type = "TVC";
  cmd.addr = address;
  // TVC command has a high priority
  command_sent.push_front(cmd);

  return true;
}

bool HNZPath::sendTCCommand(unsigned char address, unsigned char value) {
  string address_str = to_string(address);
  unsigned char msg[3];
  msg[0] = TC_CODE;
  msg[1] = stoi(address_str.substr(0, address_str.length() - 1));
  msg[2] = ((value & 0x3) << 3) | ((address_str.back() - '0') << 5);

  m_sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn(m_name_log +
                            " TC sent (address = " + to_string(address) +
                            " and value = " + to_string(value));

  // Add the command in the list of commend sent (to check ACK later)
  Command_message cmd;
  cmd.timestamp_max = duration_cast<milliseconds>(
                          high_resolution_clock::now().time_since_epoch())
                          .count() +
                      c_ack_time_max;
  cmd.type = "TC";
  cmd.addr = address;

  // TC command has a high priority, we add it to the beginning of the queue
  command_sent.push_front(cmd);

  return true;
}

void HNZPath::receivedCommandACK(string type, int addr) {
  // Remove the command from the list of sent commands
  if (!command_sent.empty()) {
    list<Command_message>::iterator it = command_sent.begin();
    while (it != command_sent.end()) {
      if (it->type == type && it->addr == addr) {
        it = command_sent.erase(it);
      } else {
        ++it;
      }
    }
  }
}
