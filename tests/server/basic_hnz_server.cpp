#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <future>

#include "basic_hnz_server.h"

void BasicHNZServer::startHNZServer() {
  printf("[HNZ Server][%d] Server starting...\n", m_port); fflush(stdout);
  if (server == nullptr) {
    server = new HNZServer();
  }
  std::lock_guard<std::mutex> guard(m_t1_mutex);
  m_t1 = new thread(&BasicHNZServer::m_start, server, m_port);
}

bool BasicHNZServer::joinStartThread() {
  // Lock to prevent multiple joins in parallel
  std::lock_guard<std::mutex> guard(m_t1_mutex);
  if (m_t1 != nullptr) {
    auto future = std::async(std::launch::async, &std::thread::join, m_t1);
    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
      // m_t1 did not join in time, abort
      printf("[HNZ Server][%d] Could not join m_t1, exiting\n", m_port); fflush(stdout);
      return false;
    }
    delete m_t1;
    m_t1 = nullptr;
  }
  return true;
}

void BasicHNZServer::stopHNZServer() {
  printf("[HNZ Server][%d] Server stopping...\n", m_port); fflush(stdout);
  is_running = false;
  
  // Ensure that m_t1 was joined no matter what
  joinStartThread();

  // Stop HNZ server before joining receiving_thread
  if (server != nullptr) {
    server->stop();
  }

  // Lock to wait for HNZServerIsReady to complete if it was running
  std::lock_guard<std::mutex> guard(m_init_mutex);
  if (receiving_thread != nullptr) {
    receiving_thread->join();
    delete receiving_thread;
    receiving_thread = nullptr;
  }
  
  // Free HNZ server after receiving_thread was joined as it uses that object
  if (server != nullptr) {
    delete server;
    server = nullptr;
  }
  printf("[HNZ Server][%d] Server stopped!\n", m_port); fflush(stdout);
}

void BasicHNZServer::receiving_loop() {
  while (is_running) {
    // Receive an hnz frame, this call is blocking
    MSG_TRAME *frReceived = (server->receiveFr());
    if (frReceived != nullptr) {
      // Checking the CRC
      if (server->checkCRC(frReceived)) {
        unsigned char *data = frReceived->aubTrame;
        unsigned char c = data[1];
        int len = 0;
        switch (c) {
          case UA_CODE:
            // printf("[HNZ Server][%d] UA received\n", m_port); fflush(stdout);
            len = 0;
            break;
          case SARM_CODE:
            // printf("[HNZ Server][%d] SARM received\n", m_port); fflush(stdout);
            len = 0;
            break;
          default:
            len = strlen((char *)data);
            break;
        }

        if (len != 0) {
          if (data[1] & 0x1) {
            // printf("> RR received : %d\n", (int)((data[1] >> 5) & 0x7)); fflush(stdout);
          } else {
            // printf("> Message received, code = %d\n", (int)data[2]); fflush(stdout);

            // Sending RR
            int f = (data[1] >> 4) & 0x1;
            m_nr = (((data[1] >> 1) & 0x7) + 1) % 8;
            // printf("< RR sent %d %s\n", m_nr, (f ? "rep" : "")); fflush(stdout);
            unsigned char num =
                static_cast<unsigned char>((m_nr << 5) + 1 + 0x10 * f);

            unsigned char message[1]{num};
            if (!ack_disabled) {
              createAndSendFrame(data[0], message, sizeof(message));
            }
          }
        }
      } else {
        printf("[HNZ Server][%d] The CRC does not match\n", m_port); fflush(stdout);
      }
      // Store the frame that was received for testing purposes
      onFrameReceived(frReceived);
    }
    if (!server->isConnected()) {
      printf("[HNZ Server][%d] TCP Connection lost, exit receiving_loop\n", m_port); fflush(stdout);
      is_running = false;
    }
  }
  printf("[HNZ Server][%d] receiving_loop terminated\n", m_port); fflush(stdout);
}

