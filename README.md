<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [ccapi_cpp](#ccapi_cpp)
  - [Build](#build)
  - [Examples](#examples)
    - [Simple](#simple)
    - [Advanced](#advanced)
    - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# ccapi_cpp
* A header-only C++ library for streaming public market data directly from cryptocurrency exchanges (i.e. the connections are between your server and the exchange server without anything in-between).
* Code closely follows Bloomberg's API: https://www.bloomberg.com/professional/support/api-library/.
* It is ultra fast thanks to very careful optimizations: move semantics, regex optimization, locality of reference, lock contention minimization, etc.
* Supported exchanges: coinbase, gemini, kraken, bitstamp, bitfinex, bitmex.
* To spur innovation and industry collaboration, this library is open for use by the public without cost. Follow us on https://medium.com/@cryptochassis and our publication on https://medium.com/open-crypto-market-data-initiative.
* For historical data, see https://github.com/crypto-chassis/cryptochassis-api-docs.
* Since symbol normalization is a tedious task, you can choose to use a reference file at https://marketdata-e0323a9039add2978bf5b49550572c7c-public.s3.amazonaws.com/supported_exchange_instrument_subscription_data.csv.gz which we frequently update.
* Please contact us for general questions, issue reporting, consultative services, and/or custom engineering work. To subscribe to our mailing list, simply send us an email with subject "subscribe".

## Build
* This library is header-only.
* Example CMake: example/CMakeLists.txt.
* Require C++14 and OpenSSL.
* Definitions in the compiler command line:
  * If you need all supported exchanges, define macro ENABLE_ALL_EXCHANGE. Otherwise, define exchange specific macros such as ENABLE_COINBASE, etc. See include/ccapi_cpp/ccapi_enable_exchange.h.
* Include directories:
  * include
  * dependency/websocketpp_tag_0.8.2
  * dependency/asio_tag_asio-1-17-0/asio/include (If you use Boost Asio, this should be your Boost include directory, and you also need to define WEBSOCKETPP_USE_BOOST_ASIO in the compiler command line)
  * dependency/rapidjson_tag_v1.1.0/include
  * dependency/date_tag_v3.0.0/include
* Link libraries:
  * OpenSSL: libssl
  * OpenSSL: libcrypto
* Troubleshoot:
  * "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_INCLUDE_DIR)": try cmake -DOPENSSL_ROOT_DIR=...(e.g. /usr/local/opt/openssl)
  * "No such file or directory" for thread-related headers if Windows MinGW without posix threads is used: please enable it (https://stackoverflow.com/questions/17242516/mingw-w64-threads-posix-vs-win32) or use Boost (so that e.g. boost/thread.hpp can be found).

## Examples

### Simple
**Objective:**

For a specific exchange and instrument, whenever the top 10 bids' or asks' price or size change, print the market depth snapshot at that moment.

**Code:**
```
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
  return 0;
}
```
**Output:**
```console
Top 10 bids and asks at 2020-07-27T23:56:51.884855000Z are:
  {BID_PRICE=10995, BID_SIZE=0.22187803}
  {BID_PRICE=10994.03, BID_SIZE=0.2}
  {BID_PRICE=10993.71, BID_SIZE=5.229}
  {BID_PRICE=10993.61, BID_SIZE=0.27282}
  {BID_PRICE=10993.58, BID_SIZE=0.1248}
  {BID_PRICE=10992.13, BID_SIZE=0.4162}
  {BID_PRICE=10992.08, BID_SIZE=0.27282}
  {BID_PRICE=10991.71, BID_SIZE=0.622}
  {BID_PRICE=10991.19, BID_SIZE=3.9}
  {BID_PRICE=10990.02, BID_SIZE=5.308}
  {ASK_PRICE=10995.44, ASK_SIZE=2}
  {ASK_PRICE=10998.95, ASK_SIZE=1.359}
  {ASK_PRICE=10999.81, ASK_SIZE=0.19414243}
  {ASK_PRICE=10999.9, ASK_SIZE=0.00827739}
  {ASK_PRICE=10999.91, ASK_SIZE=0.79238425}
  {ASK_PRICE=10999.92, ASK_SIZE=1.27862516}
  {ASK_PRICE=11000, ASK_SIZE=7.27699068}
  {ASK_PRICE=11001.94, ASK_SIZE=0.012}
  {ASK_PRICE=11002.16, ASK_SIZE=1.361}
  {ASK_PRICE=11002.86, ASK_SIZE=1.13036461}
Top 10 bids and asks at 2020-07-27T23:56:51.935993000Z are:
  ...
```

### Advanced
**Multiple exchanges and/or instruments**

Instantiate SessionConfigs with a map containing the exchange names, the instrument names specified by you, and the instrument names specified by the exchange.
```
std::map<std::string, std::map<std::string, std::string> > exchangeInstrumentSymbolMap;
exchangeInstrumentSymbolMap[CCAPI_EXCHANGE_NAME_COINBASE]["btc-usd name specified by you"] = "BTC-USD"; // Coinbase names a trading pair using upper case concatenated by dash
exchangeInstrumentSymbolMap[CCAPI_EXCHANGE_NAME_COINBASE]["eth-usd name specified by you"] = "ETH-USD"; // Coinbase names a trading pair using upper case concatenated by dash
SessionConfigs sessionConfigs(exchangeInstrumentSymbolMap);
```

**Specify market depth**

Instantiate Subscription with option CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX set to be the desired market depth, e.g.
```
std::string options = std::string(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX) + "=2";
Subscription subscription(topic, fields, options, correlationId);
```

**Only receive events at periodic intervals**

Instantiate Subscription with option CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS set to be the desired interval, e.g. if you want to only receive market depth snapshots at whole seconds
```
std::string options = std::string(CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS) + "=1000";
Subscription subscription(topic, fields, options, correlationId);
```

**Only receive events at periodic intervals including when the market depth snapshot has not changed yet**

Instantiate Subscription with option CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS set to be the desired interval and CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS to be your network latency, e.g. if you want to only receive market depth snapshots at each and every second regardless of whether the market depth snapshot hasn't changed or not, and your network is faster than the speed of light
```
std::string options = std::string(CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS) + "=1000&" + CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS + "=0";
Subscription subscription(topic, fields, options, correlationId);
```

**Dispatching events from multiple threads**

Instantiate EventDispatcher with numDispatcherThreads set to be the desired number, e.g.
```
EventDispatcher eventDispatcher(2);
Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
```

**Enable library logging**

Add one of the following macros in the compiler command line: ENABLE_TRACE_LOG, ENABLE_DEBUG_LOG, ENABLE_INFO_LOG, ENABLE_WARN_LOG, ENABLE_ERROR_LOG, ENABLE_FATAL_LOG. Extend a subclass, e.g. MyLogger, from class Logger and override method logMessage. Assign a MyLogger pointer to Logger::logger.
```
Logger* Logger::logger = 0;  // This line is needed.
class MyLogger final: public Logger {
 public:
  virtual void logMessage(Logger::Severity severity, std::thread::id threadId,
                          std::chrono::system_clock::time_point time,
                          std::string fileName, int lineNumber,
                          std::string message) override {
    std::cout << threadId << ": [" << UtilTime::getISOTimestamp(time) << "] {"
        << fileName << ":" << lineNumber << "} "
        << Logger::severityToString(severity) << std::string(8, ' ') << message
        << std::endl;
  }
};
...
MyLogger myLogger;
Logger::logger = &myLogger;
```

### Contributing
* (Required) Submit a pull request to the master branch.
* (Required) Pass Github checks: https://docs.github.com/en/rest/reference/checks.
* (Optional) Commit message format: https://github.com/conventional-changelog/conventional-changelog/tree/master/packages/conventional-changelog-eslint#eslint-convention.
