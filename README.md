<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [ccapi](#ccapi)
  - [Branches](#branches)
  - [Build](#build)
    - [C++](#c)
    - [Python](#python)
  - [Constants](#constants)
  - [Examples](#examples)
  - [Documentations](#documentations)
    - [Simple Market Data](#simple-market-data)
    - [Advanced Market Data](#advanced-market-data)
      - [Complex request parameters](#complex-request-parameters)
      - [Specify subscription market depth](#specify-subscription-market-depth)
      - [Specify correlation id](#specify-correlation-id)
      - [Multiple exchanges and/or instruments](#multiple-exchanges-andor-instruments)
      - [Receive subscription events at periodic intervals](#receive-subscription-events-at-periodic-intervals)
      - [Receive subscription events at periodic intervals including when the market depth snapshot hasn't changed](#receive-subscription-events-at-periodic-intervals-including-when-the-market-depth-snapshot-hasnt-changed)
      - [Receive subscription market depth updates](#receive-subscription-market-depth-updates)
      - [Receive subscription trade events](#receive-subscription-trade-events)
      - [Receive subscription OHLC events at periodic intervals](#receive-subscription-ohlc-events-at-periodic-intervals)
      - [Send generic public requests](#send-generic-public-requests)
      - [Make generic public subscriptions](#make-generic-public-subscriptions)
      - [Send generic private requests](#send-generic-private-requests)
    - [Simple Execution Management](#simple-execution-management)
    - [Advanced Execution Management](#advanced-execution-management)
      - [Specify correlation id](#specify-correlation-id-1)
      - [Multiple exchanges and/or instruments](#multiple-exchanges-andor-instruments-1)
      - [Multiple subscription fields](#multiple-subscription-fields)
      - [Make Session::sendRequest blocking](#make-sessionsendrequest-blocking)
      - [Provide API credentials for an exchange](#provide-api-credentials-for-an-exchange)
      - [Override exchange urls](#override-exchange-urls)
      - [Complex request parameters](#complex-request-parameters-1)
      - [Send request by Websocket API](#send-request-by-websocket-api)
    - [FIX API](#fix-api)
    - [More Advanced Topics](#more-advanced-topics)
      - [Handle events in "immediate" vs. "batching" mode](#handle-events-in-immediate-vs-batching-mode)
      - [Thread safety](#thread-safety)
      - [Enable library logging](#enable-library-logging)
      - [Set timer](#set-timer)
      - [Custom service class](#custom-service-class)
  - [Performance Tuning](#performance-tuning)
  - [Applications](#applications)
    - [Spot Market Making](#spot-market-making)
    - [Single Order Execution](#single-order-execution)
  - [Known Issues and Workarounds](#known-issues-and-workarounds)
  - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->
# ccapi
* A header-only C++ library for streaming market data and executing trades directly from cryptocurrency exchanges (i.e. the connections are between your server and the exchange server without anything in-between).
* Bindings for other languages such as Python are provided.
* Code closely follows Bloomberg's API: https://www.bloomberg.com/professional/support/api-library/.
* It is ultra fast thanks to very careful optimizations: move semantics, regex optimization, locality of reference, lock contention minimization, etc.
* Supported exchanges:
  * Market data: coinbase, gemini, kraken, kraken-futures, bitstamp, bitfinex, bitmex, binance-us, binance, binance-usds-futures, binance-coin-futures, huobi, huobi-usdt-swap, huobi-coin-swap, okex, erisx, kucoin, kucoin-futures, ftx, ftx-us, deribit, gateio, gateio-perpetual-futures, cryptocom, ascendex.
  * Execution Management: coinbase, gemini, kraken, kraken-futures, bitfinex, bitmex, binance-us, binance, binance-usds-futures, binance-coin-futures, huobi, huobi-usdt-swap, huobi-coin-swap, okex, erisx, kucoin, ftx, ftx-us, deribit, gateio, gateio-perpetual-futures, cryptocom, ascendex.
  * FIX: coinbase, gemini, ftx, ftx-us.
* A spot market making application is provided as an end-to-end solution for liquidity providers.
* A single order execution application is provided as an end-to-end solution for executing large orders.
* To spur innovation and industry collaboration, this library is open for use by the public without cost.
* For historical market data, see https://github.com/crypto-chassis/cryptochassis-api-docs.
* We specialize in market data collection, high speed trading system, infrastructure optimization, and proprietary market making. Hire us as engineers, liquidity providers, traders, or asset managers.
* Join us on Discord https://discord.gg/b5EKcp9s8T and Medium https://cryptochassis.medium.com.

## Branches
* The `develop` branch may contain experimental features.
* The `master` branch represents the most recent stable release.

## Build

### C++
* This library is header-only.
* Example CMake: example/CMakeLists.txt.
* Require C++14 and OpenSSL.
* Macros in the compiler command line:
  * Define service enablement macro such as `CCAPI_ENABLE_SERVICE_MARKET_DATA`, `CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT`, `CCAPI_ENABLE_SERVICE_FIX`, etc. and exchange enablement macros such as `CCAPI_ENABLE_EXCHANGE_COINBASE`, etc. These macros can be found at the top of [`include/ccapi_cpp/ccapi_session.h`](include/ccapi_cpp/ccapi_session.h).
* Include directories:
  * include.
  * dependency/websocketpp.
  * dependency/boost.
  * dependency/rapidjson/include.
  * If you need FIX API, also include dependency/hffix/include.
* Link libraries:
  * OpenSSL: libssl.
  * OpenSSL: libcrypto.
  * If you need market data for huobi/huobi-usdt-swap/huobi-coin-swap/okex or execution management for huobi-usdt-swap/huobi-coin-swap, also link ZLIB.
  * On Windows, also link ws2_32.
* Compiler flags:
  * `-pthread` for GCC and MinGW.
* Tested platforms:
  * macOS: Clang.
  * Linux: GCC.
  * Windows: MinGW.
* Troubleshoot:
  * Try to remove all build artifacts and start from scratch (e.g. for cmake remove all the contents inside your build directory).
  * "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_INCLUDE_DIR)". Try `cmake -DOPENSSL_ROOT_DIR=...`. On macOS, you might be missing headers for OpenSSL, `brew install openssl` and `cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl`. On Ubuntu, `sudo apt-get install libssl-dev`. On Windows, `vcpkg install openssl:x64-windows` and `cmake -DOPENSSL_ROOT_DIR=C:/vcpkg/installed/x64-windows-static`.
  * "Fatal error: can't write \<a> bytes to section .text of \<b>: 'File too big'". Try to add compiler flag `-Wa,-mbig-obj`. See https://github.com/assimp/assimp/issues/2067.
  * "string table overflow at offset \<a>". Try to add optimization flag `-O1` or `-O2`. See https://stackoverflow.com/questions/14125007/gcc-string-table-overflow-error-during-compilation.
  * On Windows, if you still encounter resource related issues, try to add optimization flag `-O3 -DNDEBUG`.

### Python
* Require Python 3, SWIG, and CMake.
  * SWIG: On macOS, `brew install SWIG`. On Linux, `sudo apt-get install -y swig`. On Windows, http://www.swig.org/Doc4.0/Windows.html#Windows.
  * CMake: https://cmake.org/download/.
* Copy file [`binding/user_specified_cmake_include.cmake.example`](binding/user_specified_cmake_include.cmake.example) to any location and rename to `user_specified_cmake_include.cmake`. Take note of its full path `<path-to-user_specified_cmake_include>`. Uncomment the lines corresponding to the desired service enablement compile definitions such as `CCAPI_ENABLE_SERVICE_MARKET_DATA`, `CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT`, `CCAPI_ENABLE_SERVICE_FIX`, etc. and exchange enablement macros such as `CCAPI_ENABLE_EXCHANGE_COINBASE`, etc. If you need market data for huobi/huobi-usdt-swap/huobi-coin-swap/okex or execution management for huobi-usdt-swap/huobi-coin-swap, also uncomment the lines corresponding to finding and linking ZLIB.
* Run the following commands.
```
mkdir binding/build
cd binding/build
rm -rf * (if rebuild from scratch)
cmake -DCMAKE_PROJECT_INCLUDE=<path-to-user_specified_cmake_include> -DBUILD_VERSION=<any-string-you-like> -DBUILD_PYTHON=ON -DINSTALL_PYTHON=ON ..
cmake --build .
cmake --install .
```
* If a virtual environment (managed by `venv` or `conda`) is active (i.e. the `activate` script has been evaluated), the package will be installed into the virtual environment rather than globally.
* Currently not working on Windows.
* Troubleshoot:
  * "CMake Error at python/CMakeLists.txt:... (message): Require Python 3". Try to create and activate a virtual environment (managed by `venv` or `conda`) with Python 3.
  * "Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_INCLUDE_DIR)". Try `cmake -DOPENSSL_ROOT_DIR=...`. On macOS, you might be missing headers for OpenSSL, `brew install openssl` and `cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl`. On Ubuntu, `sudo apt-get install libssl-dev`. On Windows, `vcpkg install openssl:x64-windows` and `cmake -DOPENSSL_ROOT_DIR=C:/vcpkg/installed/x64-windows-static`.
  * "Fatal Python error: Segmentation fault". If the macOS version is relatively new and the Python version is relatively old, please upgrade Python to a relatively new version.
  * "‘_PyObject_GC_UNTRACK’ was not declared in this scope". If you use Python >= 3.8, please use SWIG >= 4.0.

## Constants
[`include/ccapi_cpp/ccapi_macro.h`](include/ccapi_cpp/ccapi_macro.h)

## Examples
[C++](example) / [Python](binding/python/example)

Python API is nearly identical to C++ API and covers nearly all the functionalities from C++ API.

## Documentations

### Simple Market Data

**Objective 1:**

For a specific exchange and instrument, get recents trades.

**Code 1:**

[C++](example/src/market_data_simple_request/main.cpp) / [Python](binding/python/example/src/market_data_simple_request/main.py)
```
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    std::cout << "Received an event:\n" + event.toStringPretty(2, 2) << std::endl;
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::toString;
int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Request request(Request::Operation::GET_RECENT_TRADES, "coinbase", "BTC-USD");
  request.appendParam({
      {"LIMIT", "1"},
  });
  session.sendRequest(request);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
```

**Output 1:**
```console
Received an event:
  Event [
    type = RESPONSE,
    messageList = [
      Message [
        type = GET_RECENT_TRADES,
        recapType = UNKNOWN,
        time = 2021-05-25T03:23:31.124000000Z,
        timeReceived = 2021-05-25T03:23:31.239734000Z,
        elementList = [
          Element [
            nameValueMap = {
              IS_BUYER_MAKER = 1,
              LAST_PRICE = 38270.71,
              LAST_SIZE = 0.001,
              TRADE_ID = 178766798
            }
          ]
        ],
        correlationIdList = [ 5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE ]
      ]
    ]
  ]
Bye
```
* Request operation types: `GET_INSTRUMENT`, `GET_INSTRUMENTS`, `GET_RECENT_TRADES`, `GET_RECENT_AGG_TRADES`(only applicable to binance family: https://binance-docs.github.io/apidocs/spot/en/#compressed-aggregate-trades-list).
* Request parameter names: `LIMIT`, `INSTRUMENT_TYPE`. Instead of these convenient names you can also choose to use arbitrary parameter names and they will be passed to the exchange's native API. See [this example](example/src/market_data_advanced_request/main.cpp).
* Message's `time` represents the exchange's reported timestamp. Its `timeReceived` represents the library's receiving timestamp. `time` can be retrieved by `getTime` method and `timeReceived` can be retrieved by `getTimeReceived` method. (For Python, please use `getTimeISO` method and `getTimeReceivedISO` method).

**Objective 2:**

For a specific exchange and instrument, whenever the best bid's or ask's price or size changes, print the market depth snapshot at that moment.

**Code 2:**

[C++](example/src/market_data_simple_subscription/main.cpp) / [Python](binding/python/example/src/market_data_simple_subscription/main.py)
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
using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
int main(int argc, char **argv) {
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

**Output 2:**
```console
Best bid and ask at 2020-07-27T23:56:51.884855000Z are:
  {BID_PRICE=10995, BID_SIZE=0.22187803}
  {ASK_PRICE=10995.44, ASK_SIZE=2}
Best bid and ask at 2020-07-27T23:56:51.935993000Z are:
  ...
```
* Subscription fields: `MARKET_DEPTH`, `TRADE`, `AGG_TRADE`(only applicable to binance family: https://binance-docs.github.io/apidocs/spot/en/#aggregate-trade-streams).

### Advanced Market Data

#### Complex request parameters
Please follow the exchange's API documentations: e.g. https://docs.pro.coinbase.com/#pagination.
```
Request request(Request::Operation::GET_RECENT_TRADES, "coinbase", "BTC-USD");
request.appendParam({
  {"before", "1"},
  {"after", "3"},
  {"limit", "1"},
});
```

#### Specify subscription market depth

Instantiate `Subscription` with option `MARKET_DEPTH_MAX` set to be the desired market depth (e.g. you want to receive market depth snapshot whenever the top 10 bid's or ask's price or size changes).
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "MARKET_DEPTH_MAX=10");
```

#### Specify correlation id

Instantiate `Request` with the desired correlationId.
```
Request request(Request::Operation::GET_RECENT_TRADES, "coinbase", "BTC-USD", "cool correlation id");
```
Instantiate `Subscription` with the desired correlationId.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "", "cool correlation id");
```
This is used to match a particular request or subscription with its returned data. Within each `Message` there is a `correlationIdList` to identify the request or subscription that requested the data.

#### Multiple exchanges and/or instruments

Send a `std::vector<Request>`.
```
Request request_1(Request::Operation::GET_RECENT_TRADES, "coinbase", "BTC-USD");
request_1.appendParam(...);
Request request_2(Request::Operation::GET_RECENT_TRADES, "coinbase", "ETH-USD");
request_2.appendParam(...);
session.sendRequest({request_1, request_2});
```
Subscribe a `std::vector<Subscription>`.
```
Subscription subscription_1("coinbase", "BTC-USD", "MARKET_DEPTH");
Subscription subscription_2("binance-us", "ethusd", "MARKET_DEPTH");
session.subscribe({subscription_1, subscription_2});
```

#### Receive subscription events at periodic intervals

Instantiate `Subscription` with option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000");
```

#### Receive subscription events at periodic intervals including when the market depth snapshot hasn't changed

Instantiate `Subscription` with option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval and `CONFLATE_GRACE_PERIOD_MILLISECONDS` to be the grace period for late events.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "CONFLATE_INTERVAL_MILLISECONDS=1000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
```

#### Receive subscription market depth updates

Instantiate `Subscription` with option `MARKET_DEPTH_RETURN_UPDATE` set to 1. This will return the order book updates instead of snapshots.
```
Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH", "MARKET_DEPTH_RETURN_UPDATE=1&MARKET_DEPTH_MAX=2");
```

#### Receive subscription trade events

Instantiate `Subscription` with field `TRADE`.
```
Subscription subscription("coinbase", "BTC-USD", "TRADE");
```

#### Receive subscription OHLC events at periodic intervals

Instantiate `Subscription` with field `TRADE` and option `CONFLATE_INTERVAL_MILLISECONDS` set to be the desired interval and `CONFLATE_GRACE_PERIOD_MILLISECONDS` to be your network latency.
```
Subscription subscription("coinbase", "BTC-USD", "TRADE", "CONFLATE_INTERVAL_MILLISECONDS=5000&CONFLATE_GRACE_PERIOD_MILLISECONDS=0");
```

#### Send generic public requests

Instantiate `Request` with operation `GENERIC_PUBLIC_REQUEST`. Provide request parameters `HTTP_METHOD`, `HTTP_PATH`, and optionally `HTTP_QUERY_STRING` (query string parameter values should be url-encoded), `HTTP_BODY`.
```
Request request(Request::Operation::GENERIC_PUBLIC_REQUEST, "binance");
request.appendParam({
    {"HTTP_METHOD", "GET"},
    {"HTTP_PATH", "/api/v3/historicalTrades"},
    {"HTTP_QUERY_STRING", "symbol=BTCUSDT"},
});
```

#### Make generic public subscriptions

Instantiate `Subscription` with empty instrument, field `GENERIC_PUBLIC_SUBSCRIPTION` and options set to be the desired websocket payload.
```
Subscription subscription("coinbase", "", "GENERIC_PUBLIC_SUBSCRIPTION", R"({"type":"subscribe","channels":[{"name":"status"}]})");
```

#### Send generic private requests

Instantiate `Request` with operation `GENERIC_PRIVATE_REQUEST`. Provide request parameters `HTTP_METHOD`, `HTTP_PATH`, and optionally `HTTP_QUERY_STRING` (query string parameter values should be url-encoded), `HTTP_BODY`.
```
Request request(Request::Operation::GENERIC_PRIVATE_REQUEST, "coinbase");
request.appendParam({
    {"HTTP_METHOD", "GET"},
    {"HTTP_PATH", "/fills"},
    {"HTTP_QUERY_STRING", "product_id=BTC-USD"},
});
```

### Simple Execution Management

**Objective 1:**

For a specific exchange and instrument, submit a simple limit order.

**Code 1:**

[C++](example/src/execution_management_simple_request/main.cpp) / [Python](binding/python/example/src/execution_management_simple_request/main.py)
```
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    std::cout << "Received an event: " + event.toStringPretty(2, 2) << std::endl;
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::toString;
using ::ccapi::UtilSystem;
int main(int argc, char** argv) {
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

**Output 1:**
```console
Received an event:
  Event [
    type = RESPONSE,
    messageList = [
      Message [
        type = CREATE_ORDER,
        recapType = UNKNOWN,
        time = 1970-01-01T00:00:00.000000000Z,
        timeReceived = 2021-05-25T03:47:15.599562000Z,
        elementList = [
          Element [
            nameValueMap = {
              CLIENT_ORDER_ID = wBgmzOJbbMTCLJlwTrIeiH,
              CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY = 0.0000,
              CUMULATIVE_FILLED_QUANTITY = 0.00000000,
              INSTRUMENT = BTCUSD,
              LIMIT_PRICE = 20000.0000,
              ORDER_ID = 383781246,
              QUANTITY = 0.00100000,
              SIDE = BUY,
              STATUS = NEW
            }
          ]
        ],
        correlationIdList = [ 5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE ]
      ]
    ]
  ]
Bye
```
* Request operation types: `CREATE_ORDER`, `CANCEL_ORDER`, `GET_ORDER`, `GET_OPEN_ORDERS`, `CANCEL_OPEN_ORDERS`, `GET_ACCOUNTS`, `GET_ACCOUNT_BALANCES`, `GET_ACCOUNT_POSITIONS`.
* Request parameter names: `SIDE`, `QUANTITY`, `LIMIT_PRICE`, `ACCOUNT_ID`, `ACCOUNT_TYPE`, `ORDER_ID`, `CLIENT_ORDER_ID`, `PARTY_ID`, `ORDER_TYPE`, `LEVERAGE`. Instead of these convenient names you can also choose to use arbitrary parameter names and they will be passed to the exchange's native API. See [this example](example/src/execution_management_advanced_request/main.cpp).

**Objective 2:**

For a specific exchange and instrument, receive order updates.

**Code 2:**

[C++](example/src/execution_management_simple_subscription/main.cpp) / [Python](binding/python/example/src/execution_management_simple_subscription/main.py)
```
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
      std::cout << "Received an event of type SUBSCRIPTION_STATUS:\n" + event.toStringPretty(2, 2) << std::endl;
      auto message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::SUBSCRIPTION_STARTED) {
        Request request(Request::Operation::CREATE_ORDER, "coinbase", "BTC-USD");
        request.appendParam({
            {"SIDE", "BUY"},
            {"LIMIT_PRICE", "20000"},
            {"QUANTITY", "0.001"},
        });
        session->sendRequest(request);
      }
    } else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      std::cout << "Received an event of type SUBSCRIPTION_DATA:\n" + event.toStringPretty(2, 2) << std::endl;
    }
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::toString;
using ::ccapi::UtilSystem;
int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("COINBASE_API_KEY").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_SECRET").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("coinbase", "BTC-USD", "ORDER_UPDATE");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
```

**Output 2:**
```console
Received an event of type SUBSCRIPTION_STATUS:
  Event [
    type = SUBSCRIPTION_STATUS,
    messageList = [
      Message [
        type = SUBSCRIPTION_STARTED,
        recapType = UNKNOWN,
        time = 1970-01-01T00:00:00.000000000Z,
        timeReceived = 2021-05-25T04:22:25.906197000Z,
        elementList = [

        ],
        correlationIdList = [ 5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE ]
      ]
    ]
  ]
Received an event of type SUBSCRIPTION_DATA:
  Event [
    type = SUBSCRIPTION_DATA,
    messageList = [
      Message [
        type = EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE,
        recapType = UNKNOWN,
        time = 2021-05-25T04:22:26.653785000Z,
        timeReceived = 2021-05-25T04:22:26.407419000Z,
        elementList = [
          Element [
            nameValueMap = {
              CLIENT_ORDER_ID = ,
              INSTRUMENT = BTC-USD,
              LIMIT_PRICE = 20000,
              ORDER_ID = 6ca39186-be79-4777-97ab-1695fccd0ce4,
              QUANTITY = 0.001,
              SIDE = BUY,
              STATUS = received
            }
          ]
        ],
        correlationIdList = [ 5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE ]
      ]
    ]
  ]
Received an event of type SUBSCRIPTION_DATA:
  Event [
    type = SUBSCRIPTION_DATA,
    messageList = [
      Message [
        type = EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE,
        recapType = UNKNOWN,
        time = 2021-05-25T04:22:26.653785000Z,
        timeReceived = 2021-05-25T04:22:26.407704000Z,
        elementList = [
          Element [
            nameValueMap = {
              INSTRUMENT = BTC-USD,
              LIMIT_PRICE = 20000,
              ORDER_ID = 6ca39186-be79-4777-97ab-1695fccd0ce4,
              REMAINING_QUANTITY = 0.001,
              SIDE = BUY,
              STATUS = open
            }
          ]
        ],
        correlationIdList = [ 5PN2qmWqBlQ9wQj99nsQzldVI5ZuGXbE ]
      ]
    ]
  ]
Bye
```
* Subscription fields: `ORDER_UPDATE`, `PRIVATE_TRADE`.

### Advanced Execution Management

#### Specify correlation id

Instantiate `Request` with the desired correlationId.
```
Request request(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD", "cool correlation id");
```
Instantiate `Subscription` with the desired correlationId.
```
Subscription subscription("coinbase", "BTC-USD", "ORDER_UPDATE", "", "cool correlation id");
```
This is used to match a particular request or subscription with its returned data. Within each `Message` there is a `correlationIdList` to identify the request or subscription that requested the data.

#### Multiple exchanges and/or instruments

Send a `std::vector<Request>`.
```
Request request_1(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD");
request_1.appendParam(...);
Request request_2(Request::Operation::CREATE_ORDER, "binance-us", "ETHUSD");
request_2.appendParam(...);
session.sendRequest({request_1, request_2});
```
Subscribe one `Subscription` per exchange with a comma separated string of instruments.
```
Subscription subscription("coinbase", "BTC-USD,ETH-USD", "ORDER_UPDATE");
```

#### Multiple subscription fields

Subscribe one `Subscription` with a comma separated string of fields.
```
Subscription subscription("coinbase", "BTC-USD", "ORDER_UPDATE,PRIVATE_TRADE");
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

#### Provide API credentials for an exchange
There are 3 ways to provide API credentials (listed with increasing priority).
* Set the relevent environment variables. Some exchanges might need additional credentials other than API keys and secrets: e.g. `COINBASE_API_PASSPHRASE`, `KUCOIN_API_PASSPHRASE`, `KUCOIN_API_KEY_VERSION`, `FTX_API_SUBACCOUNT`. See section "exchange API credentials" in [`include/ccapi_cpp/ccapi_macro.h`](include/ccapi_cpp/ccapi_macro.h).
* Provide credentials to `SessionConfigs`.
```
sessionConfigs.setCredential({
  {"BINANCE_US_API_KEY", ...},
  {"BINANCE_US_API_SECRET", ...}
});
```
* Provide credentials to `Request` or `Subscription`.
```
Request request(Request::Operation::CREATE_ORDER, "binance-us", "BTCUSD", "", {
  {"BINANCE_US_API_KEY", ...},
  {"BINANCE_US_API_SECRET", ...}
});
```
```
Subscription subscription("coinbase", "BTC-USD", "ORDER_UPDATE", "", "", {
  {"COINBASE_API_KEY", ...},
  {"COINBASE_API_SECRET", ...}
});
```

#### Override exchange urls
See section "exchange REST urls", "exchange WS urls", and "exchange FIX urls" in [`include/ccapi_cpp/ccapi_macro.h`](include/ccapi_cpp/ccapi_macro.h). This can be useful if you need to connect to test accounts (e.g. https://docs.pro.coinbase.com/#sandbox).

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

#### Send request by Websocket API
```
Subscription subscription("okex", "BTC-USDT", "ORDER_UPDATE", "", "same correlation id for subscription and request");
session.subscribe(subscription);
...
Request request(Request::Operation::CREATE_ORDER, "okex", "BTC-USDT", "same correlation id for subscription and request");
request.appendParam({
    {"SIDE", "BUY"},
    {"LIMIT_PRICE", "20000"},
    {"QUANTITY", "0.001"},
});
session.sendRequestByWebsocket(request);
```

### FIX API

**Objective:**

For a specific exchange and instrument, submit a simple limit order.

**Code:**

[C++](example/src/fix_simple/main.cpp) / [Python](binding/python/example/src/fix_simple/main.py)
```
#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::AUTHORIZATION_STATUS) {
      std::cout << "Received an event of type AUTHORIZATION_STATUS:\n" + event.toStringPretty(2, 2) << std::endl;
      auto message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::AUTHORIZATION_SUCCESS) {
        Request request(Request::Operation::FIX, "coinbase", "", "same correlation id for subscription and request");
        request.appendParamFix({
            {35, "D"},
            {11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"},
            {55, "BTC-USD"},
            {54, "1"},
            {44, "20000"},
            {38, "0.001"},
            {40, "2"},
            {59, "1"},
        });
        session->sendRequestByFix(request);
      }
    } else if (event.getType() == Event::Type::FIX) {
      std::cout << "Received an event of type FIX:\n" + event.toStringPretty(2, 2) << std::endl;
    }
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;
int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("COINBASE_API_KEY").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_SECRET").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("coinbase", "", "FIX", "", "same correlation id for subscription and request");
  session.subscribeByFix(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
```
**Output:**
```console
Received an event of type AUTHORIZATION_STATUS:
  Event [
    type = AUTHORIZATION_STATUS,
    messageList = [
      Message [
        type = AUTHORIZATION_SUCCESS,
        recapType = UNKNOWN,
        time = 1970-01-01T00:00:00.000000000Z,
        timeReceived = 2021-05-25T05:05:15.892366000Z,
        elementList = [
          Element [
            tagValueMap = {
              96 = 0srtt0WetUTYHiTpvyWnC+XKKHCzQQIJ/8G9lE4KVxM=,
              98 = 0,
              108 = 15,
              554 = 26abh7of52i
            }
          ]
        ],
        correlationIdList = [ same correlation id for subscription and request ]
      ]
    ]
  ]
Received an event of type FIX:
  Event [
    type = FIX,
    messageList = [
      Message [
        type = FIX,
        recapType = UNKNOWN,
        time = 1970-01-01T00:00:00.000000000Z,
        timeReceived = 2021-05-25T05:05:15.984090000Z,
        elementList = [
          Element [
            tagValueMap = {
              11 = 6d4eb0fb-2229-469f-873e-557dd78ac11e,
              17 = b7caec79-1bc8-460e-af28-6489cf12f45e,
              20 = 0,
              37 = 458acfe5-bdea-46d2-aa87-933cda84163f,
              38 = 0.001,
              39 = 0,
              44 = 20000,
              54 = 1,
              55 = BTC-USD,
              60 = 20210525-05:05:16.008,
              150 = 0
            }
          ]
        ],
        correlationIdList = [ same correlation id for subscription and request ]
      ]
    ]
  ]
Bye
```

### More Advanced Topics

#### Handle events in "immediate" vs. "batching" mode

In general there are 2 ways to handle events.
* When a `Session` is instantiated with an `eventHandler` argument, it will handle events in immediate mode. The `processEvent` method in the `eventHandler` will be invoked immediately when an `Event` is available.
* When a `Session` is instantiated without an `eventHandler` argument, it will handle events in batching mode. The evetns will be batched into an internal `Queue<Event>` and can be retrieved by
```
std::vector<Event> eventList = session.getEventQueue().purge();
```
An example can be found [here](example/src/market_data_advanced_subscription/main.cpp).

#### Thread safety
* The following methods are implemented to be thread-safe: `Session::sendRequest`, `Session::subscribe`, `Session::sendRequestByFix`, `Session::subscribeByFix`, `Session::setTimer`, all public methods in `Queue`.
* The `processEvent` method in the `eventHandler` is invoked on one of the internal threads in the `eventDispatcher`. A default `EventDispatcher` with 1 internal thread will be created if no `eventDispatcher` argument is provided in `Session` instantiation. To dispatch events to multiple threads, instantiate `EventDispatcher` with `numDispatcherThreads` set to be the desired number. `EventHandler`s and/or `EventDispatcher`s can be shared among different sessions. Otherwise, different sessions are independent from each other.
```
EventDispatcher eventDispatcher(2);
Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
```
An example can be found [here](example/src/market_data_advanced_subscription/main.cpp).

#### Enable library logging

[C++](example/src/enable_library_logging/main.cpp) / [Python](binding/python/example/src/enable_library_logging/main.py)

Extend a subclass, e.g. `MyLogger`, from class `Logger` and override method `logMessage`. Assign a `MyLogger` pointer to `Logger::logger`. Add one of the following macros in the compiler command line: `CCAPI_ENABLE_LOG_TRACE`, `CCAPI_ENABLE_LOG_DEBUG`, `CCAPI_ENABLE_LOG_INFO`, `CCAPI_ENABLE_LOG_WARN`, `CCAPI_ENABLE_LOG_ERROR`, `CCAPI_ENABLE_LOG_FATAL`. Enable logging if you'd like to inspect raw responses/messages from the exchange for troubleshooting purposes.
```
namespace ccapi {
  class MyLogger final : public Logger {
   public:
    void logMessage(const std::string& severity, const std::string& threadId, const std::string& timeISO, const std::string& fileName,
                    const std::string& lineNumber, const std::string& message) override {
                      ...
    }
  };
MyLogger myLogger;
Logger* Logger::logger = &myLogger;
}
```

#### Set timer

[C++](example/src/utility_set_timer/main.cpp)

To perform an asynchronous wait, use the utility method `setTimer` in class `Session`. The handlers are invoked in the same threads as the `processEvent` method in the `EventHandler` class.
```
session->setTimer(
    "id", 1000,
    [](const boost::system::error_code&) {
      std::cout << std::string("Timer error handler is triggered at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl;
    },
    []() { std::cout << std::string("Timer success handler is triggered at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl; });
```

#### Custom service class

[C++](example/src/custom_service_class/main.cpp)

Define macro `CCAPI_EXPOSE_INTERNAL`. Extend a subclass e.g. `MyService` from class `Service`. Inject a `MyService` pointer into `Session`. E.g.
```
session.serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_COINBASE] =
    std::make_shared<ExecutionManagementServiceCoinbaseCustom>(session.internalEventHandler, session.sessionOptions, session.sessionConfigs,
                                                               session.serviceContextPtr);
```

## Performance Tuning
* Turn on compiler optimization flags (e.g. `cmake -DCMAKE_BUILD_TYPE=Release ...`).
* Enable link time optimization (e.g. in CMakeLists.txt `set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)` before a target is created). Note that link time optimization is only applicable to static linking.
* Shorten constant strings used as key names in the returned `Element` (e.g. in CmakeLists.txt `add_compile_definitions(CCAPI_BEST_BID_N_PRICE="b")`).
* Only enable the services and exchanges that you need.
* Use FIX API instead of REST API.
* Handle events in ["batching" mode](#handle-events-in-immediate-vs-batching-mode) if your application (e.g. market data archiver) isn't latency sensitive.

## Applications

### Spot Market Making
* Source code: [app](app)
* The code uses a simplified version of Avellaneda & Stoikov’s inventory strategy: https://www.math.nyu.edu/~avellane/HighFrequencyTrading.pdf. See the [parameter configuration file `app/src/spot_market_making/config.env.example`](app/src/spot_market_making/config.env.example) for more details. And read more at https://medium.com/open-crypto-market-data-initiative/simplified-avellaneda-stoikov-market-making-608b9d437403.
* Require CMake.
  * CMake: https://cmake.org/download/.
* Copy file [`app/user_specified_cmake_include.cmake.example`](app/user_specified_cmake_include.cmake.example) to any location and rename to `user_specified_cmake_include.cmake`. Take note of its full path `<path-to-user_specified_cmake_include>`. Uncomment the lines corresponding to the desired exchange enablement macros such as `CCAPI_ENABLE_EXCHANGE_COINBASE`, etc. If you need okex, also uncomment the lines corresponding to finding and linking ZLIB.
* Run the following commands.
```
mkdir app/build
cd app/build
rm -rf * (if rebuild from scratch)
cmake -DCMAKE_PROJECT_INCLUDE=<path-to-user_specified_cmake_include> ..
cmake --build . --target spot_market_making
```
* The executable is `app/build/src/spot_market_making/spot_market_making`. Run it after setting relevant environment variables shown in [`app/src/spot_market_making/config.env.example`](app/src/spot_market_making/config.env.example). For example, we can copy file `config.env.example` to `config.env`, edit it, and `export $(grep -v '^#' config.env | xargs)`. To enable and configure advanced parameters, set additional environment variables shown in [`app/src/spot_market_making/config_advanced.env.example`](app/src/spot_market_making/config_advanced.env.example).
* For live trade mode, please set the desired exchange's credential environment variables shown in [app/credential.env.example](app/credential.env.example).
* For paper trade mode and backtest mode, please see the [parameter configuration file `app/src/spot_market_making/config.env.example`](app/src/spot_market_making/config.env.example) for more details.

### Single Order Execution
* Source code: [app](app)
* The supported strategies are listed in [`app/src/single_order_execution/config.env.example`](app/src/single_order_execution/config.env.example).
* Require CMake.
  * CMake: https://cmake.org/download/.
* Copy file [`app/user_specified_cmake_include.cmake.example`](app/user_specified_cmake_include.cmake.example) to any location and rename to `user_specified_cmake_include.cmake`. Take note of its full path `<path-to-user_specified_cmake_include>`. Uncomment the lines corresponding to the desired exchange enablement macros such as `CCAPI_ENABLE_EXCHANGE_COINBASE`, etc. If you need okex, also uncomment the lines corresponding to finding and linking ZLIB.
* Run the following commands.
```
mkdir app/build
cd app/build
rm -rf * (if rebuild from scratch)
cmake -DCMAKE_PROJECT_INCLUDE=<path-to-user_specified_cmake_include> ..
cmake --build . --target single_order_execution
```
* The executable is `app/build/src/single_order_execution/single_order_execution`. Run it after setting relevant environment variables shown in [`app/src/single_order_execution/config.env.example`](app/src/single_order_execution/config.env.example). For example, we can copy file `config.env.example` to `config.env`, edit it, and `export $(grep -v '^#' config.env | xargs)`. To enable and configure advanced parameters, set additional environment variables shown in [`app/src/single_order_execution/config_advanced.env.example`](app/src/single_order_execution/config_advanced.env.example).
* For live trade mode, please set the desired exchange's credential environment variables shown in [app/credential.env.example](app/credential.env.example).
* For paper trade mode and backtest mode, please see the [parameter configuration file `app/src/single_order_execution/config.env.example`](app/src/single_order_execution/config.env.example) for more details.

## Known Issues and Workarounds
* Kraken invalid nonce errors. Give the API key a nonce window (https://support.kraken.com/hc/en-us/articles/360001148023-What-is-a-nonce-window-). We use unix timestamp with microsecond resolution as nonce and therefore a nonce window of 500000 translates to a tolerance of 0.5 second.

## Contributing
* (Required) Create a new branch from the `develop` branch and submit a pull request to the `develop` branch.
* (Optional) C++ code style: https://google.github.io/styleguide/cppguide.html. See file [.clang-format](.clang-format).
* (Optional) Commit message format: https://conventionalcommits.org.
