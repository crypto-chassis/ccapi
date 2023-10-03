#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::AUTHORIZATION_STATUS) {
      std::cout << "Received an event of type AUTHORIZATION_STATUS:\n" + event.toStringPretty(2, 2) << std::endl;
      auto message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::AUTHORIZATION_SUCCESS) {
        Request request(Request::Operation::FIX, "coinbase", "", "same correlation id for subscription and request");
        request.appendParamFix({
            {35, "D"},
            {11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"},
            {55, "BTC-USD"},
            {54, "1"},
            {44, "20000"},
            {38, "0.001"},
            {40, "2"},
            {59, "1"},
        });
        session->sendRequestByFix(request);
      }
    } else if (event.getType() == Event::Type::FIX) {
      std::cout << "Received an event of type FIX:\n" + event.toStringPretty(2, 2) << std::endl;
    }
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;
int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("COINBASE_API_KEY").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_SECRET").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  sessionOptions.heartbeatFixIntervalMilliseconds = 30000;  // Note the unit is millisecond
  sessionOptions.heartbeatFixTimeoutMilliseconds = 5000;    // Note the unit is millisecond, should be less than heartbeatFixIntervalMilliseconds
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("coinbase", "", "FIX", "8013=S&9406=Y",
                            "same correlation id for subscription and request");  // https://docs.pro.coinbase.com/#logon-a
  session.subscribeByFix(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  return EXIT_SUCCESS;
}
