<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [ccapi_cpp](#ccapi_cpp)
  - [Usage](#usage)
  - [Build](#build)
  - [Constants](#constants)
  - [Examples](#examples)
    - [Simple](#simple)
    - [Advanced](#advanced)
      - [Specify market depth](#specify-market-depth)
      - [Specify correlation id](#specify-correlation-id)
      - [Normalize instrument name](#normalize-instrument-name)
      - [Multiple exchanges and/or instruments](#multiple-exchanges-andor-instruments)
      - [Receive events at periodic intervals](#receive-events-at-periodic-intervals)
      - [Receive events at periodic intervals including when the market depth snapshot hasn't changed](#receive-events-at-periodic-intervals-including-when-the-market-depth-snapshot-hasnt-changed)
      - [Dispatch events to multiple threads](#dispatch-events-to-multiple-threads)
      - [Handle Events Synchronously](#handle-events-synchronously)
      - [Enable library logging](#enable-library-logging)
    - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

**NEW**: Version 2.0.0 has been released. The APIs remain to be largely the same to version 1.x.x except for a few small breaking changes with the most prominent one being renaming of some of the macros needed at build time.
# ccapi_cpp
* A header-only C++ library for streaming public market data directly from cryptocurrency exchanges (i.e. the connections are between your server and the exchange server without anything in-between).
* Code closely follows Bloomberg's API: https://www.bloomberg.com/professional/support/api-library/.
* It is ultra fast thanks to very careful optimizations: move semantics, regex optimization, locality of reference, lock contention minimization, etc.
* Supported exchanges: coinbase, gemini, kraken, bitstamp, bitfinex, bitmex, binance-us, binance, binance-futures, huobi, okex.
* To spur innovation and industry collaboration, this library is open for use by the public without cost. Follow us on https://medium.com/@cryptochassis and our publication on https://medium.com/open-crypto-market-data-initiative.
* For historical data, see https://github.com/crypto-chassis/cryptochassis-api-docs.
* Since symbol normalization is a tedious task, you can choose to use a reference file at https://marketdata-e0323a9039add2978bf5b49550572c7c-public.s3.amazonaws.com/supported_exchange_instrument_subscription_data.csv.gz which we frequently update.
* Please contact us for general questions, issue reporting, consultative services, and/or custom engineering work. To subscribe to our mailing list, simply send us an email with subject "subscribe".

## Usage
* Real-time market data fetching, high frequency market making, cross exchange arbitrage, etc.
* For stability, please usa stable releases. Master branch might contain experimental features.

## Build
* This library is header-only.
* Example CMake: example/CMakeLists.txt.
* Require C++14 and OpenSSL.
* Definitions in the compiler command line:
  * Define service enablement macro `ENABLE_SERVICE_MARKET_DATA` and exchange enablement macros such as `ENABLE_EXCHANGE_COINBASE`, etc. These macros can be found at the top of `include/ccapi_cpp/ccapi_session.h`.
* Include directories:
  * include
  * dependency/websocketpp
  * dependency/boost
  * dependency/rapidjson/include
  * dependency/date/include
* Link libraries:
  * OpenSSL: libssl
  * OpenSSL: libcrypto
  * If you need huobi or okex, also link ZLIB.
* Troubleshoot:
  * "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_INCLUDE_DIR)": try `cmake -DOPENSSL_ROOT_DIR=...`. On macOS, you might be missing headers for OpenSSL. `brew install openssl` and `cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl`.
  * "No such file or directory" for thread-related headers if Windows MinGW without posix threads is used: please enable it (https://stackoverflow.com/questions/17242516/mingw-w64-threads-posix-vs-win32) or use Boost (so that e.g. boost/thread.hpp can be found).

## Constants
`include/ccapi_cpp/ccapi_macro.h`

## Examples
[Source](example)
### Simple
**Objective:**

For a specific exchange and instrument, whenever the top 10 bids' or asks' price or size change, print the market depth snapshot at that moment.

**Code:**
```
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      for (const auto & message : event.getMessageList()) {
        if (message.getRecapType() == Message::RecapType::NONE) {
          std::cout << std::string("Best bid and ask at ") + UtilTime::getISOTimestamp(message.getTime()) + " are:"
                    << std::endl;
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
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
```
**Output:**
```console
Best bid and ask at 2020-07-27T23:56:51.884855000Z are:
  {BID_PRICE=10995, BID_SIZE=0.22187803}
  {ASK_PRICE=10995.44, ASK_SIZE=2}
Best bid and ask at 2020-07-27T23:56:51.935993000Z are:
  ...
```

### Advanced
#### Specify market depth

Instantiate `Subscription` with option `MARKET_DEPTH_MAX` set to be the desired market depth.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "MARKET_DEPTH_MAX=10");
```

#### Specify correlation id

Instantiate `Subscription` with the desired correlationId.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "", "cool correlation id");
```

#### Normalize instrument name

Instantiate `SessionConfigs` with a map mapping the exchange name and the normalized instrument name to the instrument's symbol on the exchange.
```
std::map<std::string, std::map<std::string, std::string> > exchangeInstrumentSymbolMap;
std::string coolName = "btc_usd";
exchangeInstrumentSymbolMap["coinbase"][coolName] = "BTC-USD";
SessionConfigs sessionConfigs(exchangeInstrumentSymbolMap);
Session session(sessionOptions, sessionConfigs, &eventHandler);
Subscription subscription("coinbase", coolName, "MARKET_DEPTH");
```

#### Multiple exchanges and/or instruments

Subscribe a `std::vector<Subscription>`.
```
std::vector<Subscription> subscriptionList;
Subscription subscription_1("coinbase", "BTC-USD", "MARKET_DEPTH", "", "coinbase|btc_usd");
subscriptionList.push_back(subscription_1);
Subscription subscription_2("binance-us", "ethusd", "MARKET_DEPTH", "", "binance-us|eth_usd");
subscriptionList.push_back(subscription_2);
session.subscribe(subscriptionList);
```

#### Receive events at periodic intervals

Instantiate `Subscription` with option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000");
```

#### Receive events at periodic intervals including when the market depth snapshot hasn't changed

Instantiate `Subscription` with option `CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval and `CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS` to be your network latency.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
```

#### Dispatch events to multiple threads

Instantiate `EventDispatcher` with
`numDispatcherThreads` set to be the desired number.
```
EventDispatcher eventDispatcher(2);
Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
```

#### Handle Events Synchronously

Instantiate `Session` without `EventHandler`, then obtain the events to be processed by calling `session.eventQueue.purge()`.
```
Session session(sessionOptions, sessionConfigs);
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
session.subscribe(subscription);
std::this_thread::sleep_for(std::chrono::seconds(5));
std::vector<Event> eventList = session.eventQueue.purge();
```

#### Enable library logging

Extend a subclass, e.g. `MyLogger`, from class `Logger` and override method `logMessage`. Assign a `MyLogger` pointer to `Logger::logger`. Add one of the following macros in the compiler command line: `ENABLE_LOG_TRACE`, `ENABLE_LOG_DEBUG`, `ENABLE_LOG_INFO`, `ENABLE_LOG_WARN`, `ENABLE_LOG_ERROR`, `ENABLE_LOG_FATAL`.
```
namespace ccapi {
class MyLogger final: public Logger {
  virtual void logMessage(std::string severity, std::string threadId,
                          std::string timeISO,
                          std::string fileName, int lineNumber,
                          std::string message) override {
    ...                          
  }
};
MyLogger myLogger;
Logger* Logger::logger = &myLogger;
}
```
### Contributing
* (Required) Submit a pull request to the master branch.
* (Required) Pass Github checks: https://docs.github.com/en/rest/reference/checks.
* (Optional) Commit message format: https://conventionalcommits.org.
