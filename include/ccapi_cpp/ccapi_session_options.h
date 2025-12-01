#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#include <optional>
#include <string>
#include <vector>

#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"

namespace ccapi {

/**
 * This class contains the options which the user can specify when creating a session. To use non-default options on a Session, create a SessionOptions instance
 * and set the required options and then supply it when creating a Session.
 */
class SessionOptions {
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
                         ", enableOneHttpConnectionPerRequest = " + ccapi::toString(enableOneHttpConnectionPerRequest) +
                         ", enableHttpConnectionPoolMultiIP = " + ccapi::toString(enableHttpConnectionPoolMultiIP) +
                         ", httpConnectionPoolBindIPs = " + ccapi::toString(httpConnectionPoolBindIPs) +
                         ", enableHttpConnectionPoolKeepAlive = " + ccapi::toString(enableHttpConnectionPoolKeepAlive) +
                         ", httpConnectionPoolKeepAliveIntervalSeconds = " + ccapi::toString(httpConnectionPoolKeepAliveIntervalSeconds) +
                         ", httpConnectionPoolKeepAliveMethod = " + httpConnectionPoolKeepAliveMethod +
                         ", httpConnectionPoolKeepAlivePath = " + httpConnectionPoolKeepAlivePath +
                         ", enableHttpConnectionPoolAutoReconnect = " + ccapi::toString(enableHttpConnectionPoolAutoReconnect) +
                         ", httpConnectionPoolReconnectMaxRetries = " + ccapi::toString(httpConnectionPoolReconnectMaxRetries) +
                         ", httpConnectionPoolReconnectDelayMilliseconds = " + ccapi::toString(httpConnectionPoolReconnectDelayMilliseconds) +
                         ", websocketConnectTimeoutMilliseconds = " + ccapi::toString(websocketConnectTimeoutMilliseconds) +
                         ", fixConnectTimeoutMilliseconds = " + ccapi::toString(fixConnectTimeoutMilliseconds) + "]";
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
  int httpConnectionPoolMaxSize{2};  // used to set the maximal number of http connections to be kept in the pool (connections in the pool are idle)
  long httpConnectionKeepAliveTimeoutSeconds{
      10};  // used to remove a http connection from the http connection pool if it has stayed idle for at least this amount of time
  bool enableOneHttpConnectionPerRequest{};  // create a new http connection for each request

  // HTTP连接池多IP支持
  bool enableHttpConnectionPoolMultiIP{false};  // 启用多IP连接池，每个连接绑定不同的本地IP
  std::vector<std::string> httpConnectionPoolBindIPs;  // 连接池绑定的本地IP列表，例如: {"192.168.1.100", "192.168.1.101"}

  // HTTP连接池主动保活机制
  bool enableHttpConnectionPoolKeepAlive{true};  // 启用HTTP连接保活，定期发送轻量级请求保持连接活跃
  long httpConnectionPoolKeepAliveIntervalSeconds{30};  // 保活间隔（秒），建议30-60秒
  std::string httpConnectionPoolKeepAliveMethod{"HEAD"};  // 保活HTTP方法：HEAD（推荐）/OPTIONS/GET
  std::string httpConnectionPoolKeepAlivePath{"/api/v3/ping"};  // 保活请求路径，币安使用 /api/v3/ping

  // HTTP连接池自动重连机制
  bool enableHttpConnectionPoolAutoReconnect{true};  // 启用自动重连，连接断开时自动尝试重新建立
  int httpConnectionPoolReconnectMaxRetries{3};  // 最大重连尝试次数
  long httpConnectionPoolReconnectDelayMilliseconds{1000};  // 重连延迟（毫秒），每次重连前等待时间

  long websocketConnectTimeoutMilliseconds{10000};
  long fixConnectTimeoutMilliseconds{10000};
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
