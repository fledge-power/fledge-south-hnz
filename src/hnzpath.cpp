/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 */

#include <sstream>
#include <iomanip>

#include "hnzutility.h"
#include "hnz.h"
#include "hnzpath.h"

HNZPath::HNZPath(const std::shared_ptr<HNZConf> hnz_conf, HNZConnection* hnz_connection, int repeat_max, std::string ip, unsigned int port, std::string pathLetter):
                  // Path settings
                  m_hnz_client(make_unique<HNZClient>()),
                  m_hnz_connection(hnz_connection),
                  repeat_max(repeat_max-1), // -1 because m_repeat is incremented when a message is re-sent
                  m_ip(ip),
                  m_port(port),
                  m_timeoutUs(hnz_conf->get_cmd_recv_timeout()),
                  m_path_letter(pathLetter),
                  m_path_name(std::string("Path ") + m_path_letter),
                  // Global connection settings
                  m_remote_address(hnz_conf->get_remote_station_addr()),
                  m_address_PA(static_cast<unsigned char>((m_remote_address << 2) + 1)),
                  m_address_ARP(static_cast<unsigned char>((m_remote_address << 2) + 3)),
                  m_max_sarm(hnz_conf->get_max_sarm()),
                  m_inacc_timeout(hnz_conf->get_inacc_timeout()),
                  m_repeat_timeout(hnz_conf->get_repeat_timeout()),
                  m_anticipation_ratio(hnz_conf->get_anticipation_ratio()),
                  m_bulle_time(hnz_conf->get_bulle_time()),
                  m_test_msg_receive(hnz_conf->get_test_msg_receive()),
                  m_test_msg_send(hnz_conf->get_test_msg_send()),
                  // Command settings
                  c_ack_time_max(hnz_conf->get_c_ack_time() * 1000)
{
  m_refreshNameLog();
  // Send "dosconnected" audit at startup as there is no transition happening to trigger it here
  sendAuditFail();
  resolveProtocolStateConnection();
}

HNZPath::~HNZPath() {
  if (m_is_running) {
    disconnect();
  }
}

void HNZPath::protocolStateTransition(const ConnectionEvent event){
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::protocolStateTransition - " + m_name_log;
  if(m_protocolStateTransitionMap.find({m_protocol_state, event}) == m_protocolStateTransitionMap.end()){
    HnzUtility::log_warn(beforeLog + " Invalid protocol transition : event %s from %s", connectionEvent2str(event).c_str(), protocolState2str(m_protocol_state).c_str());
    return;
  }
  std::pair<ProtocolState, std::vector<void (HNZPath::*)()>> resolveTransition = m_protocolStateTransitionMap[{m_protocol_state, event}];

  HnzUtility::log_info(beforeLog + " Issuing protocol state transition %s : %s -> %s", connectionEvent2str(event).c_str(),
    protocolState2str(m_protocol_state).c_str(), protocolState2str(resolveTransition.first).c_str());
  // Here m_path_mutex might be locked within the scope of m_protocol_state_mutex lock, so lock both to avoid deadlocks
  // Same can happen if m_protocol_state_mutex from the other path gets locked later withing this function
  std::lock(m_protocol_state_mutex, m_hnz_connection->getPathMutex()); // Lock all mutexes simultaneously
  std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex, std::adopt_lock);
  std::lock_guard<std::recursive_mutex> lock2(m_hnz_connection->getPathMutex(), std::adopt_lock);

  bool state_changed = (m_protocol_state != resolveTransition.first);
  m_protocol_state = resolveTransition.first;
  for (auto triggeredAction: resolveTransition.second)
  {
    (this->*triggeredAction)();
  }

  if (state_changed) {
    // Notify m_manageHNZProtocolConnection thread that m_protocol_state changed
    std::unique_lock<std::mutex> lock4(m_state_changed_mutex);
    m_state_changed = true;
    m_state_changed_cond.notify_one();
  }
}

void HNZPath::stopTCP(){
  m_hnz_client->stop();
}

