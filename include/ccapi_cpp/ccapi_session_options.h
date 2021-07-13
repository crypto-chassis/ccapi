#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#include <string>
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class SessionOptions CCAPI_FINAL {
 public:
  std::string toString() const {
    std::string output = "SessionOptions [warnLateEventMaxMilliSeconds = " + ccapi::toString(warnLateEventMaxMilliSeconds) +
                         ", enableCheckSequence = " + ccapi::toString(enableCheckSequence) +
                         ", enableCheckOrderBookChecksum = " + ccapi::toString(enableCheckOrderBookChecksum) +
                         ", enableCheckOrderBookCrossed = " + ccapi::toString(enableCheckOrderBookCrossed) +
                         ", enableCheckPingPongWebsocketProtocolLevel = " + ccapi::toString(enableCheckPingPongWebsocketProtocolLevel) +
                         ", enableCheckPingPongWebsocketApplicationLevel = " + ccapi::toString(enableCheckPingPongWebsocketApplicationLevel) +
                         ", enableCheckHeartbeatFix = " + ccapi::toString(enableCheckHeartbeatFix) +
                         ", pingWebsocketProtocolLevelIntervalMilliSeconds = " + ccapi::toString(pingWebsocketProtocolLevelIntervalMilliSeconds) +
                         ", pongWebsocketProtocolLevelTimeoutMilliSeconds = " + ccapi::toString(pongWebsocketProtocolLevelTimeoutMilliSeconds) +
                         ", pingWebsocketApplicationLevelIntervalMilliSeconds = " + ccapi::toString(pingWebsocketApplicationLevelIntervalMilliSeconds) +
                         ", pongWebsocketApplicationLevelTimeoutMilliSeconds = " + ccapi::toString(pongWebsocketApplicationLevelTimeoutMilliSeconds) +
                         ", heartbeatFixIntervalMilliSeconds = " + ccapi::toString(heartbeatFixIntervalMilliSeconds) +
                         ", heartbeatFixTimeoutMilliSeconds = " + ccapi::toString(heartbeatFixTimeoutMilliSeconds) +
                         ", maxEventQueueSize = " + ccapi::toString(maxEventQueueSize) + ", httpMaxNumRetry = " + ccapi::toString(httpMaxNumRetry) +
                         ", httpMaxNumRedirect = " + ccapi::toString(httpMaxNumRedirect) +
                         ", httpRequestTimeoutMilliSeconds = " + ccapi::toString(httpRequestTimeoutMilliSeconds) +
                         ", httpConnectionPoolMaxSize = " + ccapi::toString(httpConnectionPoolMaxSize) +
                         ", httpConnectionPoolIdleTimeoutMilliSeconds = " + ccapi::toString(httpConnectionPoolIdleTimeoutMilliSeconds) +
                         ", enableOneHttpConnectionPerRequest = " + ccapi::toString(enableOneHttpConnectionPerRequest) + "]";
    return output;
  }
  long warnLateEventMaxMilliSeconds{};                      // used to print a warning log message if en event arrives late
  bool enableCheckSequence{};                               // used to check sequence number discontinuity
  bool enableCheckOrderBookChecksum{};                      // used to check order book checksum
  bool enableCheckOrderBookCrossed{true};                   // used to check order book cross, usually this should be set to true
  bool enableCheckPingPongWebsocketProtocolLevel{true};     // used to check ping-pong health for exchange connections on websocket protocol level
  bool enableCheckPingPongWebsocketApplicationLevel{true};  // used to check ping-pong health for exchange connections on websocket application level
  bool enableCheckHeartbeatFix{true};                       // used to check heartbeat health for exchange connections on FIX
  long pingWebsocketProtocolLevelIntervalMilliSeconds{15000};
  long pongWebsocketProtocolLevelTimeoutMilliSeconds{1};  // should be less than pingWebsocketProtocolLevelIntervalMilliSeconds
  long pingWebsocketApplicationLevelIntervalMilliSeconds{15000};
  long pongWebsocketApplicationLevelTimeoutMilliSeconds{1};  // should be less than pingWebsocketApplicationLevelIntervalMilliSeconds
  long heartbeatFixIntervalMilliSeconds{15000};
  long heartbeatFixTimeoutMilliSeconds{10000};  // should be less than heartbeatFixIntervalMilliSeconds
  int maxEventQueueSize{0};                     // if set to a positive integer, the event queue will throw an exception when overflown
  int httpMaxNumRetry{1};
  int httpMaxNumRedirect{1};
  long httpRequestTimeoutMilliSeconds{10000};
  int httpConnectionPoolMaxSize{1};  // used to set the maximal number of http connections to be kept in the pool (connections in the pool are idle)
  long httpConnectionPoolIdleTimeoutMilliSeconds{
      0};  // used to purge the http connection pool if all connections in the pool have stayed idle for at least this amount of time
  bool enableOneHttpConnectionPerRequest{};  // create a new http connection for each request
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
