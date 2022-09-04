#include "hnzconnection.h"

HNZConnection::HNZConnection(HNZConf* m_hnz_conf, HNZClient* m_client1,
                             HNZ* m_hnz_fledge1) {
  m_client = m_client1;
  m_hnz_fledge = m_hnz_fledge1;
  unsigned int remote_address = m_hnz_conf->get_remote_station_addr();
  m_address_ARP = (remote_address << 2) + 3;
  m_address_PA = (remote_address << 2) + 1;
  m_max_sarm = m_hnz_conf->get_max_sarm();
  m_inacc_timeout = m_hnz_conf->get_inacc_timeout();
  m_repeat_timeout = m_hnz_conf->get_repeat_timeout();
  m_anticipation_ratio = m_hnz_conf->get_anticipation_ratio();
  m_repeat_max = m_hnz_conf->get_repeat_path_A() - 1;
  gi_repeat_count_max = m_hnz_conf->get_gi_repeat_count();
  gi_time_max = m_hnz_conf->get_gi_time() * 1000;
  m_is_running = false;
  m_go_to_connection();
}

void HNZConnection::start() {
  Logger::getLogger()->debug("HNZ Connection starting...");
  m_is_running = true;
  m_connection_thread = new thread(&HNZConnection::manageConnection, this);
  m_messages_thread = new thread(&HNZConnection::manageMessages, this);
}

void HNZConnection::stop() {
  Logger::getLogger()->info("HNZ Connection stopping...");
  m_is_running = false;
  if (m_connection_thread != nullptr) {
    Logger::getLogger()->debug("Waiting for the connection thread");
    m_connection_thread->join();
    m_connection_thread = nullptr;
  }
  if (m_messages_thread != nullptr) {
    Logger::getLogger()->debug("Waiting for the messages thread");
    m_messages_thread->join();
    m_messages_thread = nullptr;
  }
  Logger::getLogger()->info("HNZ Connection stoped !");
}

void HNZConnection::manageConnection() {
  milliseconds sleep = milliseconds(1000);
  long now;

  Logger::getLogger()->debug("HNZ Connection - Start looping");

  do {
    now = time(nullptr);

    switch (m_state) {
      case CONNECTION:
        if (!sarm_ARP_UA || !sarm_PA_received) {
          if (now - m_last_msg_time <= m_inacc_timeout) {
            if (m_nbr_sarm_sent < m_max_sarm) {
              m_sendSARM();
              sleep = milliseconds(m_repeat_timeout);
            } else {
              Logger::getLogger()->debug(
                  "The maximum number of SARM was reached.");
              m_is_running = false;
              sleep = milliseconds(10);
            }
          } else {
            Logger::getLogger()->error("Inacc timeout !");
            // TODO : Send DF.GLOB.TS
          }
        } else {
          Logger::getLogger()->debug("Connection initialized !!");
          m_go_to_connected();
          sleep = milliseconds(10);
        }
        break;
      case CONNECTED:
        if (now - m_last_msg_time <= m_inacc_timeout) {
          m_sendBULLE();
          sleep = milliseconds(10000);
        } else {
          Logger::getLogger()->warn("BULLE not received, back to SARM");
          m_go_to_connection();
          sleep = milliseconds(10);
        }
        break;
      default:
        Logger::getLogger()->debug("STOP state");
        m_is_running = false;
        sleep = milliseconds(10);
        break;
    }

    this_thread::sleep_for(sleep);
  } while (m_is_running);

  Logger::getLogger()->info("HNZ Connection Management is shutting down...");
}

void HNZConnection::manageMessages() {
  uint64_t current;
  do {
    if (m_state == CONNECTED) {
      current =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count();
      // Manage repeat/timeout
      if (!m_msg_sent.empty()) {
        Message& msg = m_msg_sent.front();
        if (m_last_sent + m_repeat_timeout < current) {
          if (m_repeat >= m_repeat_max) {
            // Connection disrupted, back to SARM
            Logger::getLogger()->warn("Connection disrupted, back to SARM");

            m_go_to_connection();
          } else {
            // Repeat the message
            Logger::getLogger()->warn(
                "Timeout, sending back first unacknowledged message");
            m_sendBackInfo(msg);

            // Move other unacknowledged messages to waiting queue
            while (m_msg_sent.size() > 1) {
              m_msg_waiting.push_front(m_msg_sent.back());
              m_msg_sent.pop_back();
            }
          }
        }
      }

      // GI support
      if (m_gi_repeat != 0) {
        if (m_gi_start + gi_time_max < current) {
          // GI not completed in time
          if (m_gi_repeat > gi_repeat_count_max) {
            // GI failed
            Logger::getLogger()->error("General Interrogation FAILED !");
            m_gi_repeat = 0;
            // TODO : send TS DF.GLOB.TS
          } else {
            Logger::getLogger()->warn(
                "General Interrogation Timeout, repeat GI");
            // Clean queue in HNZ class
            m_hnz_fledge->resetGIQueue();
            // Send a new GI
            m_send_GI();
          }
        }
      }
    }

    this_thread::sleep_for(milliseconds(100));
  } while (m_is_running);
}

