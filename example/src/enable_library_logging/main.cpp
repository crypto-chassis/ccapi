#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
class MyLogger final : public Logger {
 public:
  void logMessage(const std::string& severity, const std::string& threadId, const std::string& timeISO, const std::string& fileName,
                  const std::string& lineNumber, const std::string& message) override {
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
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
