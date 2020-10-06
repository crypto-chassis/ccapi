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
        + ccapi::toString(enableCheckOrderBookCrossed) + ", enableCheckHeartbeat = "
        + ccapi::toString(enableCheckHeartbeat) + ", enableOneConnectionPerSubscription = "
        + ccapi::toString(enableOneConnectionPerSubscription) + ", pingIntervalMilliSeconds = "
        + ccapi::toString(pingIntervalMilliSeconds) + ", pongTimeoutMilliSeconds = "
        + ccapi::toString(pongTimeoutMilliSeconds) + "]";
    return output;
  }
//  std::string defaultSubscriptionService{"// cc/mktdata"};
  long warnLateEventMaxMilliSeconds{};
  bool enableCheckSequence{};
  bool enableCheckOrderBookChecksum{};
  bool enableCheckOrderBookCrossed{};
  bool enableCheckHeartbeat{};
  bool enableOneConnectionPerSubscription{};
  bool enableOneIoContextPerExchange{};
  long pingIntervalMilliSeconds{10000};
  long pongTimeoutMilliSeconds{5000};
  long maxEventQueueSize{0};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_OPTIONS_H_