void HNZPath::resetSarmCounters(){
  m_nbr_sarm_sent = 0;
  // Reset time from last message received to prevent instant timeout
  m_last_msg_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void HNZPath::resetOutputVariables(){
  m_ns = 0;
  m_NRR = 0;
  m_repeat = 0;
}

void HNZPath::resetInputVariables(){
  m_nr = 0;
  m_last_msg_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
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
std::string convert_data_to_str(unsigned char* data, int len) {
  std::stringstream stream;
  for (int i = 0; i < len; i++) {
    if (i > 0) {
      stream << " ";
    }
    stream << std::setfill ('0') << std::setw(2) << std::hex << static_cast<unsigned int>(data[i]);
  }
  return stream.str();
}

/**
 * Helper method to convert message into something readable for logs.
 */
std::string convert_message_to_str(const Message& message) {
  std::stringstream stream;
  auto len = message.payload.size();
  for (int i = 0; i < len; i++) {
    if (i > 0) {
      stream << " ";
    }
    stream << std::setfill ('0') << std::setw(2) << std::hex << static_cast<unsigned int>(message.payload[i]);
  }
  return stream.str();
}

/**
 * Helper method to convert a list of message into something readable for logs.
 */
std::string convert_messages_to_str(const deque<Message>& messages) {
  std::string msgStr;
  for(const Message& msg: messages) {
    if (msgStr.size() > 0){
      msgStr += ", ";
    }
    msgStr += "[" + convert_message_to_str(msg) + "]";
  }
  return msgStr;
}

void HNZPath::sendAuditSuccess(){
  std::string activePassive = m_connection_state == ConnectionState::ACTIVE ? "active" : "passive";
  HnzUtility::audit_success("SRVFL", m_hnz_connection->getServiceName() + "-" + m_path_letter + "-" + activePassive);
}

void HNZPath::sendAuditFail(){
  HnzUtility::audit_fail("SRVFL", m_hnz_connection->getServiceName() + "-" + m_path_letter + "-disconnected");
}

void HNZPath::resolveProtocolStateConnected(){
  std::string beforeLogRoot = HnzUtility::NamePlugin + " - HNZPath::resolveProtocolStateConnected - ";
  std::string beforeLog = beforeLogRoot + m_name_log;
  std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex);

  // Ask to be active, HNZConnection will decide if this path can be active or passive
  m_hnz_connection->requestConnectionState(this, ConnectionState::ACTIVE);
  // m_name_log may have changed here so update log prefix
  beforeLog = beforeLogRoot + m_name_log;

  HnzUtility::log_debug(beforeLog + " HNZ Connection initialized !!");

  m_last_connected = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void HNZPath::resolveProtocolStateConnection(){
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::resolveProtocolStateConnection - " + m_name_log;
  HnzUtility::log_info(beforeLog + " Going to HNZ connection state... Waiting for a SARM.");

  m_hnz_connection->requestConnectionState(this, ConnectionState::DISCONNECTED);

  // Initialize internal variable
  resetInputVariables();
  resetOutputVariables();
  resetSarmCounters();
  m_last_sarm_sent_time = 0;
}

void HNZPath::discardMessages(){
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::discardMessages - " + m_name_log;
  if (!msg_sent.empty()) {
    std::string sentMsgStr = convert_messages_to_str(msg_sent);
    HnzUtility::log_debug(beforeLog + " Discarded unacknowledged messages sent: " + sentMsgStr);
    msg_sent.clear();
  }
  if (!msg_waiting.empty()) {
    std::string waitingMsgStr = convert_messages_to_str(msg_waiting);
    HnzUtility::log_debug(beforeLog + " Discarded messages waiting to be sent: " + waitingMsgStr);
    msg_waiting.clear();
  }
}

void HNZPath::connect() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::connect - " + m_name_log;
  // Reinitialize those variables in case of reconnection
  
  m_last_msg_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
  m_last_msg_sent_time = m_last_msg_time;
  m_is_running = true;
  // Loop until connected (make sure we exit if connection is shutting down)
  while (m_is_running && m_hnz_connection->isRunning()) {
    HnzUtility::log_info(beforeLog + " Connecting to PA on " + m_ip + " (" + to_string(m_port) + ")...");

    m_hnz_client->connect_Server(m_ip.c_str(), m_port, m_timeoutUs);

    // If shutdown started while waiting for connection, exit
    if(!m_is_running || !m_hnz_connection->isRunning()) {
      HnzUtility::log_info(beforeLog + " Connection shutting down, abort connect");
      return;
    }
    if (isTCPConnected()) {
      HnzUtility::log_info(beforeLog + " Connected to " + m_ip + " (" + to_string(m_port) + ").");
      std::lock_guard<std::mutex> lock(m_connection_thread_mutex);
      if (!m_connection_thread) {
        // Start the thread that manage the HNZ connection
        m_connection_thread = std::make_shared<std::thread>(&HNZPath::m_manageHNZProtocolConnection, this);
      }
      // Connection established, go to main loop
      return;
    }
    
    HnzUtility::log_warn(beforeLog +  " Error in connection, retrying in " + to_string(RETRY_CONN_DELAY) + "s ...");
    this_thread::sleep_for(std::chrono::seconds(RETRY_CONN_DELAY));
  }
}

void HNZPath::disconnect() {
  std::string beforeLogRoot = HnzUtility::NamePlugin + " - HNZPath::disconnect - ";
  std::string beforeLog = beforeLogRoot + m_name_log;
  HnzUtility::log_debug(beforeLog + " HNZ Path stopping...");
  // This ensures that the path is in the correct state for both south_event and audits
  protocolStateTransition(ConnectionEvent::TCP_CNX_LOST);
  // m_name_log may have changed here so update log prefix
  beforeLog = beforeLogRoot + m_name_log;

  m_is_running = false;
  m_hnz_client->stop();

  HnzUtility::log_debug(beforeLog + " HNZ client stopped");

  std::lock_guard<std::mutex> lock(m_connection_thread_mutex);
  if (m_connection_thread != nullptr) {
    // Notify m_manageHNZProtocolConnection thread that m_is_running changed
    {
      std::unique_lock<std::mutex> lock2(m_state_changed_mutex);
      m_state_changed_cond.notify_one();
    }
    HnzUtility::log_debug(beforeLog + " Waiting for the connection thread...");
    m_connection_thread->join();
    m_connection_thread = nullptr;
  }

  HnzUtility::log_info(beforeLog + " stopped !");
}

void HNZPath::m_manageHNZProtocolConnection() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_manageHNZProtocolConnection - " + m_name_log;
  auto sleep = std::chrono::milliseconds(1000);

  HnzUtility::log_debug(beforeLog + " HNZ Connection Management thread running");

  do {
    {
      // Here m_path_mutex might be locked within the scope of m_protocol_state_mutex lock, so lock both to avoid deadlocks
      std::lock(m_protocol_state_mutex, m_hnz_connection->getPathMutex()); // Lock both mutexes simultaneously
      std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex, std::adopt_lock);
      std::lock_guard<std::recursive_mutex> lock2(m_hnz_connection->getPathMutex(), std::adopt_lock);
      long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      sleep = m_manageHNZProtocolState(now);
    }
    // lock mutex (preparation to wait in cond. var.)
    std::unique_lock<std::mutex> lock3(m_state_changed_mutex);
    // unlock mutex and wait for sleep timeout or signaled state change
    m_state_changed_cond.wait_for(lock3, sleep, [this]() {
      return m_state_changed || (!m_is_running) || (!m_hnz_connection->isRunning());
    });
    m_state_changed = false;
    // Make sure we exit if connection is shutting down
  } while (m_is_running && m_hnz_connection->isRunning());

  HnzUtility::log_debug(beforeLog + " HNZ Connection Management thread is shutting down...");
}

