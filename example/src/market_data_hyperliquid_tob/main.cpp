#ifndef CCAPI_ENABLE_EXCHANGE_HYPERLIQUID
#define CCAPI_ENABLE_EXCHANGE_HYPERLIQUID
#endif
// Define this to enable MarketDataService
#ifndef CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#endif

#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {
Logger* Logger::logger = nullptr;

class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
      std::cout << "Received an event of type SUBSCRIPTION_STATUS:\n" + event.toPrettyString(2, 2) << std::endl;
    } else if (event.getType() == Event::Type::RESPONSE) {
      std::cout << "Received an event of type RESPONSE:\n" + event.toPrettyString(2, 2) << std::endl;
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::GET_INSTRUMENTS) {
          std::cout << "Found " << message.getElementList().size() << " instruments." << std::endl;
          // Subscribe to Hyperliquid 'l2Book' channel for 'BTC'
          // We use CCAPI_MARKET_DEPTH_MAX="1" to request only the top level (TOB)
          Subscription subscription(CCAPI_EXCHANGE_NAME_HYPERLIQUID, "BTC", CCAPI_MARKET_DEPTH, CCAPI_MARKET_DEPTH_MAX "=" "1");
          // Subscription subscription(CCAPI_EXCHANGE_NAME_HYPERLIQUID, "BTC", CCAPI_TRADE);
          session->subscribe(subscription);
        }
      }
    } else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH) {
          std::cout << "TOB at " << UtilTime::getISOTimestamp(message.getTime()) << ":" << std::endl;
          for (const auto& element : message.getElementList()) {
            const std::map<std::string_view, std::string>& elementNameValueMap = element.getNameValueMap();
            
            // Print Bid
            if (elementNameValueMap.find(CCAPI_BEST_BID_N_PRICE) != elementNameValueMap.end()) {
              std::cout << "  Bid: " << elementNameValueMap.at(CCAPI_BEST_BID_N_PRICE) 
                        << " Size: " << elementNameValueMap.at(CCAPI_BEST_BID_N_SIZE) << std::endl;
            }
            
            // Print Ask
            if (elementNameValueMap.find(CCAPI_BEST_ASK_N_PRICE) != elementNameValueMap.end()) {
              std::cout << "  Ask: " << elementNameValueMap.at(CCAPI_BEST_ASK_N_PRICE) 
                        << " Size: " << elementNameValueMap.at(CCAPI_BEST_ASK_N_SIZE) << std::endl;
            }
          }
          std::cout << "---------------------------------" << std::endl;
        } else if (message.getType() == Message::Type::MARKET_DATA_EVENTS_TRADE) {
          std::cout << "Trade at " << UtilTime::getISOTimestamp(message.getTime()) << ":" << std::endl;
          for (const auto& element : message.getElementList()) {
            const std::map<std::string_view, std::string>& elementNameValueMap = element.getNameValueMap();
            for (const auto& kv : elementNameValueMap) {
              std::cout << "  KEY: " << kv.first << " VAL: " << kv.second << std::endl;
            }
          }
          std::cout << "---------------------------------" << std::endl;
        }
      }
    }
  }
};
}

int main(int argc, char** argv) {
  using namespace ccapi;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);

  Request request(Request::Operation::GET_INSTRUMENTS, CCAPI_EXCHANGE_NAME_HYPERLIQUID);
  session.sendRequest(request);

  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
