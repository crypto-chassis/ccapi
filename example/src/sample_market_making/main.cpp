#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto & message : event.getMessageList()) {
        if (message.getRecapType() == Message::RecapType::NONE) {
          for (const auto & element : message.getElementList()) {
            std::lock_guard<std::mutex> lock(m);
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
    std::lock_guard<std::mutex> lock(m);
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
  if (argc != 3) {
    std::cerr << "Usage: <program name> <spread percentage> <order quantity>\n" << "Example:\n" << "    main 0.5 0.01"
              << std::endl;
    return EXIT_FAILURE;
  }
  double spreadPercentage = std::stod(argv[1]);
  std::string orderQuantity = argv[2];
  if (UtilSystem::getEnvAsString("COINBASE_API_KEY").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_SECRET").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("CCAPI_COINBASE_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable CCAPI_COINBASE_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
  session.subscribe(subscription);
  while (true) {
    Request requestCancel(Request::Operation::CANCEL_OPEN_ORDERS, "coinbase", "BTC-USD");
    session.sendRequest(requestCancel);
    std::cout << "Cancel all open orders" << std::endl;
    auto bbo = eventHandler.getBBO();
    if (!bbo.first.empty() && !bbo.second.empty()) {
      double midPrice = (std::stod(bbo.first) + std::stod(bbo.first)) / 2;
      std::cout << "Current mid price is " + std::to_string(midPrice) << std::endl;
      std::string buyPrice = regularizePrice(midPrice * (1 - spreadPercentage / 100));
      std::string sellPrice = regularizePrice(midPrice * (1 + spreadPercentage / 100));
      std::vector<Request> requestList;
      Request requestBuy(Request::Operation::CREATE_ORDER, "coinbase", "BTC-USD");
      requestBuy.appendParam({
        {"SIDE", "BUY"},
        {"QUANTITY", orderQuantity},
        {"LIMIT_PRICE", buyPrice}
      });
      requestList.push_back(requestBuy);
      Request requestSell(Request::Operation::CREATE_ORDER, "coinbase", "BTC-USD");
      requestSell.appendParam({
        {"SIDE", "SELL"},
        {"QUANTITY", orderQuantity},
        {"LIMIT_PRICE", sellPrice}
      });
      requestList.push_back(requestSell);
      session.sendRequest(requestList);
      std::cout << "Buy " + orderQuantity + " BTC-USD at price " + buyPrice << std::endl;
      std::cout << "Sell " + orderQuantity + " BTC-USD at price " + sellPrice << std::endl;
    } else {
      std::cout << "Insufficient market information" << std::endl;
    }
    int timeToSleepSeconds = 10;
    std::cout << "About to sleep for " + std::to_string(timeToSleepSeconds) + " seconds\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(timeToSleepSeconds));
  }
  return EXIT_SUCCESS;
}
