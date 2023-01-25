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
  Logger* Logger::logger = &myLogger;class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
      std::cout << "Received an event of type SUBSCRIPTION_STATUS:\n" + event.toStringPretty(2, 2) << std::endl;
      auto message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::SUBSCRIPTION_STARTED) {
        Request request(Request::Operation::CREATE_ORDER, "binance-usds-futures", "BTCUSDT");
        request.appendParam({
            {"SIDE", "BUY"},
            {"LIMIT_PRICE", "23000"},
            {"QUANTITY", "0.001"},
            {"timeInForce", "GTX"},
            // {"CLIENT_ORDER_ID", "6d4eb0fb-2229-469f-873e-557dd78ac11e"},
        });
        session->sendRequest(request);
      }
    } else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      std::cout << "Received an event of type SUBSCRIPTION_DATA:\n" + event.toStringPretty(2, 2) << std::endl;
    }
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
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
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("binance-usds-futures", "BTCUSDT", "ORDER_UPDATE,PRIVATE_TRADE");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
