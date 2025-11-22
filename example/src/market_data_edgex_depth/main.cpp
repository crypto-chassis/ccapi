#ifndef CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#endif

#ifndef CCAPI_ENABLE_EXCHANGE_EDGEX
#define CCAPI_ENABLE_EXCHANGE_EDGEX
#endif

#include "ccapi_cpp/ccapi_session.h"
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

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

class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::RESPONSE) {
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::GET_INSTRUMENTS) {
          const auto& elements = message.getElementList();
          std::cout << "EdgeX contracts discovered: " << elements.size() << std::endl;
          if (!elements.empty()) {
            const auto& map = elements.front().getNameValueMap();
            std::cout << "Sample instrument fields:" << std::endl;
            for (const auto& kv : map) {
              std::cout << "  " << kv.first << " = " << kv.second << std::endl;
            }
          }
        }
      }
    } else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH) {
          std::cout << "[DEPTH] " << UtilTime::getISOTimestamp(message.getTime()) << std::endl;
          int level = 0;
          for (const auto& element : message.getElementList()) {
            ++level;
            const auto& map = element.getNameValueMap();
            if (map.find(CCAPI_BEST_BID_N_PRICE) != map.end()) {
              std::cout << "  L" << level << " Bid  " << map.at(CCAPI_BEST_BID_N_PRICE) << " x " << map.at(CCAPI_BEST_BID_N_SIZE) << std::endl;
            }
            if (map.find(CCAPI_BEST_ASK_N_PRICE) != map.end()) {
              std::cout << "  L" << level << " Ask  " << map.at(CCAPI_BEST_ASK_N_PRICE) << " x " << map.at(CCAPI_BEST_ASK_N_SIZE) << std::endl;
            }
          }
          std::cout << "---------------------------------" << std::endl;
        } else if (message.getType() == Message::Type::MARKET_DATA_EVENTS_TRADE) {
          std::cout << "[TRADE] " << UtilTime::getISOTimestamp(message.getTime()) << std::endl;
          for (const auto& element : message.getElementList()) {
            for (const auto& kv : element.getNameValueMap()) {
              std::cout << "  " << kv.first << " = " << kv.second << std::endl;
            }
          }
        }
      }
    }
  }
};

}  // namespace ccapi

using namespace ccapi;

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);

  Request instruments(Request::Operation::GET_INSTRUMENTS, CCAPI_EXCHANGE_NAME_EDGEX);
  session.sendRequest(instruments);

  const std::string contractId = "10000001";  // BTCUSDT in sample metadata
  Subscription depthSubscription(CCAPI_EXCHANGE_NAME_EDGEX, contractId, CCAPI_MARKET_DEPTH, CCAPI_MARKET_DEPTH_MAX "=" "3");
  Subscription tradesSubscription(CCAPI_EXCHANGE_NAME_EDGEX, contractId, CCAPI_TRADE);
  session.subscribe(depthSubscription);
  session.subscribe(tradesSubscription);

  std::this_thread::sleep_for(std::chrono::seconds(6));
  session.stop();
  std::cout << "EdgeX market data example finished." << std::endl;
  return EXIT_SUCCESS;
}