std::chrono::milliseconds HNZPath::m_manageHNZProtocolState(long long now) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_manageHNZProtocolState - " + m_name_log;
  auto sleep = std::chrono::milliseconds(1000);
  // Must have received a SARM and an UA (in response to our SARM) from
  // the PA to be connected.
  if (m_protocol_state == ProtocolState::CONNECTION || m_protocol_state == ProtocolState::INPUT_CONNECTED) {
    if (now - m_last_msg_time <= (m_inacc_timeout * 1000)) {
      long long ms_since_last_sarm = now - m_last_sarm_sent_time;
      // Wait the appropriate time
      sleep = (ms_since_last_sarm >= m_repeat_timeout) * std::chrono::milliseconds(m_repeat_timeout) + 
              (ms_since_last_sarm < m_repeat_timeout) * std::chrono::milliseconds(m_repeat_timeout - ms_since_last_sarm);
      if (ms_since_last_sarm < m_repeat_timeout) return sleep;
      // Enough time elapsed since last SARM sent, send SARM
      if (m_nbr_sarm_sent == m_max_sarm) {
        HnzUtility::log_warn(beforeLog + " The maximum number of SARM was reached.");
        protocolStateTransition(ConnectionEvent::MAX_SARM_SENT);
      }
      // Send SARM and wait
      m_sendSARM();
    } else {
      // Inactivity timer reached
      HnzUtility::log_warn(beforeLog + " Inacc timeout! Reconnecting...");
      protocolStateTransition(ConnectionEvent::TO_RECV);
    }
  } else if (m_protocol_state == ProtocolState::CONNECTED || m_protocol_state == ProtocolState::OUTPUT_CONNECTED) {
    long long ms_since_last_msg = now - m_last_msg_time;
    long long ms_since_last_msg_sent = now - m_last_msg_sent_time;
    long long bulle_time_ms = m_bulle_time * 1000;
    // Enough time elapsed since last message sent, send BULLE
    if (ms_since_last_msg_sent >= bulle_time_ms) {
      m_sendBULLE();
      sleep = std::chrono::milliseconds(bulle_time_ms);
    }
    // Else wait until enough time passed
    else {
      sleep = std::chrono::milliseconds(bulle_time_ms - ms_since_last_msg_sent);
    }

    if (ms_since_last_msg > (m_inacc_timeout * 1000) && m_protocol_state == ProtocolState::CONNECTED) {
      HnzUtility::log_warn(beforeLog + " Inactivity timer reached, a message or a BULLE were not received on time.");
      protocolStateTransition(ConnectionEvent::TO_RECV);
      sleep = std::chrono::milliseconds(10);
    }
  }
  return sleep;
}

