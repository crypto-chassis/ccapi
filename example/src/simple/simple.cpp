#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = 0;  // This line is needed.
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
  SessionOptions sessionOptions;
  std::string instrument = "my cool naming";
  std::string symbol = "BTC-USD";
  // Coinbase names a trading pair using upper case concatenated by dash
  // Since symbol normalization is a tedious task, you can choose to use a reference file at https://marketdata-e0323a9039add2978bf5b49550572c7c-public.s3.amazonaws.com/supported_exchange_instrument_subscription_data.csv.gz which we frequently update.
  SessionConfigs sessionConfigs({{
    CCAPI_EXCHANGE_NAME_COINBASE, {{
        instrument, symbol
    }}
  }});
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  SubscriptionList subscriptionList;
  std::string topic = std::string("/") + CCAPI_EXCHANGE_NAME_COINBASE + "/" + instrument;
  std::string fields = CCAPI_EXCHANGE_NAME_MARKET_DEPTH;
  std::string options;
  CorrelationId correlationId("this is my correlation id");
  Subscription subscription(topic, fields, options, correlationId);
  subscriptionList.add(subscription);
  session.subscribe(subscriptionList);
  return EXIT_SUCCESS;
}
