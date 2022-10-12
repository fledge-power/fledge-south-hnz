#include <chrono>
#include <thread>

#include "hnz_server.h"

using namespace std;

class BasicHNZServer {
 public:
  BasicHNZServer(int port, int addr) {
    this->server = new HNZServer();
    this->addr = addr;
    this->m_port = port;
  };
  ~BasicHNZServer() {
    server->stop();
    delete server;
  };

  void startHNZServer();

  bool HNZServerIsReady();

  void sendSARM();

  void sendFrame(vector<unsigned char> message, bool repeat);

  HNZServer* server;
  int addr;

 private:
  thread* m_t1;
  int m_ns = 0;
  int m_port;

  static void m_start(HNZServer* server, int port) { server->start(port); };
};