#include "basic_hnz_server.h"

void BasicHNZServer::startHNZServer() {
  m_t1 = new thread(&BasicHNZServer::m_start, server, m_port);
}

bool BasicHNZServer::HNZServerIsReady() {
  long start = time(NULL);

  // Check server is connected
  while (!server->isConnected()) {
    if (time(NULL) - start > 10) {
      return false;
    }
    this_thread::sleep_for(chrono::milliseconds(500));
  }
  m_t1->join();
  delete m_t1;

  // Wait for UA and send UA in response of SARM
  bool ua_ok = false;
  bool sarm_ok = false;
  int cpt = 0;
  while (1) {
    unsigned char* data = server->receiveData();
    unsigned char c = data[1];
    if (cpt % 2 == 0) {
      sendSARM();
    }
    switch (c) {
      case UA_CODE:
        printf("[HNZ Server] UA received\n");
        ua_ok = true;
        break;
      case SARM_CODE:
        printf("[HNZ Server] SARM received, sending UA\n");
        unsigned char message[1];
        message[0] = 0x63;
        server->createAndSendFr(0x07, message, sizeof(message));
        sarm_ok = true;
        break;
      default:
        printf("[HNZ Server] Neither UA nor SARM\n");
        break;
    }
    cpt++;
    if (ua_ok && sarm_ok) break;
  }
  printf("[HNZ Server] Connection OK !!\n");

  return true;
}

void BasicHNZServer::sendSARM() {
  unsigned char message[1];
  message[0] = 0x0F;
  server->createAndSendFr(0x05, message, sizeof(message));
}

void BasicHNZServer::sendFrame(vector<unsigned char> message, bool repeat) {
  message.insert(message.begin(), (((repeat) ? (m_ns - 1) : m_ns) % 8) << 1);
  int len = message.size();
  server->createAndSendFr(addr, message.data(), len);
  if (!repeat) m_ns++;
}