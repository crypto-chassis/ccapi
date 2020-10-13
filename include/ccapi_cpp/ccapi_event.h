#ifndef INCLUDE_CCAPI_CPP_CCAPI_EVENT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EVENT_H_
#include <vector>
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_message.h"
namespace ccapi {
class Event final {
 public:
  enum class Type {
    UNKNOWN,
    ADMIN,
    SESSION_STATUS,
    SUBSCRIPTION_STATUS,
    REQUEST_STATUS,
    RESPONSE,
    PARTIAL_RESPONSE,
    SUBSCRIPTION_DATA,
    SERVICE_STATUS,
    TIMEOUT,
    AUTHORIZATION_STATUS,
    RESOLUTION_STATUS,
    TOPIC_STATUS,
    TOKEN_STATUS,
    REQUEST
  };
  static std::string typeToString(Type type) {
    std::string output;
    switch (type) {
      case Type::UNKNOWN:
        output = "UNKNOWN";
        break;
      case Type::ADMIN:
        output = "ADMIN";
        break;
      case Type::SESSION_STATUS:
        output = "SESSION_STATUS";
        break;
      case Type::SUBSCRIPTION_STATUS:
        output = "SUBSCRIPTION_STATUS";
        break;
      case Type::REQUEST_STATUS:
        output = "REQUEST_STATUS";
        break;
      case Type::RESPONSE:
        output = "RESPONSE";
        break;
      case Type::PARTIAL_RESPONSE:
        output = "PARTIAL_RESPONSE";
        break;
      case Type::SUBSCRIPTION_DATA:
        output = "SUBSCRIPTION_DATA";
        break;
      case Type::SERVICE_STATUS:
        output = "SERVICE_STATUS";
        break;
      case Type::TIMEOUT:
        output = "TIMEOUT";
        break;
      case Type::AUTHORIZATION_STATUS:
        output = "AUTHORIZATION_STATUS";
        break;
      case Type::RESOLUTION_STATUS:
        output = "RESOLUTION_STATUS";
        break;
      case Type::TOPIC_STATUS:
        output = "TOPIC_STATUS";
        break;
      case Type::TOKEN_STATUS:
        output = "TOKEN_STATUS";
        break;
      case Type::REQUEST:
        output = "REQUEST";
        break;
      default:
        CCAPI_LOGGER_FATAL("");
    }
    return output;
  }
  std::string toString() const {
    std::string output = "Event [type = " + typeToString(type) + ", messageList = " + ccapi::toString(messageList)
        + "]";
    return output;
  }
  const std::vector<Message>& getMessageList() const {
    return messageList;
  }
  void addMessages(const std::vector<Message>& newMessageList) {
    this->messageList.insert(std::end(this->messageList), std::begin(newMessageList), std::end(newMessageList));
  }
  void setMessageList(const std::vector<Message>& messageList) {
    this->messageList = messageList;
  }
  Type getType() const {
    return type;
  }
  void setType(Type type) {
    this->type = type;
  }

 private:
  Type type{Type::UNKNOWN};
  std::vector<Message> messageList;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EVENT_H_
