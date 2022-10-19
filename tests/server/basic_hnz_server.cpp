#include "basic_hnz_server.h"

void BasicHNZServer::startHNZServer() {
  m_t1 = new thread(&BasicHNZServer::m_start, server, m_port);
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
            // printf("UA received\n");
            len = 0;
            break;
          case SARM_CODE:
            // printf("SARM received\n");
            len = 0;
            break;
          default:
            len = strlen((char *)data);
            break;
        }

        if (len != 0) {
          if (data[1] & 0x1) {
            // printf("> RR received : %d\n", (int)((data[1] >> 5) & 0x7));
          } else {
            // printf("> Message received, code = %d\n", (int)data[2]);

            // Sending RR
            int f = (data[1] >> 4) & 0x1;
            m_nr = (((data[1] >> 1) & 0x7) + 1) % 8;
            // printf("< RR sent %d %s\n", m_nr, (f ? "rep" : ""));
            unsigned char num =
                static_cast<unsigned char>((m_nr << 5) + 1 + 0x10 * f);

            unsigned char message[1]{num};
            server->createAndSendFr(data[0], message, sizeof(message));
          }
        }
      } else {
        printf("The CRC does not match");
      }
    }
  }
  // printf("Stopping loop...\n ");
}

void BasicHNZServer::sendSARMLoop() {
  while (!ua_ok || !sarm_ok) {
    sendSARM();
    ua_ok = false;
    sarm_ok = false;
    this_thread::sleep_for(chrono::milliseconds(3000));
  }
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

  // Loop that sending sarm every 3s
  thread *m_t2 = new thread(&BasicHNZServer::sendSARMLoop, this);
  this_thread::sleep_for(chrono::milliseconds(100));

  // Wait for UA and send UA in response of SARM
  int cpt = 1;
  while (1) {
    unsigned char *data = server->receiveData();
    unsigned char c = data[1];
    switch (c) {
      case UA_CODE:
        // printf("[HNZ Server] UA received\n");
        ua_ok = true;
        break;
      case SARM_CODE:
        // printf("[HNZ Server] SARM received, sending UA\n");
        unsigned char message[1];
        message[0] = 0x63;
        server->createAndSendFr(0x07, message, sizeof(message));
        sarm_ok = true;
        break;
      default:
        // printf("[HNZ Server] Neither UA nor SARM\n");
        break;
    }
    cpt++;
    if (ua_ok && sarm_ok) break;
  }
  m_t2->join();
  printf("[HNZ Server] Connection OK !!\n");

  // Connection established
  is_running = true;
  receiving_thread = new thread(&BasicHNZServer::receiving_loop, this);

  return true;
}

void BasicHNZServer::sendSARM() {
  unsigned char message[1];
  message[0] = 0x0F;
  server->createAndSendFr(0x05, message, sizeof(message));
}

void BasicHNZServer::sendFrame(vector<unsigned char> message, bool repeat) {
  int num = (((repeat) ? (m_ns - 1) : m_ns) % 8) << 1 + ((m_nr + 1 << 5) % 8);
  message.insert(message.begin(), num);
  int len = message.size();
  server->createAndSendFr(addr, message.data(), len);
  if (!repeat) m_ns++;
}