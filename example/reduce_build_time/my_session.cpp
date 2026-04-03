#include "my_session.h"

#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

MySession::MySession(const SessionOptions& sessionOptions, const SessionConfigs& sessionConfigs, EventHandler* eventHandler)
    : ccapiSessionPtr(new Session(sessionOptions, sessionConfigs, eventHandler)) {}

void MySession::subscribe(Subscription& subscription) { this->ccapiSessionPtr->subscribe(subscription); }

void MySession::stop() { this->ccapiSessionPtr->stop(); }

}  // namespace ccapi
