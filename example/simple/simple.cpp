#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
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
Logger* Logger::logger = 0;  // This line is needed.
} /* namespace ccapi */
int main(int argc, char **argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
  SessionOptions sessionOptions;
  std::string pair = "I want to name BTC against USD this";
  std::string symbol = "BTC-USD";  // This is how Coinbase names BTC/USD
  SessionConfigs sessionConfigs({{
    CCAPI_EXCHANGE_NAME_COINBASE, {{
        pair, symbol
    }}
  }});
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  SubscriptionList subscriptionList;
  std::string topic = std::string("/") + CCAPI_EXCHANGE_NAME_COINBASE + "/" + pair;
  std::string fields = CCAPI_EXCHANGE_NAME_MARKET_DEPTH;
  std::string options = "";
  CorrelationId correlationId("This is my correlation id");
  Subscription subscription(topic, fields, options, correlationId);
  subscriptionList.add(subscription);
  session.subscribe(subscriptionList);
  return 0;
}
