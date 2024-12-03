/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#ifndef HNZPath_H
#define HNZPath_H

#include <atomic>
#include <list>
#include <queue>
#include <thread>
#include <condition_variable>

#include <hnz_client.h>

#include "hnzconnection.h"

#define CONNECTION 0
#define CONNECTED 1

/**
 * @brief Structure containing internal informations about a message
 */
struct Message {
  /// Number in sending
  int ns;
  /// Payload of the message
  vector<unsigned char> payload;
  /// Timestamp of the sending
  uint64_t timestamp;
} typedef Message;

/**
 * @brief Structure containing internal informations about a command message.
 * Used when waiting for message acknowledgment.
 */
struct Command_message {
  /// Type of the command : TV or TVC
  string type;
  /// Address
  int addr;
  /// Max timestamp for acknowledgment
  uint64_t timestamp_max;
  /// Is the message acknowledged
  bool ack;
} typedef Command_message;

using namespace std::chrono;

class HNZConnection;

/**
 * @brief Class used to manage each HNZ path : connection to hnz_client,
 * send message, received message, manage message numbering, ...
 */
class HNZPath {
  // Give access to HNZPath private members for HNZConnection
  friend class HNZConnection;
 public:
  HNZPath(const std::shared_ptr<HNZConf> hnz_conf, HNZConnection* hnz_connection, bool secondary);
  ~HNZPath();

  string getName() const { return m_name_log; };

  /**
   * Connect (or re-connect) to the HNZ PA (TCP connection and HNZ connection
   * management if isn't started).
   */
  void connect();

  /**
   * Disconnect from the HNZ PA (TCP connection and HNZ connection management).
   */
  void disconnect();

  /**
   * Get the message(s) of the last HNZ frame received. Waits for the frame to
   * be received and then returns the message(s) inside.
   * @return message(s) received in the last HNZ frame
   */
  vector<vector<unsigned char>> getData();

