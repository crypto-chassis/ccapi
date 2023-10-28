#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#include <string>

#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
/**
 * This class contains the options which the user can specify when creating a session. To use non-default options on a Session, create a SessionOptions instance
 * and set the required options and then supply it when creating a Session.
 */
class SessionOptions CCAPI_FINAL {
 public:
  std::string toString() const {
    std::string output = "SessionOptions [enableCheckSequence = " + ccapi::toString(enableCheckSequence) +
                         ", enableCheckOrderBookChecksum = " + ccapi::toString(enableCheckOrderBookChecksum) +
                         ", enableCheckOrderBookCrossed = " + ccapi::toString(enableCheckOrderBookCrossed) +
                         ", enableCheckPingPongWebsocketProtocolLevel = " + ccapi::toString(enableCheckPingPongWebsocketProtocolLevel) +
                         ", enableCheckPingPongWebsocketApplicationLevel = " + ccapi::toString(enableCheckPingPongWebsocketApplicationLevel) +
                         ", enableCheckHeartbeatFix = " + ccapi::toString(enableCheckHeartbeatFix) +
                         ", pingWebsocketProtocolLevelIntervalMilliseconds = " + ccapi::toString(pingWebsocketProtocolLevelIntervalMilliseconds) +
                         ", pongWebsocketProtocolLevelTimeoutMilliseconds = " + ccapi::toString(pongWebsocketProtocolLevelTimeoutMilliseconds) +
                         ", pingWebsocketApplicationLevelIntervalMilliseconds = " + ccapi::toString(pingWebsocketApplicationLevelIntervalMilliseconds) +
                         ", pongWebsocketApplicationLevelTimeoutMilliseconds = " + ccapi::toString(pongWebsocketApplicationLevelTimeoutMilliseconds) +
                         ", heartbeatFixIntervalMilliseconds = " + ccapi::toString(heartbeatFixIntervalMilliseconds) +
                         ", heartbeatFixTimeoutMilliseconds = " + ccapi::toString(heartbeatFixTimeoutMilliseconds) +
                         ", maxEventQueueSize = " + ccapi::toString(maxEventQueueSize) + ", httpMaxNumRetry = " + ccapi::toString(httpMaxNumRetry) +
                         ", httpMaxNumRedirect = " + ccapi::toString(httpMaxNumRedirect) +
                         ", httpRequestTimeoutMilliseconds = " + ccapi::toString(httpRequestTimeoutMilliseconds) +
                         ", httpConnectionPoolMaxSize = " + ccapi::toString(httpConnectionPoolMaxSize) +
                         ", httpConnectionKeepAliveTimeoutSeconds = " + ccapi::toString(httpConnectionKeepAliveTimeoutSeconds) +
                         ", enableOneHttpConnectionPerRequest = " + ccapi::toString(enableOneHttpConnectionPerRequest) + "]";
    return output;
  }
  // long warnLateEventMaxMilliseconds{};                      // used to print a warning log message if en event arrives late
  bool enableCheckSequence{};                               // used to check sequence number discontinuity
  bool enableCheckOrderBookChecksum{};                      // used to check order book checksum
  bool enableCheckOrderBookCrossed{true};                   // used to check order book cross, usually this should be set to true
  bool enableCheckPingPongWebsocketProtocolLevel{true};     // used to check ping-pong health for exchange connections on websocket protocol level
  bool enableCheckPingPongWebsocketApplicationLevel{true};  // used to check ping-pong health for exchange connections on websocket application level
  bool enableCheckHeartbeatFix{true};                       // used to check heartbeat health for exchange connections on FIX
  long pingWebsocketProtocolLevelIntervalMilliseconds{60000};
  long pongWebsocketProtocolLevelTimeoutMilliseconds{30000};  // should be less than pingWebsocketProtocolLevelIntervalMilliseconds
  long pingWebsocketApplicationLevelIntervalMilliseconds{60000};
  long pongWebsocketApplicationLevelTimeoutMilliseconds{30000};  // should be less than pingWebsocketApplicationLevelIntervalMilliseconds
  long heartbeatFixIntervalMilliseconds{60000};
  long heartbeatFixTimeoutMilliseconds{30000};  // should be less than heartbeatFixIntervalMilliseconds
  int maxEventQueueSize{0};                     // if set to a positive integer, the event queue will throw an exception when overflown
  int httpMaxNumRetry{1};
  int httpMaxNumRedirect{1};
  long httpRequestTimeoutMilliseconds{10000};
  int httpConnectionPoolMaxSize{1};  // used to set the maximal number of http connections to be kept in the pool (connections in the pool are idle)
  long httpConnectionKeepAliveTimeoutSeconds{
      10};  // used to remove a http connection from the http connection pool if it has stayed idle for at least this amount of time
  bool enableOneHttpConnectionPerRequest{};  // create a new http connection for each request
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
  long websocketConnectTimeoutMilliseconds{10000};
#endif
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
