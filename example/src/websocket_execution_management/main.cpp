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
    if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto& message : event.getMessageList()) {
        std::vector<Message> messageList = event.getMessageList();
        // Expecting a single element inside of the messageList
        //            std::string messageType =
        //            messageList.at(0).typeToString(messageList.at(0).getType());
        Message::Type messageType = messageList.at(0).getType();
        switch (messageType) {
          case Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE: {
            // Process the Futures positions and balances
            CCAPI_LOGGER_WARN("Got a response for EXECUTION_FILL");
          } break;
          case Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE: {
            // Process the USD and SPOT balances
            CCAPI_LOGGER_WARN("Got a response for EXECUTION_ORDER");
          } break;
          // case Message::Type::EXECUTION_MANAGEMENT_EVENTS: {
          //   // Process the USD and SPOT balances
          //   CCAPI_LOGGER_WARN("Got a response for EXECUTION_MANAGEMENT_EVENTS");
          // } break;
          default:
            continue;
        }
      }

    } else {
      auto eType = event.getType();
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

int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  // Event queue for account information
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  std::vector<Subscription> subscriptionList;
  //  Subscription subscription("coinbase", "BTC-USD", "ORDER");
  Subscription subscriptionTrade("ftx", "", CCAPI_EM_PRIVATE_TRADE);
  Subscription subscriptionOrder("ftx", "", CCAPI_EM_ORDER_UPDATE);
  subscriptionList.push_back(subscriptionTrade);
  subscriptionList.push_back(subscriptionOrder);
  session.subscribe(subscriptionList);

  std::this_thread::sleep_for(std::chrono::seconds(10000));
  session.stop();
  return EXIT_SUCCESS;
}
