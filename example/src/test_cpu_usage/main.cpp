#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  MyEventHandler(size_t subscriptionDataEventPrintCount) : subscriptionDataEventPrintCount(subscriptionDataEventPrintCount) {}

  void processEvent(const Event& event, Session* sessionPtr) override {
    const auto& eventType = event.getType();
    if (eventType == Event::Type::SESSION_STATUS || eventType == Event::Type::SUBSCRIPTION_STATUS) {
      std::cout << "Received an event:\n" + event.toPrettyString(2, 2) << std::endl;
    } else if (eventType == Event::Type::SUBSCRIPTION_DATA) {
      ++subscriptionDataEventCount;
      if (subscriptionDataEventCount % this->subscriptionDataEventPrintCount == 0) {
        std::cout << "Received " << subscriptionDataEventCount << " SUBSCRIPTION_DATA events." << std::endl;
      }
    }
  }

 private:
  size_t subscriptionDataEventPrintCount{};
  size_t subscriptionDataEventCount{};
};

} /* namespace ccapi */

using ::ccapi::Event;
using ::ccapi::MyEventHandler;
using ::ccapi::Queue;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilString;
using ::ccapi::UtilSystem;

struct Ticker {
  std::string instrument;
  double quoteVolume24h{};
};

int main(int argc, char** argv) {
  const auto& numSymbols = UtilSystem::getEnvAsInt("NUM_SYMBOLS", 50);
  std::vector<std::string> instruments = UtilString::split(UtilSystem::getEnvAsString("SYMBOLS"), ",");
  instruments.erase(std::remove_if(instruments.begin(), instruments.end(), [](const std::string& s) { return s.empty(); }), instruments.end());
  bool subscribeMarketDepth = UtilSystem::getEnvAsBool("SUBSCRIBE_MARKET_DEPTH");
  bool subscribeMarketDepth50 = UtilSystem::getEnvAsBool("SUBSCRIBE_MARKET_DEPTH_50");
  bool subscribeMarketDepth400 = UtilSystem::getEnvAsBool("SUBSCRIBE_MARKET_DEPTH_400");
  bool subscribeTrade = UtilSystem::getEnvAsBool("SUBSCRIBE_TRADE");
  size_t subscriptionDataEventPrintCount = UtilSystem::getEnvAsInt("SUBSCRIPTION_DATA_EVENT_PRINT_COUNT", 10000);
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler(subscriptionDataEventPrintCount);
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  std::string exchange = "okx";

  if (instruments.empty()) {
    Request request(Request::Operation::GET_TICKERS, exchange);
    request.appendParam({
        {"INSTRUMENT_TYPE", "SPOT"},
    });
    Queue<Event> eventQueue;
    session.sendRequest(request, &eventQueue);
    std::vector<Event> eventList = eventQueue.purge();
    const auto& message = eventList.front().getMessageList().front();
    std::vector<Ticker> tickers;
    for (const auto& element : message.getElementList()) {
      const auto& instrument = element.getValue("INSTRUMENT");
      const auto& volume24h = std::stod(element.getValue("VOLUME_24H"));
      if (volume24h > 0) {
        const auto& averagePrice = (std::stod(element.getValue("OPEN_24H_PRICE")) + std::stod(element.getValue("HIGH_24H_PRICE")) +
                                    std::stod(element.getValue("LOW_24H_PRICE")) + std::stod(element.getValue("LAST_PRICE"))) /
                                   4;
        const auto& quoteVolume24h = averagePrice * volume24h;
        tickers.push_back({instrument, quoteVolume24h});
      }
    }
    std::sort(tickers.begin(), tickers.end(), [](const Ticker& a, const Ticker& b) {
      return a.quoteVolume24h > b.quoteVolume24h;  // descending
    });
    std::vector<Ticker> topNTickers;
    topNTickers.assign(tickers.begin(), tickers.begin() + std::min(numSymbols, static_cast<int>(tickers.size())));
    for (const auto& ticker : topNTickers) {
      instruments.push_back(ticker.instrument);
    }
  }
  std::cout << "instruments = " + UtilString::join(instruments, ",") << std::endl;
  std::vector<Subscription> subscriptionList;
  for (const auto& instrument : instruments) {
    if (subscribeMarketDepth) {
      Subscription subscription(exchange, instrument, "MARKET_DEPTH");
      subscriptionList.push_back(subscription);
    }
    if (subscribeMarketDepth50) {
      Subscription subscription(exchange, instrument, "MARKET_DEPTH", "MARKET_DEPTH_MAX=50");
      subscriptionList.push_back(subscription);
    }
    if (subscribeMarketDepth400) {
      Subscription subscription(exchange, instrument, "MARKET_DEPTH", "MARKET_DEPTH_MAX=400");
      subscriptionList.push_back(subscription);
    }
    if (subscribeTrade) {
      Subscription subscription(exchange, instrument, "TRADE");
      subscriptionList.push_back(subscription);
    }
  }
  session.subscribe(subscriptionList);
  std::this_thread::sleep_for(std::chrono::seconds(UtilSystem::getEnvAsInt("STOP_TIME_IN_SECONDS", INT_MAX)));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
