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
#include <math.h>

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
   * @param repetition set to true if frame received is repeated
   */
  void receivedRR(int nr, bool repetition);

  /**
   * Check if the HNZ connection with RTU is good.
   */
  bool checkConnection() { return m_state == CONNECTED; }

  /**
   * Send a RR
   * @param repetition set to true if frame received is repeated
   * @param ns NS of the received frame
   */
  void sendRR(bool repetition, int ns, int nr);

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
  int m_NRR;       // Received aquit number

  // Plugin configuration
  unsigned char m_address_PA;   // remote address + 1
  unsigned char m_address_ARP;  // remote address + 3
  int m_max_sarm;  // max number of SARM messages before handing over to the
                   // passive path
  int m_inacc_timeout;   // timeout before declaring the remote server
                         // unreachable
  int m_repeat_timeout;  // time allowed for the receiver to acknowledge a frame
  int m_anticipation_ratio;  // number of frames allowed to be received without
                             // acknowledgement
  int m_repeat_max;          // max number of authorized repeats

  long m_last_msg_time;  // Timestamp of the last reception
  long m_last_sent;      // Timestamp of the last send

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
  void m_sendBackInfo(Message& message);

  /**
   * Send a message immediately
   * @param message The message to send
   */
  void m_sendInfoImmediately(Message message);

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
  void m_send_GI();

  void m_go_to_connection();
  void m_go_to_connected();

  HNZClient* m_client;  // HNZ Client (lib hnz)
};

#endif