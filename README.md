<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [ccapi](#ccapi)
  - [Usage](#usage)
  - [Build](#build)
    - [C++](#c)
    - [Python](#python)
  - [Constants](#constants)
  - [Examples](#examples)
    - [Simple Market Data](#simple-market-data)
    - [Advanced Market Data](#advanced-market-data)
      - [Specify market depth](#specify-market-depth)
      - [Specify correlation id](#specify-correlation-id)
      - [Normalize instrument name](#normalize-instrument-name)
      - [Multiple exchanges and/or instruments](#multiple-exchanges-andor-instruments)
      - [Receive events at periodic intervals](#receive-events-at-periodic-intervals)
      - [Receive events at periodic intervals including when the market depth snapshot hasn't changed](#receive-events-at-periodic-intervals-including-when-the-market-depth-snapshot-hasnt-changed)
      - [Receive market depth updates](#receive-market-depth-updates)
      - [Receive trade events](#receive-trade-events)
      - [Receive OHLC events at periodic intervals](#receive-ohlc-events-at-periodic-intervals)
    - [Simple Execution Management](#simple-execution-management)
    - [Advanced Execution Management](#advanced-execution-management)
      - [Specify correlation id](#specify-correlation-id-1)
      - [Normalize instrument name](#normalize-instrument-name-1)
      - [Multiple exchanges and/or instruments](#multiple-exchanges-andor-instruments-1)
      - [Make Session::sendRequest blocking](#make-sessionsendrequest-blocking)
      - [Multiple sets of API credentials for the same exchange](#multiple-sets-of-api-credentials-for-the-same-exchange)
      - [Override exchange urls](#override-exchange-urls)
      - [Complex request parameters](#complex-request-parameters)
    - [More Advanced Topics](#more-advanced-topics)
      - [Handle events in "immediate" vs. "batching" mode](#handle-events-in-immediate-vs-batching-mode)
      - [Thread safety](#thread-safety)
      - [Enable library logging](#enable-library-logging)
    - [Performance Tuning](#performance-tuning)
    - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

**Small breaking change in v3.2.x**: `SessionOptions` `enableCheckPingPong`(default to false) has been replaced by `enableCheckPingPongWebsocketProtocolLevel`(default to true) and `enableCheckPingPongWebsocketApplicationLevel`(default to true).

# ccapi
* A header-only C++ library for streaming market data and executing trades directly from cryptocurrency exchanges (i.e. the connections are between your server and the exchange server without anything in-between).
* Bindings for other languages such as Python are provided.
* Code closely follows Bloomberg's API: https://www.bloomberg.com/professional/support/api-library/.
* It is ultra fast thanks to very careful optimizations: move semantics, regex optimization, locality of reference, lock contention minimization, etc.
* Supported exchanges:
  * Market data: coinbase, gemini, kraken, bitstamp, bitfinex, bitmex, binance-us, binance, binance-futures, huobi, okex, erisx, kucoin.
  * Execution Management: coinbase, gemini, bitmex, binance-us, binance, binance-futures, huobi, erisx.
* To spur innovation and industry collaboration, this library is open for use by the public without cost.
* For historical market data, see https://github.com/crypto-chassis/cryptochassis-api-docs.
* Since symbol normalization is a tedious task, you can choose to use a reference file at https://marketdata-e0323a9039add2978bf5b49550572c7c-public.s3.amazonaws.com/supported_exchange_instrument_subscription_data.csv.gz which we frequently update.
* Please contact us for general questions, issue reporting, consultative services, and/or custom engineering work. To subscribe to our mailing list, simply send us an email with subject "subscribe".
* Join us on Medium https://cryptochassis.medium.com and Telegram https://t.me/cryptochassis.

## Usage
* Real-time market data fetching, high frequency market making, cross exchange arbitrage, etc.
* For stability, please usa stable releases. Master branch might contain experimental features.

## Build

### C++
* This library is header-only.
* Example CMake: example/CMakeLists.txt.
* Require C++14 and OpenSSL.
* Macros in the compiler command line:
  * Define service enablement macro such as `CCAPI_ENABLE_SERVICE_MARKET_DATA`, `CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT`, etc. and exchange enablement macros such as `CCAPI_ENABLE_EXCHANGE_COINBASE`, etc. These macros can be found at the top of `include/ccapi_cpp/ccapi_session.h`.
* Include directories:
  * include
  * dependency/websocketpp
  * dependency/boost
  * dependency/rapidjson/include
* Link libraries:
  * OpenSSL: libssl
  * OpenSSL: libcrypto
  * If you need huobi or okex, also link ZLIB.
  * On Windows, also link ws2_32.
* Compiler flags:
  * `-pthread` for GCC and MinGW.
* Tested platforms:
  * macOS: Clang.
  * Linux: GCC.
  * Windows: MinGW.
* Troubleshoot:
  * "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_INCLUDE_DIR)". Try `cmake -DOPENSSL_ROOT_DIR=...`. On macOS, you might be missing headers for OpenSSL, `brew install openssl` and `cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl`. On Windows, `vcpkg install openssl:x64-windows` and `cmake -DOPENSSL_ROOT_DIR=C:/vcpkg/installed/x64-windows-static`.
  * "Fatal error: can't write \<a> bytes to section .text of \<b>: 'File too big'". Try to add compiler flag `-Wa,-mbig-obj`. See https://github.com/assimp/assimp/issues/2067.
  * "string table overflow at offset \<a>". Try to add optimization flag `-O1` or `-O2`. See https://stackoverflow.com/questions/14125007/gcc-string-table-overflow-error-during-compilation.

### Python
* Require Python 3, SWIG, and CMake.
  * SWIG: On macOS, `brew install SWIG`. On Linux, `sudo apt-get install -y swig`. On Windows, http://www.swig.org/Doc4.0/Windows.html#Windows.
  * CMake: https://cmake.org/download/.
* Copy file `binding/user_specified_cmake_include.cmake.example` to any location and rename to `user_specified_cmake_include.cmake`. Take note of its full path `<path-to-user_specified_cmake_include>`. Uncomment the lines corresponding to the desired service enablement compile definitions such as `CCAPI_ENABLE_SERVICE_MARKET_DATA`, `CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT`, etc. and exchange enablement macros such as `CCAPI_ENABLE_EXCHANGE_COINBASE`, etc. If you need huobi or okex, also uncomment the lines corresponding to finding ZLIB.
* Run the following commands.
```
mkdir binding/build
cd binding/build
cmake -DCMAKE_PROJECT_INCLUDE=<path-to-user_specified_cmake_include> -DBUILD_VERSION=... -DBUILD_PYTHON=ON -DINSTALL_PYTHON=ON ..
cmake --build . -j
cmake --install .
```
* If a virtual environment (managed by `venv` or `conda`) is active (i.e. the `activate` script has been evaluated), the package will be installed into the virtual environment rather than globally.
* Currently not working on Windows.

## Constants
`include/ccapi_cpp/ccapi_macro.h`

## Examples
[C++](example) / [Python](binding/python/example)

Python API is nearly identical to C++ API, please refer to C++ for more examples.

### Simple Market Data
[C++](example/src/market_data_simple/main.cpp) / [Python](binding/python/example/src/market_data_simple/main.py)

**Objective:**

For a specific exchange and instrument, whenever the best bid's or ask's price or size changes, print the market depth snapshot at that moment.

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
        std::cout << std::string("Best bid and ask at ") + UtilTime::getISOTimestamp(message.getTime()) + " are:"
                  << std::endl;
        for (const auto & element : message.getElementList()) {
          const std::map<std::string, std::string>& elementNameValueMap = element.getNameValueMap();
          std::cout << "  " + toString(elementNameValueMap) << std::endl;
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
* Subscription fields: `MARKET_DEPTH`, `TRADE`.

### Advanced Market Data

#### Specify market depth

Instantiate `Subscription` with option `MARKET_DEPTH_MAX` set to be the desired market depth (e.g. you want to receive market depth snapshot whenever the top 10 bid's or ask's price or size changes).
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
```

#### Multiple exchanges and/or instruments

Subscribe a `std::vector<Subscription>`.
```
std::vector<Subscription> subscriptionList;
Subscription subscription_1("coinbase", "BTC-USD", "MARKET_DEPTH");
subscriptionList.push_back(subscription_1);
Subscription subscription_2("binance-us", "ethusd", "MARKET_DEPTH");
subscriptionList.push_back(subscription_2);
session.subscribe(subscriptionList);
```

#### Receive events at periodic intervals

Instantiate `Subscription` with option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000");
```

#### Receive events at periodic intervals including when the market depth snapshot hasn't changed

Instantiate `Subscription` with option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval and `CONFLATE_GRACE_PERIOD_MILLISECONDS` to be your network latency.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
```

#### Receive market depth updates

Instantiate `Subscription` with option `MARKET_DEPTH_RETURN_UPDATE` set to 1.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "MARKET_DEPTH_RETURN_UPDATE=1&MARKET_DEPTH_MAX=2");
```

#### Receive trade events

Instantiate `Subscription` with field `TRADE`.
```
Subscription subscription("coinbase", "BTC-USD", "TRADE");
```

#### Receive OHLC events at periodic intervals

Instantiate `Subscription` with field `TRADE` and option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval and `CONFLATE_GRACE_PERIOD_MILLISECONDS` to be your network latency.
```
Subscription subscription("coinbase", "BTC-USD", "TRADE", "CONFLATE_INTERVAL_MILLISECONDS=5000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
```

### Simple Execution Management

[C++](example/src/execution_management_simple/main.cpp) / [Python](binding/python/example/src/execution_management_simple/main.py)

**Objective:**

For a specific exchange and instrument, submit a simple limit order.

**Code:**
```
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    std::cout << "Received an event: " + toString(event) << std::endl;
    return true;
  }
};
} /* namespace ccapi */
int main(int argc, char** argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
  std::string key = UtilSystem::getEnvAsString("BINANCE_US_API_KEY");
  if (key.empty()) {
    std::cerr << "Please set environment variable BINANCE_US_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  std::string secret = UtilSystem::getEnvAsString("BINANCE_US_API_SECRET");
  if (secret.empty()) {
    std::cerr << "Please set environment variable BINANCE_US_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Request request(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD");
  request.appendParam({
    {"SIDE", "BUY"},
    {"QUANTITY", "0.0005"},
    {"LIMIT_PRICE", "20000"}
  });
  session.sendRequest(request);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
```
**Output:**
```console
Received an event:
  Event [
    type = RESPONSE,
    messageList = [
      Message [
        type = CREATE_ORDER,
        recapType = UNKNOWN,
        time = 1970-01-01T00:00:00.000000000Z,
        timeReceived = 2021-01-04T04:15:04.710133000Z,
        elementList = [
          Element [
            nameValueMap = {
              CLIENT_ORDER_ID = MbdTQCHc0EQgLKry0Ryrhr,
              CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY = 0.0000,
              CUMULATIVE_FILLED_QUANTITY = 0.00000000,
              INSTRUMENT = BTCUSD,
              LIMIT_PRICE = 20000.0000,
              ORDER_ID = 187143156,
              QUANTITY = 0.00050000,
              SIDE = BUY,
              STATUS = OPEN
            }
          ]
        ],
        correlationIdList = [ 5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE ]
      ]
    ]
  ]
Bye
```
* Request operation types: `CREATE_ORDER`, `CANCEL_ORDER`, `GET_ORDER`, `GET_OPEN_ORDERS`, `CANCEL_OPEN_ORDERS`.
* Request parameter names: `SIDE`, `QUANTITY`, `LIMIT_PRICE`, `ACCOUNT_ID`, `ORDER_ID`, `CLIENT_ORDER_ID`, `PARTY_ID`.

### Advanced Execution Management

#### Specify correlation id

Instantiate `Request` with the desired correlationId.
```
Request request(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD", "cool correlation id");
```

#### Normalize instrument name

Instantiate `SessionConfigs` with a map mapping the exchange name and the normalized instrument name to the instrument's symbol on the exchange.
```
std::map<std::string, std::map<std::string, std::string> > exchangeInstrumentSymbolMap;
std::string coolName = "btc_usd";
exchangeInstrumentSymbolMap["coinbase"][coolName] = "BTC-USD";
SessionConfigs sessionConfigs(exchangeInstrumentSymbolMap);
Session session(sessionOptions, sessionConfigs, &eventHandler);
```

#### Multiple exchanges and/or instruments

Send a `std::vector<Request>`.
```
std::vector<Request> requestList;
Request request_1(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD");
request_1.appendParam(...);
requestList.push_back(request_1);
Request request_2(Request::Operation::CREATE_ORDER, "binance-us", "ETHUSD");
request_2.appendParam(...);
requestList.push_back(request_2);
session.sendRequest(requestList);
```

#### Make Session::sendRequest blocking
Instantiate `Session` without `EventHandler` argument, and pass a pointer to `Queue<Event>` as an additional argument.
```
Session session(sessionOptions, sessionConfigs);
...
Queue<Event> eventQueue;
session.sendRequest(request, &eventQueue);  // block until a response is received
std::vector<Event> eventList = eventQueue.purge();
```

#### Multiple sets of API credentials for the same exchange
There are 3 ways to provide API credentials (listed with increasing priority).
* Set the relevent environment variables (see section "exchange API credentials" in `include/ccapi_cpp/ccapi_macro.h`).
* Provide credentials to `SessionConfigs`.
```
sessionConfigs.setCredential({
  {"BINANCE_US_API_KEY", ...},
  {"BINANCE_US_API_SECRET", ...}
});
```
* Provide credentials to `Request`.
```
Request request(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD", "cool correlation id", {
  {"BINANCE_US_API_KEY", ...},
  {"BINANCE_US_API_SECRET", ...}
});
```

#### Override exchange urls
See section "exchange REST urls" in `include/ccapi_cpp/ccapi_macro.h`. This can be useful if you need to connect to test accounts (e.g. https://docs.pro.coinbase.com/#sandbox).

#### Complex request parameters
Please follow the exchange's API documentations: e.g. https://github.com/binance-us/binance-official-api-docs/blob/master/rest-api.md#new-order--trade.
```
Request request(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD");
request.appendParam({
  {"side", "SELL"},
  {"type", "STOP_LOSS_LIMIT"},
  {"quantity", "0.0005"},
  {"stopPrice", "20001"},
  {"price", "20000"},
  {"timeInForce", "GTC"}
});
```

### More Advanced Topics

#### Handle events in "immediate" vs. "batching" mode

In general there are 2 ways to handle events.
* When a `Session` is instantiated with an `eventHandler` argument, it will handle events in immediate mode. The `processEvent` method in the `eventHandler` will be executed on one of the internal threads in the `eventDispatcher`. A default `EventDispatcher` with 1 internal thread will be created if no `eventDispatcher` argument is provided in `Session` instantiation. To dispatch events to multiple threads, instantiate `EventDispatcher` with `numDispatcherThreads` set to be the desired number. `EventHandler`s and/or `EventDispatcher`s can be shared among different sessions. Otherwise, different sessions are independent from each other.
```
EventDispatcher eventDispatcher(2);
Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
```
* When a `Session` is instantiated without an `eventHandler` argument, it will handle events in batching mode. The evetns will be batched into an internal `Queue<Event>` and can be retrieved by
```
std::vector<Event> eventList = session.getEventQueue().purge();
```

#### Thread safety
The following methods are implemented to be thread-safe: `Session::subscribe`, `Session::sendRequest`, all public methods in `Queue`.

#### Enable library logging
[C++](example/src/enable_library_logging/main.cpp) / [Python](binding/python/example/src/enable_library_logging/main.py)

Extend a subclass, e.g. `MyLogger`, from class `Logger` and override method `logMessage`. Assign a `MyLogger` pointer to `Logger::logger`. Add one of the following macros in the compiler command line: `CCAPI_ENABLE_LOG_TRACE`, `CCAPI_ENABLE_LOG_DEBUG`, `CCAPI_ENABLE_LOG_INFO`, `CCAPI_ENABLE_LOG_WARN`, `CCAPI_ENABLE_LOG_ERROR`, `CCAPI_ENABLE_LOG_FATAL`.
```
namespace ccapi {
class MyLogger final: public Logger {
  void logMessage(std::string severity,
                          std::string threadId,
                          std::string timeISO,
                          std::string fileName,
                          std::string lineNumber,
                          std::string message) override {
    ...                          
  }
};
MyLogger myLogger;
Logger* Logger::logger = &myLogger;
}
```

### Performance Tuning
* Turn on compiler optimization flags (e.g. `cmake -DCMAKE_BUILD_TYPE=Release ...`).
* Enable link time optimization (e.g. in CMakeLists.txt `set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)` before a target is created).
* Shorten constant strings used as key names in the returned `Element` (e.g. in CmakeLists.txt `add_compile_definitions(CCAPI_BEST_BID_N_PRICE="b")`).
* Only enable the services and exchanges that you need.

### Contributing
* (Required) Submit a pull request to the master branch.
* (Optional) Commit message format: https://conventionalcommits.org.
