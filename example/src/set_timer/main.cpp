#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* sessionPtr) override {
    if (numEvent == 0) {
      std::cout << std::string("Timer is set at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl;
      sessionPtr->setTimer(
          "id", 1000,
          [](const boost::system::error_code&) { std::cout << std::string("Timer is canceled at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl; },
          []() { std::cout << std::string("Timer is triggered at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl; });
    }
    ++numEvent;
  }

  int numEvent{};
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;

int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("okx", "BTC-USDT", "MARKET_DEPTH");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
