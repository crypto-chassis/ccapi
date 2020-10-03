#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = 0;  // This line is needed.
class MyLogger final: public Logger {
 public:
  void logMessage(Logger::Severity severity, std::thread::id threadId,
                          std::chrono::system_clock::time_point time,
                          std::string fileName, int lineNumber,
                          std::string message) override {
    std::cout << threadId << ": [" << UtilTime::getISOTimestamp(time) << "] {"
        << fileName << ":" << lineNumber << "} "
        << Logger::severityToString(severity) << std::string(8, ' ') << message
        << std::endl;
  }
};
class MyEventHandler : public EventHandler {
  bool processEvent(const Event& event, Session *session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto & message : event.getMessageList()) {
        if (message.getRecapType() == Message::RecapType::NONE) {
          std::cout << std::string("Top ") + CCAPI_EXCHANGE_VALUE_MARKET_DEPTH_MAX_DEFAULT + " bids and asks at " + UtilTime::getISOTimestamp(message.getTime()) + " are:" << std::endl;
          for (const auto & element : message.getElementList()) {
            const std::map<std::string, std::string>& elementNameValueMap = element.getNameValueMap();
            std::cout << "  " + toString(elementNameValueMap) << std::endl;
          }
        }
      }
    }
    return true;
  }
};
} /* namespace ccapi */
int main(int argc, char **argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
  MyLogger myLogger;
  Logger::logger = &myLogger;
  std::vector<std::string> modeList = {
      "multiple_exchanges_instruments",
      "specify_market_depth",
      "only_receive_events_at_periodic_intervals",
      "only_receive_events_at_periodic_intervals_including_when_the_market_depth_snapshot_has_not_changed_yet",
      "dispatching_events_from_multiple_threads"
  };
  if (argc != 2 || std::find(modeList.begin(), modeList.end(), argv[1]) == modeList.end()) {
    std::cout << "Please provide one command line argument from this list: "+toString(modeList) << std::endl;
    return 0;
  }
  std::string mode(argv[1]);

  SessionOptions sessionOptions;
  std::map<std::string, std::map<std::string, std::string> > exchangeInstrumentSymbolMap;
  exchangeInstrumentSymbolMap[CCAPI_EXCHANGE_NAME_COINBASE]["my cool naming for btc-usd"] = "BTC-USD";
  if (mode == "multiple_exchanges_instruments") {
    exchangeInstrumentSymbolMap[CCAPI_EXCHANGE_NAME_COINBASE]["my cool naming for eth-usd"] = "ETH-USD";
  }
  SessionConfigs sessionConfigs(exchangeInstrumentSymbolMap);
  MyEventHandler eventHandler;
  int eventDispatcherNumThreads = mode == "dispatching_events_from_multiple_threads" ? 2 : 1;
  EventDispatcher eventDispatcher(eventDispatcherNumThreads);
  Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
  SubscriptionList subscriptionList;
  std::string options;
  if (mode == "specify_market_depth") {
    options = std::string(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX) + "=2";
  } else if (mode == "only_receive_events_at_periodic_intervals") {
    options = std::string(CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS) + "=1000";
  } else if (mode == "only_receive_events_at_periodic_intervals_including_when_the_market_depth_snapshot_has_not_changed_yet") {
    options = std::string(CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS) + "=1000&" + CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS + "=0";
  }
  Subscription subscription(std::string("/") + CCAPI_EXCHANGE_NAME_COINBASE + "/my cool naming for btc-usd",
                            CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
                            options,
                            CorrelationId("this is my correlation id for btc-usd"));
  subscriptionList.add(subscription);
  if (mode == "multiple_exchanges_instruments") {
    Subscription subscription(std::string("/") + CCAPI_EXCHANGE_NAME_COINBASE + "/my cool naming for eth-usd",
                              CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
                              options,
                              CorrelationId("this is my correlation id for eth-usd"));
    subscriptionList.add(subscription);
  }
  session.subscribe(subscriptionList);
  return EXIT_SUCCESS;
}
