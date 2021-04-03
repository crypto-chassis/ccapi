#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#include <string>
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class SessionOptions CCAPI_FINAL {
 public:
  std::string toString() const {
    std::string output =
        "SessionOptions [warnLateEventMaxMilliSeconds = " + ccapi::toString(warnLateEventMaxMilliSeconds) +
        ", enableCheckSequence = " + ccapi::toString(enableCheckSequence) +
        ", enableCheckOrderBookChecksum = " + ccapi::toString(enableCheckOrderBookChecksum) +
        ", enableCheckOrderBookCrossed = " + ccapi::toString(enableCheckOrderBookCrossed) +
        ", enableCheckPingPongWebsocketProtocolLevel = " + ccapi::toString(enableCheckPingPongWebsocketProtocolLevel) +
        ", enableCheckPingPongWebsocketApplicationLevel = " + ccapi::toString(enableCheckPingPongWebsocketApplicationLevel) +
        ", pingIntervalMilliSeconds = " + ccapi::toString(pingIntervalMilliSeconds) +
        ", pongTimeoutMilliSeconds = " + ccapi::toString(pongTimeoutMilliSeconds) + "]";
    return output;
  }
  long warnLateEventMaxMilliSeconds{};     // used to print a warning log message if en event arrives late
  bool enableCheckSequence{};              // used to check sequence number discontinuity
  bool enableCheckOrderBookChecksum{};     // used to check order book checksum
  bool enableCheckOrderBookCrossed{true};  // used to check order book cross, usually this should be set to true
  bool enableCheckPingPongWebsocketProtocolLevel{
      true};  // used to check ping-pong health for exchange connections on websocket protocol level
  bool enableCheckPingPongWebsocketApplicationLevel{
      true};  // used to check ping-pong health for exchange connections on websocket application level
  long pingIntervalMilliSeconds{10000};
  long pongTimeoutMilliSeconds{5000};
  int maxEventQueueSize{0};  // if set to a positive integer, the event queue will throw an exception when overflown
  bool enableOneHttpConnectionPerRequest{};
  int httpMaxNumRetry{3};
  int httpMaxNumRedirect{3};
  long httpRequestTimeoutMilliSeconds{10000};
  int httpConnectionPoolMaxSize{
      1};  // used to set the maximal number of http connections to be kept in the pool (connections in the pool are idle)
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
