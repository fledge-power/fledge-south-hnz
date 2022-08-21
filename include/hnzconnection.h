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

#include <logger.h>

#include <queue>

#include "../../libhnz/src/inc/hnz_client.h"
#include "hnzconf.h"

#define CONNECTION 0
#define CONNECTED 1

using namespace std;
using namespace std::chrono;

struct Message {
  int ns;
  vector<unsigned char> payload;
  uint64_t timestamp;
} typedef Message;

class HNZConnection {
 public:
  HNZConnection(HNZConf* m_hnz_conf, HNZClient* m_client);
  ~HNZConnection();

  /**
   * Manage the connection with RTU.
   */
  void manageConnection();

  void manageMessages();

  void start();
  void stop();

  /**
   * Call this method when a SARM message is received.
   */
  void receivedSARM();

  /**
   * Call this method when a UA message is received.
   */
  void receivedUA();

  /**
   * Call this method when a BULLE message is received.
   */
  void receivedBULLE();

  /**
   * Call this method when a RR message is received.
   * @param nr NR of the RTU
   * @param repetition ???
   */
  void receivedRR(int nr, bool repetition);

  /**
   * Check if the HNZ connection with RTU is good.
   */
  bool checkConnection() { return m_state == CONNECTED; }

  /**
   * Send a RR
   * @param repetition ???
   */
  void sendRR(bool repetition, int ns);

  /**
   * Send an information frame. The address byte, numbering bit (containing NR,
   * NS) will be added by this method.
   * @param msg payload
   * @param size nubmer of byte in the payload
   */
  void sendInfo(unsigned char* msg, unsigned long size);

 private:
  thread* m_connection_thread;  // Main thread that maintains the connection
  thread* m_messages_thread;    // Main thread that monitors messages
  atomic<bool> m_is_running;    // If false, the connection thread will stop

  int m_nr, m_ns;  // Number in reception
  int m_nr_PA;     // Number in reception of the PA

  // Plugin configuration
  unsigned char m_address_PA;   // remote address + 1
  unsigned char m_address_ARP;  // remote address + 3
  int m_max_sarm;
  int m_inacc_timeout;
  int m_repeat_timeout;
  int m_anticipation_ratio;
  int m_repeat_max;

  long m_last_received_bulle;  // timestamp of the last reception of a BULLE

  bool sarm_PA_received;  // The SARM sent by the PA was received
  bool sarm_ARP_UA;  // The UA sent by the PA after receiving SARM was received
  int m_nbr_sarm_sent;  // Number of SARM sent

  int m_state;                   // Connection state
  int m_repeat;                  // Number of times the sent message is repeated
  deque<Message> m_msg_sent;     // Queue of information messages already sent
  deque<Message> m_msg_waiting;  // Queue of information messages not yet sent

  /**
   * Resend a message that has already been sent but not acknowledged.
   * @param message The message to send back
   */
  void sendBackInfo(Message& message);

  /**
   * Send a message immediately
   * @param message The message to send
   */
  void sendInfoImmediately(Message message);

  void m_sendSARM();

  void m_sendBULLE();

  void m_sendUA();

  /**
   * Send a date configuration message
   */
  void m_send_date_setting();

  /**
   * Send a time configuration message
   */
  void m_send_time_setting();

  /**
   * Send a general configuration request
   */
  void m_send_CG();

  void m_go_to_connection();
  void m_go_to_connected();

  HNZClient* m_client;  // HNZ Client (lib hnz)
};

#endif