void BasicHNZServer::sendSARMLoop() {
  // Reset SARM/UA variables in case of reconnect
  {
    std::lock_guard<std::mutex> guard(m_sarm_ua_mutex);
    ua_ok = false;
    sarm_ok = false;
  }
  bool sarm_ua_ok = false;
  while (is_running && !sarm_ua_ok) {
    if(server->isConnected()) {
      sendSARM();
      std::lock_guard<std::mutex> guard(m_sarm_ua_mutex);
      ua_ok = false;
      sarm_ok = false;
    }
    this_thread::sleep_for(chrono::milliseconds(3000));
    std::lock_guard<std::mutex> guard(m_sarm_ua_mutex);
    sarm_ua_ok = ua_ok && sarm_ok;
  }
}

bool BasicHNZServer::waitForTCPConnection(int timeout_s) {
  long start = time(NULL);
  while (!server->isConnected() && is_running) {
    if (time(NULL) - start > timeout_s) {
      printf("[HNZ Server][%d] Connection timeout\n", m_port); fflush(stdout);
      is_running = false;
      break;
    }
    this_thread::sleep_for(chrono::milliseconds(500));
  }
  if (!joinStartThread()) {
    return false;
  }
  return true;
}

bool BasicHNZServer::HNZServerIsReady(int timeout_s /*= 16*/) {
  // Lock to prevent multiple calls to this function in parallel
  std::lock_guard<std::mutex> guard(m_init_mutex);
  if (receiving_thread) {
    printf("[HNZ Server][%d] Server already connected\n", m_port); fflush(stdout);
    return true;
  }
  is_running = true;
  long start = time(NULL);
  printf("[HNZ Server][%d] Waiting for initial connection...\n", m_port); fflush(stdout);
  // Wait for the server to finish starting
  if (!waitForTCPConnection(timeout_s)) {
    return false;
  }
  if (!is_running) {
    printf("[HNZ Server][%d] Not running after initial connection, exit\n", m_port); fflush(stdout);
    return false;
  }
  // Loop that sending sarm every 3s
  thread *m_t2 = new thread(&BasicHNZServer::sendSARMLoop, this);
  this_thread::sleep_for(chrono::milliseconds(100));
  // Wait for UA and send UA in response of SARM
  start = time(NULL);
  bool lastFrameWasEmpty = false;
  // Make sure to always exit this loop with is_running==false, not return
  // to ensure that m_t2 is joined properly
  while (is_running) {
    if (time(NULL) - start > timeout_s) {
      printf("[HNZ Server][%d] SARM/UA timeout\n", m_port); fflush(stdout);
      is_running = false;
      break;
    }
    if (!server->isConnected()) {
      printf("[HNZ Server][%d] Connection lost, restarting server...\n", m_port); fflush(stdout);
      // Server is still not connected? restart the server
      server->stop();
      startHNZServer();
      if (!waitForTCPConnection(timeout_s)) {
        is_running = false;
        break;
      }
      if (!is_running) {
        printf("[HNZ Server][%d] Not running after reconnection, exit\n", m_port); fflush(stdout);
        break;
      }
      start = time(NULL);
      printf("[HNZ Server][%d] Server reconnected!\n", m_port); fflush(stdout);
    }

    MSG_TRAME *frReceived = (server->receiveFr());
    if (frReceived == nullptr) {
      if (!lastFrameWasEmpty) {
        lastFrameWasEmpty = true;
        printf("[HNZ Server][%d] Received empty frame\n", m_port); fflush(stdout);
      }
      continue;
    }
    lastFrameWasEmpty = false;
    unsigned char *data = frReceived->aubTrame;
    unsigned char c = data[1];
    switch (c) {
      case UA_CODE:
        printf("[HNZ Server][%d] UA received\n", m_port); fflush(stdout);
        {
          std::lock_guard<std::mutex> guard2(m_sarm_ua_mutex);
          ua_ok = true;
        }
        break;
      case SARM_CODE:
        printf("[HNZ Server][%d] SARM received, sending UA\n", m_port); fflush(stdout);
        unsigned char message[1];
        message[0] = 0x63;
        createAndSendFrame(0x07, message, sizeof(message));
        {
          std::lock_guard<std::mutex> guard2(m_sarm_ua_mutex);
          sarm_ok = true;
        }
        break;
      default:
        printf("[HNZ Server][%d] Neither UA nor SARM: %d\n", m_port, static_cast<int>(c)); fflush(stdout);
        break;
    }
    // Store the frame that was received for testing purposes
    onFrameReceived(frReceived);
    std::lock_guard<std::mutex> guard2(m_sarm_ua_mutex);
    if (ua_ok && sarm_ok) break;
  }
  m_t2->join();
  delete m_t2;
  m_t2 = nullptr;
  if (!is_running) {
    printf("[HNZ Server][%d] Not running after SARM/UA, exit\n", m_port); fflush(stdout);
    return false;
  }
  printf("[HNZ Server][%d] Connection OK !!\n", m_port); fflush(stdout);

  // Connection established
  receiving_thread = new thread(&BasicHNZServer::receiving_loop, this);
  return true;
}

