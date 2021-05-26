#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (numEvent == 0) {
      std::cout << std::string("Timer is set at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl;
      session->setTimer(
          "id", 1000,
          [](const boost::system::error_code&) {
            std::cout << std::string("Timer error handler is triggered at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl;
          },
          []() { std::cout << std::string("Timer success handler is triggered at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl; });
    }
    ++numEvent;
    return true;
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
  Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