void HNZPath::sendInitMessages(){
  m_send_date_setting();
  m_send_time_setting();
  sendGeneralInterrogation();
  m_last_connected = 0;
}

void HNZPath::setConnectionState(ConnectionState newState) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::setConnectionState - " + m_name_log;
  // Only process updates (especially audits) when state changed
  if (newState != m_connection_state) {
    ConnectionState oldState = m_connection_state;
    m_connection_state = newState;
    m_refreshNameLog();
    HnzUtility::log_debug(beforeLog + " => " + m_name_log);

    // Transitions to ACTIVE or PASSIVE generate a success audit
    if ((newState == ConnectionState::PASSIVE) || (newState == ConnectionState::ACTIVE)) {
      sendAuditSuccess();
    }
    // Any other transition except those between DISCONNECTED and PENDING_HNZ generate a failure audit
    else if (((oldState != ConnectionState::PENDING_HNZ) && (newState == ConnectionState::DISCONNECTED))
          || ((oldState != ConnectionState::DISCONNECTED) && (newState == ConnectionState::PENDING_HNZ))) {
      sendAuditFail();
    }
  }
}

void HNZPath::setConnectionPending(){
  m_hnz_connection->requestConnectionState(this, ConnectionState::PENDING_HNZ);
}

vector<vector<unsigned char>> HNZPath::getData() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::getData - " + m_name_log;
  vector<vector<unsigned char>> messages;

  // Receive an hnz frame, this call is blocking
  MSG_TRAME* frReceived = (m_hnz_client->receiveFr());
  if (frReceived != nullptr) {
    // Checking the CRC
    if (m_hnz_client->checkCRC(frReceived)) {
      messages = m_analyze_frame(frReceived);
    } else {
      HnzUtility::log_warn(beforeLog + " The CRC does not match");
    }
  }

  return messages;
}

bool HNZPath::isTCPConnected() {
  return m_hnz_client->is_connected();
}

vector<vector<unsigned char>> HNZPath::m_analyze_frame(MSG_TRAME* frReceived) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_analyze_frame - " + m_name_log;
  vector<vector<unsigned char>> messages;
  unsigned char* data = frReceived->aubTrame;
  int size = frReceived->usLgBuffer;
  // Original command direction 0 => south to north, 1 => north to south
  unsigned char directionBit = (data[0] >> 1) & 0x01;
  unsigned char expectedDirectionBit = 0; // SARM, INFO => 0 | UA, RR => 1
  unsigned char address = data[0] >> 2;   // remote address
  unsigned char type = data[1];           // Message type

  HnzUtility::log_debug(beforeLog + " " + convert_data_to_str(data, size));

  if (m_remote_address == address) {
    // Branchless check if message is UA or RR (Supervision)
    expectedDirectionBit = 1*(type == UA_CODE) + 1*(type != SARM_CODE && type != UA_CODE && (type & 0x01) != 0);
    if(directionBit != expectedDirectionBit){
      HnzUtility::log_warn(beforeLog + " Invalid direction bit (A/B), found "+ to_string(directionBit) +" (expected "+ to_string(expectedDirectionBit) +").");
      return messages;
    }
    switch (type) {
      case UA_CODE:
        HnzUtility::log_info(beforeLog + " Received UA");
        m_receivedUA();
        break;
      case SARM_CODE:
        HnzUtility::log_info(beforeLog + " Received SARM");
        m_receivedSARM();
        break;
      default:
        if(m_protocol_state == ProtocolState::CONNECTION) break;
        // Get NR, P/F ans NS field
        int ns = (type >> 1) & 0x07;
        int pf = (type >> 4) & 0x01;
        int nr = (type >> 5) & 0x07;
        if ((type & 0x01) == 0) {
          m_receivedINFO(data, size, &messages);
        } else {
          // Supervision frame
          HnzUtility::log_info(beforeLog + " RR received (f = " + to_string(pf) + ", nr = " + to_string(nr) + ")");
          m_receivedRR(nr, pf == 1);
        }
        break;
    }
  } else {
    HnzUtility::log_warn(beforeLog + " The received address " + to_string(address) +
                        " don't match the configured address: " + to_string(m_remote_address));
  }
  return messages;
}