void BasicHNZServer::sendSARM() {
  printf("[HNZ Server][%d] Sending SARM...\n", m_port); fflush(stdout);
  unsigned char message[1];
  message[0] = 0x0F;
  createAndSendFrame(0x05, message, sizeof(message));
}

void BasicHNZServer::sendFrame(vector<unsigned char> message, bool repeat) {
  int num = (((repeat) ? (m_ns - 1) : m_ns) % 8) << 1 + ((m_nr + 1 << 5) % 8);
  message.insert(message.begin(), num);
  int len = message.size();
  createAndSendFrame(addr, message.data(), len);
  if (!repeat) m_ns++;
}

void BasicHNZServer::createAndSendFrame(unsigned char addr, unsigned char *msg, int msgSize) {
  // Code extracted from HNZClient::createAndSendFr of libhnz
  MSG_TRAME* pTrame = new MSG_TRAME;
  unsigned char msgWithAddr[msgSize + 1];
  // Add address
  msgWithAddr[0] = addr;
  // Add message
  memcpy(msgWithAddr + 1, msg, msgSize);
  server->addMsgToFr(pTrame, msgWithAddr, sizeof(msgWithAddr));
  server->setCRC(pTrame);
  // Store the frame about to be sent for testing purposes
  onFrameSent(pTrame);
  server->sendFr(pTrame);
}

std::vector<std::shared_ptr<MSG_TRAME>> BasicHNZServer::popLastFramesReceived() {
  std::lock_guard<std::mutex> guard(m_received_mutex);
  std::vector<std::shared_ptr<MSG_TRAME>> tmp = m_last_frames_received;
  m_last_frames_received.clear();
  return tmp;
}

std::vector<std::shared_ptr<MSG_TRAME>> BasicHNZServer::popLastFramesSent() {
  std::lock_guard<std::mutex> guard(m_sent_mutex);
  std::vector<std::shared_ptr<MSG_TRAME>> tmp = m_last_frames_sent;
  m_last_frames_sent.clear();
  return tmp;
}

void BasicHNZServer::onFrameReceived(MSG_TRAME* frame) {
  std::lock_guard<std::mutex> guard(m_received_mutex);
  std::shared_ptr<MSG_TRAME> newFrame = std::make_shared<MSG_TRAME>();
  memcpy(newFrame.get(), frame, sizeof(MSG_TRAME));
  m_last_frames_received.push_back(newFrame);
}

void BasicHNZServer::onFrameSent(MSG_TRAME* frame) {
  std::lock_guard<std::mutex> guard(m_sent_mutex);
  std::shared_ptr<MSG_TRAME> newFrame = std::make_shared<MSG_TRAME>();
  memcpy(newFrame.get(), frame, sizeof(MSG_TRAME));
  m_last_frames_sent.push_back(newFrame);
}

std::string BasicHNZServer::toHexStr(unsigned char num) {
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(num);
  return stream.str();
}

std::string BasicHNZServer::frameToStr(std::shared_ptr<MSG_TRAME> frame) {
  std::stringstream stream;
  stream << "\n[";
  for(int i=0 ; i<frame->usLgBuffer ; i++) {
    if (i > 0) {
      stream << ", ";
    }
    stream << toHexStr(frame->aubTrame[i]);
  }
  stream << "]";
  return stream.str();
}

std::string BasicHNZServer::framesToStr(std::vector<std::shared_ptr<MSG_TRAME>> frames) {
  std::stringstream stream;
  for(auto frame: frames) {
    stream << frameToStr(frame);
  }
  return stream.str();
}