#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
class MyLogger final : public Logger {
 public:
  void logMessage(std::string severity, std::string threadId, std::string timeISO, std::string fileName, std::string lineNumber, std::string message) override {
    std::lock_guard<std::mutex> lock(m);
    std::cout << threadId << ": [" << timeISO << "] {" << fileName << ":" << lineNumber << "} " << severity << std::string(8, ' ') << message << std::endl;
  }

 private:
  std::mutex m;
};
MyLogger myLogger;
Logger* Logger::logger = &myLogger;
} /* namespace ccapi */
using ::ccapi::Session;
using ::ccapi::Subscription;
int main(int argc, char** argv) {
  Session session;
  Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  return EXIT_SUCCESS;
}
