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
    this->is_running = false;
  };
  ~BasicHNZServer() {
    // Stop thread
    is_running = false;
    server->stop();
    if (receiving_thread != nullptr) {
      receiving_thread->join();
      delete receiving_thread;
    }

    delete server;
  };

  void startHNZServer();

  bool HNZServerIsReady();

  void sendSARM();

  void sendFrame(vector<unsigned char> message, bool repeat);

  HNZServer* server;
  int addr;

 private:
  thread* m_t1 = nullptr;
  thread* receiving_thread = nullptr;
  atomic<bool> is_running;
  int m_ns = 0;
  int m_nr = 0;
  int m_port;
  bool ua_ok = false;
  bool sarm_ok = false;

  void receiving_loop();

  void sendSARMLoop();

  static void m_start(HNZServer* server, int port) { server->start(port); };
};