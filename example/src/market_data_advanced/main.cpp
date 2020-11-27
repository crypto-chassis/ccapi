#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = 0;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    std::cout << toString(event) + "\n" << std::endl;
    return true;
  }
};
} /* namespace ccapi */
int main(int argc, char **argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
  std::vector<std::string> modeList = {
      "specify_correlation_id",
      "normalize_instrument_name",
      "multiple_exchanges_instruments",
      "specify_market_depth",
      "receive_events_at_periodic_intervals",
      "receive_events_at_periodic_intervals_including_when_the_market_depth_snapshot_has_not_changed",
      "dispatch_events_to_multiple_threads",
      "handle_events_synchronously"
  };
  if (argc != 2 || std::find(modeList.begin(), modeList.end(), argv[1]) == modeList.end()) {
    std::cout << "Please provide one command line argument from this list: "+toString(modeList) << std::endl;
    return 0;
  }
  std::string mode(argv[1]);
  if (mode == "specify_correlation_id") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "", "cool correlation id");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
  } else if (mode == "normalize_instrument_name") {
    SessionOptions sessionOptions;
    std::map<std::string, std::map<std::string, std::string> > exchangeInstrumentSymbolMap;
    std::string coolName = "btc_usd";
    exchangeInstrumentSymbolMap["coinbase"][coolName] = "BTC-USD";
    SessionConfigs sessionConfigs(exchangeInstrumentSymbolMap);
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    Subscription subscription("coinbase", coolName, "MARKET_DEPTH");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
  } else if (mode == "multiple_exchanges_instruments") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    std::vector<Subscription> subscriptionList;
    Subscription subscription_1("coinbase", "BTC-USD", "MARKET_DEPTH", "", "coinbase|btc_usd");
    subscriptionList.push_back(subscription_1);
    Subscription subscription_2("binance-us", "ethusd", "MARKET_DEPTH", "", "binance-us|eth_usd");
    subscriptionList.push_back(subscription_2);
    session.subscribe(subscriptionList);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
  } else if (mode == "specify_market_depth") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "MARKET_DEPTH_MAX=2");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
  } else if (mode == "receive_events_at_periodic_intervals") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
  } else if (mode == "receive_events_at_periodic_intervals_including_when_the_market_depth_snapshot_has_not_changed") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    Session session(sessionOptions, sessionConfigs, &eventHandler);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
  } else if (mode == "dispatch_events_to_multiple_threads") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    EventDispatcher eventDispatcher(2);
    Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    session.stop();
    eventDispatcher.stop();
  } else if (mode == "handle_events_synchronously") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    Session session(sessionOptions, sessionConfigs);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::vector<Event> eventList = session.eventQueue.purge();
    for (const auto & event : eventList) {
      std::cout << toString(event) + "\n" << std::endl;
    }
    session.stop();
  }
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
