#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto & message : event.getMessageList()) {
        auto correlationId = message.getCorrelationIdList().at(0);
        if (correlationId == "c") {
          for (const auto & element : message.getElementList()) {
            if (element.has("BID_PRICE")) {
              bestBidPriceCoinbase = element.getValue("BID_PRICE");
            }
            if (element.has("ASK_PRICE")) {
              bestAskPriceCoinbase = element.getValue("ASK_PRICE");
            }
            if (element.has("BID_SIZE")) {
              bestBidSizeCoinbase = element.getValue("BID_SIZE");
            }
            if (element.has("ASK_SIZE")) {
              bestAskSizeCoinbase = element.getValue("ASK_SIZE");
            }
          }
        }
        if (correlationId == "g") {
          for (const auto & element : message.getElementList()) {
            if (element.has("BID_PRICE")) {
              bestBidPriceGemini = element.getValue("BID_PRICE");
            }
            if (element.has("ASK_PRICE")) {
              bestAskPriceGemini = element.getValue("ASK_PRICE");
            }
            if (element.has("BID_SIZE")) {
              bestBidSizeGemini = element.getValue("BID_SIZE");
            }
            if (element.has("ASK_SIZE")) {
              bestAskSizeGemini = element.getValue("ASK_SIZE");
            }
          }
        }
        if (!bestBidPriceCoinbase.empty() && !bestAskPriceGemini.empty() && Decimal(bestBidPriceCoinbase) >= Decimal(bestAskPriceGemini)) {
          std::vector<Request> requestList;
          Request requestBuy(Request::Operation::CREATE_ORDER, "gemini", "btcusd");
          std::string orderQuantity = std::min(Decimal(bestBidSizeCoinbase), Decimal(bestAskSizeGemini)).toString();
          requestBuy.appendParam({
            {"SIDE", "BUY"},
            {"QUANTITY", orderQuantity},
            {"LIMIT_PRICE", bestAskPriceGemini}
          });
          requestList.push_back(requestBuy);
          Request requestSell(Request::Operation::CREATE_ORDER, "coinbase", "BTC-USD");
          requestSell.appendParam({
            {"SIDE", "SELL"},
            {"QUANTITY", orderQuantity},
            {"LIMIT_PRICE", bestBidPriceCoinbase}
          });
          requestList.push_back(requestSell);
          session->sendRequest(requestList);
        }
        if (!bestBidPriceGemini.empty() && !bestAskPriceCoinbase.empty() && Decimal(bestBidPriceGemini) >= Decimal(bestAskPriceCoinbase)) {
          std::vector<Request> requestList;
          Request requestBuy(Request::Operation::CREATE_ORDER, "coinbase", "BTC-USD");
          std::string orderQuantity = std::min(Decimal(bestBidSizeGemini), Decimal(bestAskSizeCoinbase)).toString();
          requestBuy.appendParam({
            {"SIDE", "BUY"},
            {"QUANTITY", orderQuantity},
            {"LIMIT_PRICE", bestAskPriceCoinbase}
          });
          requestList.push_back(requestBuy);
          Request requestSell(Request::Operation::CREATE_ORDER, "gemini", "btcusd");
          requestSell.appendParam({
            {"SIDE", "SELL"},
            {"QUANTITY", orderQuantity},
            {"LIMIT_PRICE", bestBidPriceGemini}
          });
          requestList.push_back(requestSell);
          session->sendRequest(requestList);
        }
      }
    }
    return true;
  }

 private:
  std::string bestBidPriceCoinbase;
  std::string bestAskPriceCoinbase;
  std::string bestBidSizeCoinbase;
  std::string bestAskSizeCoinbase;
  std::string bestBidPriceGemini;
  std::string bestAskPriceGemini;
  std::string bestBidSizeGemini;
  std::string bestAskSizeGemini;
};
} /* namespace ccapi */

int main(int argc, char **argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  std::vector<Subscription> subscriptionList;
  subscriptionList.emplace_back("coinbase", "BTC-USD", "MARKET_DEPTH", "", "c");
  subscriptionList.emplace_back("gemini", "btcusd", "MARKET_DEPTH", "", "g");
  session.subscribe(subscriptionList);
  while (true) {
    std::cout << "I'm waiting for arbitrage opportunities" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return EXIT_SUCCESS;
}
