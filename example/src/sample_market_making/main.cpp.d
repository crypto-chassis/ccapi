#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
class ExampleLogger final: public Logger {
 public:
  virtual void logMessage(Logger::Severity severity, std::thread::id threadId,
                          std::chrono::system_clock::time_point time,
                          std::string fileName, int lineNumber,
                          std::string message) override {
    std::lock_guard<std::mutex> lock(m);
    std::cout << threadId << ": [" << UtilTime::getISOTimestamp(time) << "] {"
        << fileName << ":" << lineNumber << "} "
        << Logger::severityToString(severity) << std::string(8, ' ') << message
        << std::endl;
//    lock.unlock();
  }
 private:
  std::mutex m;
};
  Logger* Logger::logger = 0;  // This line is needed.
  class MyEventHandler : public EventHandler {
   public:
    bool processEvent(const Event& event, Session *session) override {
//      std::cout << "Event = "+event.toString() <<std::endl;
      if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
        for (const auto & message : event.getMessageList()) {
          if (message.getRecapType() == Message::RecapType::NONE) {
  //          std::cout << std::string("Best bid and ask at " + UtilTime::getISOTimestamp(message.getTime()) + " are:" << std::endl;
            for (const auto & element : message.getElementList()) {
              const std::lock_guard<std::mutex> lock(m);
              if (element.has("BID_PRICE")) {
                bestBidPrice = element.getValue("BID_PRICE");
              }
              if (element.has("ASK_PRICE")) {
                bestAskPrice = element.getValue("ASK_PRICE");
              }
            }
          }
        }
      }
      return true;
    }
    std::pair<std::string, std::string> getBBO() {
      const std::lock_guard<std::mutex> lock(m);
      return std::make_pair(bestBidPrice, bestAskPrice);
    }

   private:
    mutable std::mutex m;
    std::string bestBidPrice;
    std::string bestAskPrice;
  };
} /* namespace ccapi */
std::string regularizePrice(double price) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(2) << price;
  return stream.str();
}
int main(int argc, char **argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
//  ExampleLogger exampleLogger;
//  Logger::logger = &exampleLogger;
  if (argc != 3) {
      std::cerr <<
          "Usage: <program name> <spread percentage> <order quantity>\n" <<
          "Example:\n" <<
          "    main 0.5 0.01" << std::endl;
      return EXIT_FAILURE;
  }
  double spreadPercentage = std::stod(argv[1]);
  std::string orderQuantity = argv[2];
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  // https://github.com/binance-us/binance-official-api-docs/blob/master/web-socket-streams.md: All symbols for streams are lowercase
  Subscription subscription("binance-us", "btcusd", "MARKET_DEPTH");
  session.subscribe(subscription);
  std::string key = UtilSystem::getEnvAsString("BINANCE_US_API_KEY");
  if (key.empty()) {
    std::cerr << "Please provide environment variable BINANCE_US_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  std::string secret = UtilSystem::getEnvAsString("BINANCE_US_API_SECRET");
  if (secret.empty()) {
    std::cerr << "Please provide environment variable BINANCE_US_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  std::map<std::string, std::string> credential = {
     { CCAPI_BINANCE_US_API_KEY, key },
     { CCAPI_BINANCE_US_API_SECRET, secret }
  };
//  Queue<Event> eventQueue;
  while (true) {
    // https://github.com/binance-us/binance-official-api-docs/blob/master/rest-api.md#signed-endpoint-examples-for-post-apiv3order: All symbols for REST are uppercase
    Request requestCancel(Request::Operation::CANCEL_OPEN_ORDERS, credential, "binance-us", "BTCUSD");
    session.sendRequest(requestCancel);
    std::cout << "Cancel all open orders" << std::endl;
//    std::vector<Event> eventList = eventQueue.purge();
//    CCAPI_LOGGER_TRACE("eventList = "+toString(eventList));
    auto bbo = eventHandler.getBBO();
//    CCAPI_LOGGER_TRACE("bbo.first = "+bbo.first);
//    CCAPI_LOGGER_TRACE("bbo.second = "+bbo.second);

    if (!bbo.first.empty() && !bbo.second.empty()) {
      double midPrice = (std::stod(bbo.first) + std::stod(bbo.first)) / 2;
      std::cout << "Current mid price is " + std::to_string(midPrice) << std::endl;
      std::string buyPrice = regularizePrice(midPrice * (1 - spreadPercentage / 100));
      std::string sellPrice = regularizePrice(midPrice * (1 + spreadPercentage / 100));
      std::vector<Request> requestList;
      Request requestBuy(Request::Operation::CREATE_ORDER, credential, "binance-us", "BTCUSD");
      requestBuy.setParam("SIDE", "BUY");
      requestBuy.setParam("QUANTITY", orderQuantity);
      requestBuy.setParam("LIMIT_PRICE", buyPrice);
      requestList.push_back(requestBuy);
      Request requestSell(Request::Operation::CREATE_ORDER, credential, "binance-us", "BTCUSD");
      requestSell.setParam("SIDE", "SELL");
      requestSell.setParam("QUANTITY", orderQuantity);
      requestSell.setParam("LIMIT_PRICE", sellPrice);
      requestList.push_back(requestSell);
      session.sendRequest(requestList);
//      std::vector<Event> eventList = eventQueue.purge();
//      CCAPI_LOGGER_TRACE("eventList = "+toString(eventList));
      std::cout << "Buy " + orderQuantity + " BTCUSD at price " + buyPrice << std::endl;
      std::cout << "Sell " + orderQuantity + " BTCUSD at price " + sellPrice << std::endl;
    } else {
      std::cout << "Insufficient market information" << std::endl;
    }
    int timeToSleepSeconds = 10;
    std::cout << "About to sleep for " + std::to_string(timeToSleepSeconds) + " seconds\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(timeToSleepSeconds));
  }
  return EXIT_SUCCESS;
}