void HNZConnection::m_go_to_connection() {
  Logger::getLogger()->warn("[HNZ Connection] Going to connection state...");
  // TODO : add timer inacc ?
  m_state = CONNECTION;
  sarm_PA_received = false;
  sarm_ARP_UA = false;
  m_nr = 0;
  m_ns = 0;
  m_NRR = 0;
  m_nbr_sarm_sent = 0;
  m_repeat = 0;
  m_gi_start = 0;
  m_gi_repeat = 0;
  m_last_msg_time = time(nullptr);

  // Put unacknowledged messages in the list of messages waiting to be sent
  if (!m_msg_sent.empty()) {
    while (!m_msg_sent.empty()) {
      m_msg_waiting.push_front(m_msg_sent.back());
      m_msg_sent.pop_back();
    }
  }
}

void HNZConnection::m_go_to_connected() {
  m_send_date_setting();
  m_send_time_setting();
  m_send_GI();
  m_state = CONNECTED;
}

void HNZConnection::receivedSARM() {
  Logger::getLogger()->info("[HNZ Connection] SARM Received");
  m_go_to_connection();
  sarm_PA_received = true;
  m_sendUA();
}

void HNZConnection::receivedUA() {
  Logger::getLogger()->info("[HNZ Connection] UA Received");
  sarm_ARP_UA = true;
}

void HNZConnection::receivedBULLE() { m_last_msg_time = time(nullptr); }

void HNZConnection::receivedRR(int nr, bool repetition) {
  if (nr != m_NRR) {
    int frameOk = (nr - m_NRR + 7) % 8 + 1;
    if (frameOk <= m_anticipation_ratio) {
      if (!repetition || (m_repeat > 0)) {
        // valid NR, message(s) well received
        // remove them from msg sent list
        for (size_t i = 0; i < frameOk; i++) {
          if (!m_msg_sent.empty()) m_msg_sent.pop_front();
        }

        m_NRR = nr;
        m_repeat = 0;

        // Waiting for other RR, set timer
        if (!m_msg_sent.empty())
          m_last_sent = duration_cast<milliseconds>(
                            system_clock::now().time_since_epoch())
                            .count();

        // Sent message in waiting queue
        while (!m_msg_waiting.empty() &&
               (m_msg_sent.size() < m_anticipation_ratio)) {
          m_sendInfoImmediately(m_msg_waiting.front());
          m_msg_waiting.pop_front();
        }
      } else {
        Logger::getLogger()->warn(
            "Received an unexpected repeated RR, ignoring it");
      }
    } else {
      // invalid NR
      Logger::getLogger()->warn(
          "Ignoring the RR, NR (=" + to_string(nr) +
          ") is invalid. Current NRR : " + to_string(m_NRR + 1));
    }
  }
}

void HNZConnection::m_sendSARM() {
  unsigned char msg[1]{SARM};
  m_client->createAndSendFr(m_address_ARP, msg, sizeof(msg));
  Logger::getLogger()->info("SARM sent");
  m_nbr_sarm_sent++;
}

void HNZConnection::m_sendUA() {
  unsigned char msg[1]{UA};
  m_client->createAndSendFr(m_address_PA, msg, sizeof(msg));
  Logger::getLogger()->info("UA sent");
}

void HNZConnection::m_sendBULLE() {
  unsigned char msg[2]{0x13, 0x04};
  sendInfo(msg, sizeof(msg));
  Logger::getLogger()->info("BULLE sent");
}

void HNZConnection::sendRR(bool repetition, int ns, int nr) {
  // use NR to validate frames sent
  receivedRR(nr, 0);

  // send RR message
  if (ns == m_nr) {
    m_nr = (m_nr + 1) % 8;

    unsigned char msg[1];
    if (repetition) {
      msg[0] = 0x01 + m_nr * 0x20 + 0x10;
      Logger::getLogger()->info("RR sent with repeated=1");
    } else {
      msg[0] = 0x01 + m_nr * 0x20;
      Logger::getLogger()->info("RR sent");
    }

    m_client->createAndSendFr(m_address_PA, msg, sizeof(msg));
  } else {
    if (repetition) {
      // Repeat the last RR
      unsigned char msg[1];
      msg[0] = 0x01 + m_nr * 0x20 + 0x10;
      m_client->createAndSendFr(m_address_PA, msg, sizeof(msg));
      Logger::getLogger()->info("Repeat the last RR sent");
    } else {
      Logger::getLogger()->warn(
          "The NS of the received frame is not the expected one");
    }
  }

  // Update timer
  m_last_msg_time = time(nullptr);
}

