#ifndef EXAMPLE_SRC_REDUCE_BUILD_TIME_MY_SESSION_H_
#define EXAMPLE_SRC_REDUCE_BUILD_TIME_MY_SESSION_H_

#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_subscription.h"

namespace ccapi {

class EventHandler;
class Session;  // forward declaration instead of including full header!

class MySession {
 public:
  explicit MySession(const SessionOptions& sessionOptions, const SessionConfigs& sessionConfigs, EventHandler* eventHandler);
  void subscribe(Subscription& subscription);
  void stop();

 private:
  Session* ccapiSessionPtr{nullptr};
};

}  // namespace ccapi

#endif  // EXAMPLE_SRC_REDUCE_BUILD_TIME_MY_SESSION_H_
