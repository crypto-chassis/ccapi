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
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::AUTHORIZATION_STATUS) {
      std::cout << "Received an event of type AUTHORIZATION_STATUS:\n" + event.toStringPretty(2, 2) << std::endl;
      auto message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::AUTHORIZATION_SUCCESS) {
        Request request(Request::Operation::FIX, "ftx", "", "cool correlation id");
        request.appendParamFix({
            {35, "D"},
            {21, "1"},
            {11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"},
            {55, "BTC/USD"},
            {40, "2"},
            {38, "0.001"},
            {44, "20000"},
            {54, "1"},
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
  // if (UtilSystem::getEnvAsString("COINBASE_API_KEY").empty()) {
  //   std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
  //   return EXIT_FAILURE;
  // }
  // if (UtilSystem::getEnvAsString("COINBASE_API_SECRET").empty()) {
  //   std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
  //   return EXIT_FAILURE;
  // }
  // if (UtilSystem::getEnvAsString("COINBASE_API_PASSPHRASE").empty()) {
  //   std::cerr << "Please set environment variable COINBASE_API_PASSPHRASE" << std::endl;
  //   return EXIT_FAILURE;
  // }
  SessionOptions sessionOptions;
  sessionOptions.heartbeatFixIntervalMilliSeconds = 30000;
  SessionConfigs sessionConfigs;
  CCAPI_LOGGER_INFO(ccapi::toString(sessionConfigs.getUrlFixBase()));
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  CCAPI_LOGGER_INFO("");
  Subscription subscription("ftx", "", "FIX", "", "cool correlation id");
  CCAPI_LOGGER_INFO("");
  session.subscribeByFix(subscription);
  CCAPI_LOGGER_INFO("");
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  return EXIT_SUCCESS;
}
