#include <chrono>
#include <thread>
#include <mutex>

#include "hnz_server.h"

using namespace std;

class BasicHNZServer {
 public:
  BasicHNZServer(int port, int addr) {
    this->addr = addr;
    this->m_port = port;
  };
  ~BasicHNZServer() {
    // Stop thread
    stopHNZServer();
  };

  void startHNZServer();
  bool stopHNZServer();
  bool joinStartThread();

  bool waitForTCPConnection(int timeout_s);
  // Timeout = 16 = (5 * 3) + 1 sec = (SARM retries * SARM delay) + 1
  bool HNZServerIsReady(int timeout_s = 16);

  void sendSARM();

  void sendFrame(vector<unsigned char> message, bool repeat);
  void createAndSendFrame(unsigned char addr, unsigned char *msg, int msgSize);
  // Note: return the strcutre by value becase a copy must be done by the caller to remain thread safe
  std::vector<std::shared_ptr<MSG_TRAME>> popLastFramesReceived();
  std::vector<std::shared_ptr<MSG_TRAME>> popLastFramesSent();

  void onFrameReceived(MSG_TRAME* frame);
  void onFrameSent(MSG_TRAME* frame);

  void disableAcks(bool disabled) { ack_disabled = disabled; }

  static std::string toHexStr(unsigned char num);
  static std::string frameToStr(std::shared_ptr<MSG_TRAME> frame);
  static std::string framesToStr(std::vector<std::shared_ptr<MSG_TRAME>> frames);

  HNZServer* server = nullptr;
  int addr = 0;

 private:
  std::thread* m_t1 = nullptr;
  std::mutex m_t1_mutex;

  std::thread* receiving_thread = nullptr;
  std::mutex m_init_mutex;

  std::atomic<bool> is_running{false};
  std::atomic<bool> ack_disabled{false};

  int m_ns = 0;
  int m_nr = 0;
  int m_port = 0;
  
  bool ua_ok = false;
  bool sarm_ok = false;
  std::mutex m_sarm_ua_mutex;

  std::vector<std::shared_ptr<MSG_TRAME>> m_last_frames_received;
  std::mutex m_received_mutex;

  std::vector<std::shared_ptr<MSG_TRAME>> m_last_frames_sent;
  std::mutex m_sent_mutex;
  
  void receiving_loop();

  void sendSARMLoop();

  static void m_start(HNZServer* server, int port) { server->start(port); };
};