void HNZPath::m_receivedINFO(unsigned char* data, int size, vector<vector<unsigned char>>* messages){
  if(messages == nullptr) return;
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_receivedINFO - " + m_name_log;
  // Here m_path_mutex might be locked within the scope of m_protocol_state_mutex lock, so lock both to avoid deadlocks
  std::lock(m_protocol_state_mutex, m_hnz_connection->getPathMutex()); // Lock both mutexes simultaneously
  std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex, std::adopt_lock);
  std::lock_guard<std::recursive_mutex> lock2(m_hnz_connection->getPathMutex(), std::adopt_lock);
  unsigned char type = data[1];           // Message type (INFO)
  // Get NR, P/F ans NS field
  int ns = (type >> 1) & 0x07;
  int pf = (type >> 4) & 0x01;
  int nr = (type >> 5) & 0x07;
  if(m_protocol_state == ProtocolState::OUTPUT_CONNECTED){
    HnzUtility::log_warn(beforeLog + " Unexpected information frame received in partial connection state : OUTPUT_CONNECTED");
  } else {
    // Information frame
    HnzUtility::log_info(beforeLog + " Received an information frame (ns = " + to_string(ns) +
                                    ", p = " + to_string(pf) + ", nr = " + to_string(nr) + ")");
    std::lock_guard<std::recursive_mutex> lock3(m_hnz_connection->getPathMutex());
    if (m_hnz_connection->canPathExtractMessage(this)) {
      // Only the messages on the active path are extracted. The
      // passive path does not need them.
      int payloadSize = size - 4;  // Remove address, type, CRC (2 bytes)
      *messages = m_extract_messages(data + 2, payloadSize);
    }

    // Computing the frame number & sending RR
    if (!m_sendRR(pf == 1, ns, nr)) {
      // If NR was invalid, skip message processing
      messages->clear();
    }
  }
}

vector<vector<unsigned char>> HNZPath::m_extract_messages(unsigned char* data, int payloadSize) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_extract_messages - " + m_name_log;
  vector<vector<unsigned char>> messages;
  int len = 0;                // Length of message to push in Fledge
  unsigned char t = data[0];  // Payload type

  switch (t) {
    case TM4_CODE:
      HnzUtility::log_info(beforeLog + " Received TMA");
      len = 6;
      break;
    case TSCE_CODE:
      HnzUtility::log_info(beforeLog + " Received TSCE");
      len = 5;
      break;
    case TSCG_CODE:
      HnzUtility::log_info(beforeLog + " Received TSCG");
      len = 6;
      break;
    case TMN_CODE:
      HnzUtility::log_info(beforeLog + " Received TMN");
      len = 7;
      break;
    case MODULO_CODE:
      HnzUtility::log_info(beforeLog + " Received Modulo 10mn");
      len = 2;
      break;
    case TCACK_CODE:
      HnzUtility::log_info(beforeLog + " Received TC ACK");
      len = 3;
      break;
    case TVCACK_CODE:
      HnzUtility::log_info(beforeLog + " Received TVC ACK");
      len = 4;
      break;
    default:
      if (t == m_test_msg_receive.first &&
          data[1] == m_test_msg_receive.second) {
        HnzUtility::log_info(beforeLog + " Received BULLE");
        m_receivedBULLE();
        len = 2;
      } else {
        HnzUtility::log_info(beforeLog + "Received an unknown type");
        len = payloadSize;
      }
      break;
  }

  if (len != 0) {
    HnzUtility::log_debug(beforeLog + " [" + convert_data_to_str(data, len) + "]");

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
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_receivedSARM - " + m_name_log;
  m_sendUA();

  long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
  if(now - m_last_connected > m_repeat_timeout){
    protocolStateTransition(ConnectionEvent::RECEIVED_SARM);
  } else {
    HnzUtility::log_info(beforeLog + " Protocol state transition from CONNECTED ignored, a SARM was received too recently.");
  }
}

