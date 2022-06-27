#ifndef INCLUDE_CCAPI_CPP_CCAPI_EVENT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EVENT_H_
#include <vector>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_message.h"
namespace ccapi {
/**
** A single event resulting from a subscription or a request. Event objects are created by the API and passed to the application either through a registered
*EventHandler or EventQueue. Event objects contain Message objects which can be accessed using the getMessageList() function. The Event object is a handle to an
*event. The event is the basic unit of work provided to applications. Each Event object consists of an Type attribute and zero or more Message objects.
*/

class Event CCAPI_FINAL {
 public:
  enum class Type {
    UNKNOWN,
    SESSION_STATUS,
    SUBSCRIPTION_STATUS,
    REQUEST_STATUS,
    RESPONSE,
    SUBSCRIPTION_DATA,
    AUTHORIZATION_STATUS,
    FIX,
    FIX_STATUS,
  };
  static std::string typeToString(Type type) {
    std::string output;
    switch (type) {
      case Type::UNKNOWN:
        output = "UNKNOWN";
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
      case Type::SUBSCRIPTION_DATA:
        output = "SUBSCRIPTION_DATA";
        break;
      case Type::AUTHORIZATION_STATUS:
        output = "AUTHORIZATION_STATUS";
        break;
      case Type::FIX:
        output = "FIX";
        break;
      case Type::FIX_STATUS:
        output = "FIX_STATUS";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  std::string toString() const {
    std::string output = "Event [type = " + typeToString(type) + ", messageList = " + ccapi::toString(messageList) + "]";
    return output;
  }
  std::string toStringPretty(const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) const {
    std::string sl(leftToIndent, ' ');
    std::string ss(leftToIndent + space, ' ');
    std::string output = (indentFirstLine ? sl : "") + "Event [\n" + ss + "type = " + typeToString(type) + ",\n" + ss +
                         "messageList = " + ccapi::toStringPretty(messageList, space, leftToIndent + space, false) + "\n" + sl + "]";
    return output;
  }
  const std::vector<Message>& getMessageList() const { return messageList; }
  void addMessages(const std::vector<Message>& newMessageList) {
    this->messageList.insert(std::end(this->messageList), std::begin(newMessageList), std::end(newMessageList));
  }
  void addMessages(std::vector<Message>& newMessageList) {
    if (this->messageList.empty()) {
      this->messageList = std::move(newMessageList);
    } else {
      this->messageList.reserve(this->messageList.size() + newMessageList.size());
      std::move(std::begin(newMessageList), std::end(newMessageList), std::back_inserter(this->messageList));
    }
  }
  void addMessage(const Message& newMessage) { this->messageList.push_back(newMessage); }
  void addMessage(Message& newMessage) { this->messageList.emplace_back(std::move(newMessage)); }
  void setMessageList(const std::vector<Message>& messageList) { this->messageList = messageList; }
  void setMessageList(std::vector<Message>& messageList) { this->messageList = std::move(messageList); }
  Type getType() const { return type; }
  void setType(Type type) { this->type = type; }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  Type type{Type::UNKNOWN};
  std::vector<Message> messageList;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EVENT_H_