void HNZConnection::sendInfo(unsigned char* msg, unsigned long size) {
  Message message;
  message.payload = vector<unsigned char>(msg, msg + size);

  if (m_msg_sent.size() < m_anticipation_ratio) {
    m_sendInfoImmediately(message);
  } else {
    m_msg_waiting.push_back(message);
  }
}

void HNZConnection::m_sendInfoImmediately(Message message) {
  unsigned char* msg = &message.payload[0];
  int size = message.payload.size();

  unsigned char msgWithNrNs[size + 1];
  memcpy(msgWithNrNs + 1, msg, size);

  msgWithNrNs[0] = m_nr * 0x20 + m_ns * 0x2;
  m_client->createAndSendFr(m_address_ARP, msgWithNrNs, sizeof(msgWithNrNs));

  // Set timer if there is not other message sent waiting for confirmation
  if (m_msg_sent.empty())
    m_last_sent =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count();

  message.ns = m_ns;
  m_msg_sent.push_back(message);

  m_ns = (m_ns + 1) % 8;
}

void HNZConnection::m_sendBackInfo(Message& message) {
  unsigned char* msg = &message.payload[0];
  int size = message.payload.size();

  unsigned char msgWithNrNs[size + 1];
  memcpy(msgWithNrNs + 1, msg, size);

  m_repeat++;
  msgWithNrNs[0] = m_nr * 0x20 + 0x10 + message.ns * 0x2;
  m_client->createAndSendFr(m_address_ARP, msgWithNrNs, sizeof(msgWithNrNs));

  m_last_sent =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
}

void HNZConnection::m_send_date_setting() {
  unsigned char msg[4];
  time_t now = time(0);
  tm* time_struct = gmtime(&now);
  msg[0] = 0x1c;
  msg[1] = time_struct->tm_mday;
  msg[2] = time_struct->tm_mon + 1;
  msg[3] = time_struct->tm_year % 100;
  sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn("Time setting sent : " + to_string((int)msg[1]) +
                            "/" + to_string((int)msg[2]) + "/" +
                            to_string((int)msg[3]));
}

void HNZConnection::m_send_time_setting() {
  unsigned char msg[5];
  long int ms_since_epoch, mod10m, frac;
  ms_since_epoch = duration_cast<milliseconds>(
                       high_resolution_clock::now().time_since_epoch())
                       .count();
  long int ms_today = (ms_since_epoch % 86400000);
  mod10m = ms_today / 600000;
  frac = (ms_today - (mod10m * 600000)) / 10;
  msg[0] = 0x1d;
  msg[1] = mod10m & 0xFF;
  msg[2] = frac >> 8;
  msg[3] = frac & 0xff;
  msg[4] = 0x00;
  sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn(
      "Time setting sent : mod10m = " + to_string(mod10m) +
      " and 10ms frac = " + to_string(frac) + " (" + to_string(mod10m / 6) +
      "h" + to_string((mod10m % 6) * 10) + "m and " + to_string(frac / 100) +
      "s " + to_string(frac % 100) + "ms");
}

void HNZConnection::m_send_GI() {
  unsigned char msg[2]{0x13, 0x01};
  sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn("GI (General interrogation) request sent");
  m_gi_repeat++;
  m_gi_start = duration_cast<milliseconds>(
                   high_resolution_clock::now().time_since_epoch())
                   .count();
}

bool HNZConnection::sendTVCCommand(unsigned char address, int value,
                                   unsigned char val_coding) {
  unsigned char msg[4];
  msg[0] = 0x1A;
  msg[1] = (address & 0x1F) | ((val_coding & 0x1) << 5);
  if ((val_coding & 0x1) == 1) {
    msg[2] = value & 0xFF;
    msg[3] = ((value >= 0) ? 0 : 0x80) | value & 0xF00;
  } else {
    msg[2] = value & 0x7F;
    msg[3] = (value >= 0) ? 0 : 0x80;
  }
  // TODO : Check later for priority / ACK_TVC
  sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn("TVC sent (address = " + to_string(address) +
                            ", value = " + to_string(value) +
                            " and value coding = " + to_string(val_coding));
}

bool HNZConnection::sendTCCommand(unsigned char address, unsigned char value) {
  string address_str = to_string(address);
  unsigned char msg[3];
  msg[0] = 0x19;
  msg[1] = stoi(address_str.substr(0, address_str.length() - 2));
  msg[2] = ((value & 0x3) << 3) | ((address_str.back() - '0') << 5);
  // TODO : Check later for priority / ACK_TC
  sendInfo(msg, sizeof(msg));
  Logger::getLogger()->warn("TC sent (address = " + to_string(address) +
                            " and value = " + to_string(value));
}