void HNZPath::m_receivedUA() {
  protocolStateTransition(ConnectionEvent::RECEIVED_UA);
}

void HNZPath::m_receivedBULLE() {
  m_last_msg_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

bool HNZPath::m_receivedRR(int nr, bool repetition) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_receivedRR - " + m_name_log;
  if (nr != m_NRR) {
    if (m_isNRValid(nr)) {
      if (!repetition || (m_repeat > 0)) {
        m_NRAccepted(nr);
      } else {
        HnzUtility::log_warn(beforeLog + " Received an unexpected repeated RR, ignoring it");
        return false;
      }
    } else {
      // invalid NR
      HnzUtility::log_warn(beforeLog + " Ignoring the RR, NR (" + to_string(nr) + ") is invalid. " +
                                      "Current NRR: " + to_string(m_NRR) + ", Current VS: " + to_string(m_ns));
      return false;
    }
  }
  else {
    HnzUtility::log_debug(beforeLog + " Received RR with NR=NRR (" + to_string(nr) + "), ignoring it");
  }
  return true;
}

bool HNZPath::m_isNRValid(int nr) const {
  // We want to test (m_NRR <= nr <= m_ns) modulo 8
  bool frameOk = true;
  // Case 0 (OK): m_NRR == nr <= m_ns
  if (nr != m_NRR) {
    // Case 1 (OK): m_NRR < nr <= m_ns
    frameOk = (m_NRR < nr) && (nr <= m_ns);
    if (m_ns < m_NRR) {
      // Case 2 (OK): m_ns < m_NRR < nr (m_ns wrapped left by modulo 8)
      if(m_NRR < nr) {
        frameOk = true;
      }
      // Case 3 (OK):  nr <= m_ns < m_NRR (m_NRR wrapped right by modulo 8)
      else if (nr <= m_ns) {
        frameOk = true;
      }
      // Case 4 (NOK): m_ns < nr < m_NRR (nr out of bounds)
    }
    // Case 5 (NOK): m_NRR < m_ns < nr (nr out of bounds)
    // Case 6 (NOK): nr < m_NRR < m_ns (nr out of bounds)
  }
  return frameOk;
}

void HNZPath::m_NRAccepted(int nr) {
  // valid NR, message(s) well received
  // remove them from msg sent list
  int modulo = 8;
  int nrOffset = (((nr - m_NRR) % modulo) + modulo) % modulo;
  for (size_t i = 0; i < nrOffset; i++) {
    if (!msg_sent.empty()) msg_sent.pop_front();
  }

  m_NRR = nr;
  m_repeat = 0;

  // Waiting for other RR, set timer
  if (!msg_sent.empty())
    last_sent_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::high_resolution_clock::now().time_since_epoch())
                          .count();

  // Sent message in waiting queue
  while (!msg_waiting.empty() &&
          (msg_sent.size() < m_anticipation_ratio)) {
    if (m_sendInfoImmediately(msg_waiting.front())) {
      msg_waiting.pop_front();
    }
  }
}

void HNZPath::m_sendSARM() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_sendSARM - " + m_name_log;
  unsigned char msg[1]{SARM_CODE};
  m_sendFrame(msg, sizeof(msg));
  HnzUtility::log_info(beforeLog + " SARM sent [" + to_string(m_nbr_sarm_sent + 1) + " / " + to_string(m_max_sarm) + "]");
  m_nbr_sarm_sent++;
  m_last_sarm_sent_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void HNZPath::m_sendUA() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_sendUA - " + m_name_log;
  unsigned char msg[1]{UA_CODE};
  m_sendFrame(msg, sizeof(msg), true);
  HnzUtility::log_info(beforeLog + " UA sent");
}

void HNZPath::m_sendBULLE() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_sendBULLE - " + m_name_log;
  unsigned char msg[2]{m_test_msg_send.first, m_test_msg_send.second};
  bool sent = m_sendInfo(msg, sizeof(msg));
  HnzUtility::log_info(beforeLog + " BULLE " + (sent?"sent":"discarded"));
}

