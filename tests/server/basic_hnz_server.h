#include <chrono>
#include <thread>
#include <mutex>

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
  void createAndSendFrame(unsigned char addr, unsigned char *msg, int msgSize);
  // Note: return the strcutre by value becase a copy must be done by the caller to remain thread safe
  std::vector<std::shared_ptr<MSG_TRAME>> popLastFramesReceived();
  std::vector<std::shared_ptr<MSG_TRAME>> popLastFramesSent();

  void onFrameReceived(MSG_TRAME* frame);
  void onFrameSent(MSG_TRAME* frame);

  static std::string toHexStr(unsigned char num);
  static std::string frameToStr(std::shared_ptr<MSG_TRAME> frame);
  static std::string framesToStr(std::vector<std::shared_ptr<MSG_TRAME>> frames);

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

  std::vector<std::shared_ptr<MSG_TRAME>> m_last_frames_received;
  std::mutex m_received_mutex;

  std::vector<std::shared_ptr<MSG_TRAME>> m_last_frames_sent;
  std::mutex m_sent_mutex;
  
  void receiving_loop();

  void sendSARMLoop();

  static void m_start(HNZServer* server, int port) { server->start(port); };
};