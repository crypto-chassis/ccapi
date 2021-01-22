#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace ccapi {
class Service {
 public:
  typedef wspp::lib::shared_ptr<ServiceContext> ServiceContextPtr;
  typedef wspp::lib::error_code ErrorCode;
  Service(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      :
        eventHandler(eventHandler),
        sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs), serviceContextPtr(serviceContextPtr) {
  }
  virtual ~Service() {
  }
  void setEventHandler(const std::function<void(Event& event)>& eventHandler) {
    this->eventHandler = eventHandler;
  }
  virtual void stop() = 0;
  virtual void subscribe(const std::vector<Subscription>& subscriptionList) = 0;
  virtual std::shared_ptr<std::future<void> > sendRequest(const Request& request, const bool useFuture, const TimePoint& now) {
    return std::shared_ptr<std::future<void> >(nullptr);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::string& errorMessage) {
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    Event event;
    event.setType(eventType);
    Message message;
    auto now = std::chrono::system_clock::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(messageType);
    Element element;
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const ErrorCode& ec, const std::string& what) {
    this->onError(eventType, messageType, what + ": " + ec.message() + ", category: " + ec.category().name());
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::exception& e) {
    this->onError(eventType, messageType, e.what());
  }
  void onResponseError(int statusCode, const std::string& errorMessage) {
    std::string statusCodeStr = std::to_string(statusCode);
    CCAPI_LOGGER_ERROR("statusCode = " + statusCodeStr + ", errorMessage = " + errorMessage);
    Event event;
    event.setType(Event::Type::REQUEST_STATUS);
    Message message;
    auto now = std::chrono::system_clock::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(Message::Type::RESPONSE_ERROR);
    Element element;
    element.insert(CCAPI_HTTP_STATUS_CODE, statusCodeStr);
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
  }

 protected:
  std::string name;
  std::string baseUrl;
  std::string baseUrlRest;
  std::function<void(Event& event)> eventHandler;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  ServiceContextPtr serviceContextPtr;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