bool HNZPath::m_sendRR(bool repetition, int ns, int nr) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_sendRR - " + m_name_log;
  // use NR to validate frames sent
  if(!m_receivedRR(nr, false)) {
    HnzUtility::log_warn(beforeLog + " Information frame contained unexpected NR (" + std::to_string(nr) + "), ignoring it");
    return false;
  }

  // send RR message
  if (ns == m_nr) {
    m_nr = (m_nr + 1) % 8;

    unsigned char msg[1];
    if (repetition) {
      msg[0] = 0x01 + m_nr * 0x20 + 0x10;
      HnzUtility::log_info(beforeLog + " RR sent with repeated=1");
    } else {
      msg[0] = 0x01 + m_nr * 0x20;
      HnzUtility::log_info(beforeLog + " RR sent");
    }

    m_sendFrame(msg, sizeof(msg), true);
  } else {
    if (repetition) {
      // Repeat the last RR
      unsigned char msg[1];
      msg[0] = 0x01 + m_nr * 0x20 + 0x10;
      m_sendFrame(msg, sizeof(msg), true);
      HnzUtility::log_info(beforeLog + " Repeat the last RR sent");
    } else {
      HnzUtility::log_warn(beforeLog + " The NS of the received frame (" + std::to_string(ns) + ") is not the expected one (" + std::to_string(m_nr) + ")");
    }
  }

  // Update timer
  m_last_msg_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
  return true;
}

bool HNZPath::m_sendInfo(unsigned char* msg, unsigned long size) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_sendInfo - " + m_name_log;
  std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex);
  if (m_protocol_state != ProtocolState::CONNECTED && !(m_protocol_state == ProtocolState::OUTPUT_CONNECTED && isBULLE(msg, size))) {
    HnzUtility::log_debug(beforeLog + " Connection is not yet fully established, discarding message ["
                        + convert_data_to_str(msg, static_cast<int>(size)) + "]");
    return false;
  }
  Message message;
  message.payload = vector<unsigned char>(msg, msg + size);

  if (msg_sent.size() < m_anticipation_ratio) {
    return m_sendInfoImmediately(message);
  } else {
    std::string waitingMsgStr = convert_messages_to_str(msg_sent);
    HnzUtility::log_debug(beforeLog + " Anticipation ratio reached (" + std::to_string(m_anticipation_ratio) + "), message ["
                        + convert_data_to_str(msg, static_cast<int>(size)) + "] will be delayed. Messages waiting: "
                        + waitingMsgStr);
    msg_waiting.push_back(message);
  }
  return false;
}

bool HNZPath::m_sendInfoImmediately(Message message) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_sendInfoImmediately - " + m_name_log;
  unsigned char* msg = &message.payload[0];
  int size = message.payload.size();
  std::lock_guard<std::recursive_mutex> lock(m_protocol_state_mutex);
  if (m_protocol_state != ProtocolState::CONNECTED && !(m_protocol_state == ProtocolState::OUTPUT_CONNECTED && isBULLE(msg, size))) {
    HnzUtility::log_debug(beforeLog + " Connection is not yet fully established, discarding message ["
                        + convert_data_to_str(msg, size) + "]");
    return false;
  }

  unsigned char msgWithNrNs[size + 1];
  memcpy(msgWithNrNs + 1, msg, size);

  msgWithNrNs[0] = m_nr * 0x20 + m_ns * 0x2;
  m_sendFrame(msgWithNrNs, sizeof(msgWithNrNs));

  // Set timer if there is not other message sent waiting for confirmation
  if (msg_sent.empty())
    last_sent_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();

  message.ns = m_ns;
  msg_sent.push_back(message);

  HnzUtility::log_debug(beforeLog + " Sent information frame: " +
                        convert_data_to_str(&m_address_ARP, 1) + " " + convert_data_to_str(msgWithNrNs, size + 1));

  m_ns = (m_ns + 1) % 8;
  return true;
}

void HNZPath::sendBackInfo(Message& message) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::sendBackInfo - " + m_name_log;

  unsigned char* msg = &message.payload[0];
  int size = message.payload.size();

  unsigned char msgWithNrNs[size + 1];
  memcpy(msgWithNrNs + 1, msg, size);

  m_repeat++;
  msgWithNrNs[0] = m_nr * 0x20 + 0x10 + message.ns * 0x2;
  m_sendFrame(msgWithNrNs, sizeof(msgWithNrNs));

  last_sent_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  
  HnzUtility::log_debug(beforeLog + " Resent information frame: " +
                        convert_data_to_str(&m_address_ARP, 1) + " " + convert_data_to_str(msgWithNrNs, size + 1));

}

