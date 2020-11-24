#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
#include <string>
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class SessionOptions final {
 public:
  std::string toString() const {
    std::string output = "SessionOptions [warnLateEventMaxMilliSeconds = " + ccapi::toString(warnLateEventMaxMilliSeconds)
        + ", enableCheckSequence = " + ccapi::toString(enableCheckSequence) + ", enableCheckOrderBookChecksum = "
        + ccapi::toString(enableCheckOrderBookChecksum) + ", enableCheckOrderBookCrossed = "
        + ccapi::toString(enableCheckOrderBookCrossed) + ", enableCheckPingPong = "
        + ccapi::toString(enableCheckPingPong) + ", enableOneConnectionPerSubscription = "
        + ccapi::toString(enableOneConnectionPerSubscription) + ", pingIntervalMilliSeconds = "
        + ccapi::toString(pingIntervalMilliSeconds) + ", pongTimeoutMilliSeconds = "
        + ccapi::toString(pongTimeoutMilliSeconds) + "]";
    return output;
  }
  long warnLateEventMaxMilliSeconds{};
  bool enableCheckSequence{};
  bool enableCheckOrderBookChecksum{};
  bool enableCheckOrderBookCrossed{};
  bool enableCheckPingPong{};
  bool enableOneConnectionPerSubscription{};
  bool enableOneIoContextPerExchange{};
  long pingIntervalMilliSeconds{10000};
  long pongTimeoutMilliSeconds{5000};
  long maxEventQueueSize{0};
  bool enableOneHttpConnectionPerRequest{};
  int httpMaxNumRetry{3};
  int httpMaxNumRedirect{3};
  long httpRequestTimeoutMilliSeconds{10000};
  int httpConnectionPoolMaxSize{1};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