  /**
   * Is the HNZ connection with the PA established and still alive?
   * @return true if connected, false otherwise
   */
  bool isHNZConnected() {
    std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex);
    return (m_protocol_state == CONNECTED) && isConnected();
  };

  /**
   * Is the TCP connection with the PA established and still alive?
   * @return true if connected, false otherwise
   */
  bool isConnected() { return m_connected && isTCPConnected(); };

  /**
   * Is the TCP connection with the PA still alive according to HNZ client?
   * @return true if connected, false otherwise
   */
  bool isTCPConnected();

  /**
   * Returns the number of times the last message was sent.
   */
  int getRepeat() const { return m_repeat; };

  /**
   * Resend a message that has already been sent but not acknowledged.
   * @param message The message to send back
   */
  void sendBackInfo(Message& message);

  /**
   * Send a TVC command.
   * @param address ADO
   * @param value value
   */
  bool sendTVCCommand(unsigned char address, int value);

  /**
   * Send a TC command.
   * @param address ADO + ADB
   * @param value value
   */
  bool sendTCCommand(unsigned char address, unsigned char value);

  /**
   * Received a TC or TVC ACK. Remove the command from the list of command sent.
   * @param type TC or TVC
   * @param addr Message address (ADO+ADB or ADO)
   */
  void receivedCommandACK(string type, int addr);

  /**
   * Send a general configuration request.
   */
  void sendGeneralInterrogation();

  /**
   * Go to connection state. Can be call when there is a problem with the path.
   * It will re-synchronize with the PA.
   */
  void go_to_connection();

  /**
   * Set the state of the path.
   */
  void setActivePath(bool active);

  /**
   * Gets the state of the path
   * @return true if active, false if passive
   */
  bool isActivePath() const { return m_is_active_path; }

  /**
   * Gets the state of the HNZ protocol (CONNECTION, CONNECTED)
   * @return CONNECTION if SARM/UA step is not complete, CONNECTED after that
   */
  int getProtocolState() const { 
    std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex);
    return m_protocol_state;
  }

 private:
  std::unique_ptr<HNZClient> m_hnz_client;  // HNZ Client that manage TCP connection
                                            // (receives/assembles and sends TCP frame)
  HNZConnection* m_hnz_connection = nullptr;

  deque<Message> msg_waiting;  // Queue of information messages not yet sent
  deque<Message> msg_sent;     // Queue of information messages already sent
  list<Command_message>
      command_sent;  // List of command already sent waiting to be ack

  long long last_sent_time = 0;     // Timestamp of the last information message sent
  int repeat_max = 0;          // max number of authorized repeats
  int gi_repeat = 0;       // number of time a GI is repeated
  long gi_start_time = 0;  // GI start time

  std::shared_ptr<std::thread> m_connection_thread; // Main thread that maintains the connection
  std::mutex m_connection_thread_mutex; // mutex to protect changes in m_connection_thread
  atomic<bool> m_is_running{true};  // If false, the connection thread will stop
  atomic<bool> m_connected{false};  // TCP Connection state with the PA
  
  std::mutex m_state_changed_mutex; // mutex to use condition variable below
  std::condition_variable m_state_changed_cond; // Condition variable used to notify changes in m_manageHNZProtocolConnection thread
  bool m_state_changed = false; // variable set to true when m_protocol_state changed
  // Initializing to CONNECTED ensures that the initial state transition from go_to_connection generates an audit
  int m_protocol_state = CONNECTED; // HNZ Protocol connection state
  mutable std::recursive_mutex m_protocol_state_mutex; // mutex to protect changes in m_protocol_state
  bool m_is_active_path = false;

  // Plugin configuration
  string m_ip;  // IP of the PA
  int m_port = 0;   // Port to connect to
  long long int m_timeoutUs = 0; // Timeout for socket recv in microseconds

  string m_name_log;   // Path name used in log
  string m_path_letter; // Path letter
  string m_path_name;  // Path name

  unsigned int m_remote_address = 0;
  unsigned char m_address_PA = 0;   // remote address + 1
  unsigned char m_address_ARP = 0;  // remote address + 3

  int m_max_sarm = 0;  // max number of SARM messages before handing over to the
                   // passive path
  int m_inacc_timeout = 0;   // timeout in seconds before declaring the remote server unreachable
  int m_repeat_timeout = 0;  // time allowed in ms for the receiver to acknowledge a frame
  int m_anticipation_ratio = 0;  // number of frames allowed to be received without
                             // acknowledgement
  unsigned int m_bulle_time = 0; // time in seconds before sending a BULLE mesage when no message have been sent on this path
  BulleFormat m_test_msg_receive;  // Payload of received BULLE
  BulleFormat m_test_msg_send;     // Payload of sent BULLE
  long long c_ack_time_max = 0;  // Max time to wait before receving a acknowledgement for a control command (in ms)

  // HNZ protocol related variable
  int m_nr = 0;   // Number in reception
  int m_ns = 0;   // Number in sending
  int m_NRR = 0;  // Received aquit number
  long long m_last_msg_time = 0;   // Timestamp of the last reception in ms
  long long m_last_msg_sent_time = 0;   // Timestamp of the last sent message in ms
  long long m_last_sarm_sent_time = 0; // Timestamp of the last sent SARM message in ms
  bool sarm_PA_received = false;  // The SARM sent by the PA was received
  bool sarm_ARP_UA = false;     // The UA sent by the PA (after receiving our SARM) was
                        // received
  int m_nbr_sarm_sent = 0;  // Number of SARM sent
  int m_repeat = 0;         // Number of times the sent message is repeated

  /**
   * Manage the HNZ protocol connection with the PA. Be careful, it doesn't
   * manage the TCP connection.
   */
  void m_manageHNZProtocolConnection();

  /**
   * Manage the HNZ protocol when connecting
   * @param now epoch time in ms
   * @return Number of miliseconds to sleep after this step
   */
  milliseconds m_manageHNZProtocolConnecting(long long now);

  /**
   * Manage the HNZ protocol when connected
   * @param now epoch time in ms
   * @return Number of miliseconds to sleep after this step
   */
  milliseconds m_manageHNZProtocolConnected(long long now);

  /**
   * Analyze a HNZ frame. If the frame is an information frame then we extract
   * its content and sends an acknowledgement, otherwise we return an empty
   * list/vector. Also manages the protocol aspect with SARM, UA and RR.
   * @param frReceived HNZ Frame to analyze
   * @return The list of informations messages contained in the frame.
   */
  vector<vector<unsigned char>> m_analyze_frame(MSG_TRAME* frReceived);

  /**
   * Extract the messages from the information frame payload.
   * @param data payload of the information frame
   * @param payloadSize size of the payload
   * @return The list of informations messages contained.
   */
  vector<vector<unsigned char>> m_extract_messages(unsigned char* data,
                                                   int payloadSize);

  /**
   * Call this method when a SARM message is received.
   */
  void m_receivedSARM();

  /**
   * Call this method to send a SARM message.
   */
  void m_sendSARM();

  /**
   * Call this method when a UA message is received.
   */
  void m_receivedUA();

  /**
   * Call this method to send a UA message.
   */
  void m_sendUA();

  /**
   * Call this method when a BULLE message is received.
   */
  void m_receivedBULLE();

  /**
   * Call this method to send a BULLE message.
   */
  void m_sendBULLE();

  /**
   * Call this method when a RR message is received.
   * @param nr NR of the RTU
   * @param repetition set to true if frame received is repeated
   * @return True if the NR contained in the message was correct, else false
   */
  bool m_receivedRR(int nr, bool repetition);

  /**
   * Send a RR
   * @param repetition set to true if frame received is repeated
   * @param ns NS of the received frame
   * @return True if received NR was valid and RR was sent, false if invalid NR was received and no RR was sent
   */
  bool m_sendRR(bool repetition, int ns, int nr);

  /**
   * Send an information frame. The address byte, numbering bit (containing NR,
   * NS) will be added by this method.
   * @param msg payload
   * @param size nubmer of byte in the payload
   * @return True if the message was sent, false if it was discarded
   */
  bool m_sendInfo(unsigned char* msg, unsigned long size);

  /**
   * Send a message immediately
   * @param message The message to send
   * @return True if the message was sent, false if it was discarded
   */
  bool m_sendInfoImmediately(Message message);

  /**
   * Send a date configuration message
   */
  void m_send_date_setting();

  /**
   * Send a time configuration message
   */
  void m_send_time_setting();

  /**
   * Go to the CONNECTED statue of the HNZ connection
   */
  void m_go_to_connected();

  /**
   * Get the other path if any
   * @return Second HNZ path, or nullptr if no other path defined
   */
  std::shared_ptr<HNZPath> m_getOtherPath() const;

  /**
   * Tells if the HNZ connection is fully established and active on the other path
   * @return True if the connection is established, false if not established or no other path defined
   */
  bool m_isOtherPathHNZConnected() const;

  /**
   * Called after sending a Command to store its information until a ACK is received, if the command was actually sent
   * @param type Type of command: TC or TVC
   * @param sent True if the command was sent to the HNZ device, else false
   * @param address Destination address of the command
   * @param value Value of the command
   * @param beforeLog Prefix for the log messages produced by this function
   */
  void m_registerCommandIfSent(const std::string& type, bool sent, unsigned char address, int value, const std::string& beforeLog);

  /**
   * Test if a NR is valid
   * @param nr NR of the RTU
   * @return True if the NR contained in the message was correct, else false
   */
  bool m_isNRValid(int nr) const;

  /**
   * Called to update internal values once a message containing a valid NR was received
   * @param nr NR of the RTU
   */
  void m_NRAccepted(int nr);

  /**
   * Returns mutex used to protect the protocol state from the other path,
   * if no other path is defined, returns a static mutex object instead
   * so that the return of this function can always be passed to a lock
   * @return Mutex protecting m_protocol_state from the other path, or static mutex
   */
  std::recursive_mutex& m_getOtherPathProtocolStateMutex() const;

  /**
   * Send a frame through the HNZ client and record the last send time
   * @param msg Bytes of the frame to send
   * @param msgSize Number of bytes in msg
   * @param usePAAddr If true, use PA address in the message, else use Center address
   * Protocol expect the following addresses when sending the following type of messages :
   * | Type | Expected Addr |
   * | ---- | ------------- |
   * | SARM | Center        |
   * | UA   | PA            |
   * | RR   | PA            |
   * | INFO | Center        |
   */
  void m_sendFrame(unsigned char *msg, unsigned long msgSize, bool usePAAddr = false);
};

#endif