void HNZPath::m_send_date_setting() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_send_date_setting - " + m_name_log;
  unsigned char msg[4];
  time_t now = time(0);
  tm* time_struct = gmtime(&now);
  msg[0] = SETDATE_CODE;
  msg[1] = time_struct->tm_mday;
  msg[2] = time_struct->tm_mon + 1;
  msg[3] = time_struct->tm_year % 100;
  bool sent = m_sendInfo(msg, sizeof(msg));
  HnzUtility::log_info(beforeLog + " Time setting " + (sent?"sent":"discarded") + " : " + to_string((int)msg[1]) + "/" +
                                  to_string((int)msg[2]) + "/" + to_string((int)msg[3]));
}

void HNZPath::m_send_time_setting() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::m_send_time_setting - " + m_name_log;
  long int ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::high_resolution_clock::now().time_since_epoch())
                          .count();
  long int ms_today = ms_since_epoch % 86400000;
  long int mod10m = ms_today / 600000;
  long int frac = (ms_today - (mod10m * 600000)) / 10;
  unsigned char msg[5];
  msg[0] = SETTIME_CODE;
  msg[1] = mod10m & 0xFF;
  msg[2] = frac >> 8;
  msg[3] = frac & 0xff;
  msg[4] = 0x00;
  bool sent = m_sendInfo(msg, sizeof(msg));
  m_hnz_connection->setDaySection(static_cast<unsigned char>(mod10m));
  HnzUtility::log_info(beforeLog + " Time setting " + (sent?"sent":"discarded") + " : mod10m = " + to_string(mod10m) +
                                  " and 10ms frac = " + to_string(frac) + " (" + to_string(mod10m / 6) +
                                  "h" + to_string((mod10m % 6) * 10) + "m and " + to_string(frac / 100) +
                                  "s " + to_string(frac % 100) + "ms");
}

void HNZPath::sendGeneralInterrogation() {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::sendGeneralInterrogation - " + m_name_log;
  unsigned char msg[2]{0x13, 0x01};
  bool sent = m_sendInfo(msg, sizeof(msg));
  HnzUtility::log_info(beforeLog + " GI (General Interrogation) request " + (sent?"sent":"discarded"));
  if(sent) m_hnz_connection->notifyGIsent();
}

bool HNZPath::sendTVCCommand(unsigned char address, int value) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::sendTVCCommand - " + m_name_log;
  unsigned char msg[4];
  msg[0] = TVC_CODE;
  msg[1] = (address & 0x1F);
  msg[2] = ((value >= 0) ? value : -value) & 0x7F;
  msg[3] = (value >= 0) ? 0 : 0x80;

  bool sent = m_sendInfo(msg, sizeof(msg));
  m_registerCommandIfSent("TVC", sent, static_cast<int>(address), value, beforeLog);
  return sent;
}

bool HNZPath::sendTCCommand(int address, unsigned char value) {
  std::string beforeLog = HnzUtility::NamePlugin + " - HNZPath::sendTCCommand - " + m_name_log;
  // Add a 0 in the string version to ensure that there is always 2 digits in the address
  string address_str = "0" + to_string(address);
  unsigned char msg[3];
  msg[0] = TC_CODE;
  msg[1] = stoi(address_str.substr(0, address_str.length() - 1));
  msg[2] = ((value & 0x3) << 3) | ((address_str.back() - '0') << 5);

  bool sent = m_sendInfo(msg, sizeof(msg));
  m_registerCommandIfSent("TC", sent, address, value, beforeLog);
  return sent;
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


void HNZPath::m_registerCommandIfSent(const std::string& type, bool sent, int address, int value, const std::string& beforeLog) {
  HnzUtility::log_info(beforeLog + " " + type + " " + (sent?"sent":"discarded") + " (address = " + to_string(address) + ", value = " + to_string(value) + ")");
  if (!sent) {
    return;
  }
  // Add the command in the list of commend sent (to check ACK later)
  Command_message cmd;
  cmd.timestamp_max = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::high_resolution_clock::now().time_since_epoch())
                          .count() +
                      c_ack_time_max;
  cmd.type = type;
  cmd.addr = address;
  // TVC command has a high priority
  command_sent.push_front(cmd);
}

void HNZPath::m_sendFrame(unsigned char *msg, unsigned long msgSize, bool usePAAddr /*= false*/) {
  m_hnz_client->createAndSendFr(usePAAddr ? m_address_PA : m_address_ARP, msg, static_cast<int>(msgSize));
  m_last_msg_sent_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

bool HNZPath::isBULLE(const unsigned char* msg, unsigned long size) const{
  return (size == 2) && (msg[0] == m_test_msg_send.first) && (msg[1] == m_test_msg_send.second);
}