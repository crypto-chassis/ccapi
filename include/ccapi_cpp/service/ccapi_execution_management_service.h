#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "ccapi_cpp/service/ccapi_service.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_request.h"
#include "boost/shared_ptr.hpp"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_hmac.h"
namespace ccapi {
class ExecutionManagementService : public Service {
 public:
  enum class JsonDataType {
    STRING,
    INTEGER,
    BOOLEAN
  };
  ExecutionManagementService(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
  : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
  }
  virtual ~ExecutionManagementService() {
  }
  void stop() override {}
  void subscribe(const std::vector<Subscription>& subscriptionList) override {}

 protected:
  void setupCredential(std::vector<std::string> nameList) {
    for (const auto& x : nameList) {
      if (this->sessionConfigs.getCredential().find(x) != this->sessionConfigs.getCredential().end()) {
        this->credentialDefault.insert(std::make_pair(x, this->sessionConfigs.getCredential().at(x)));
      } else if (!UtilSystem::getEnvAsString(x).empty()) {
        this->credentialDefault.insert(std::make_pair(x, UtilSystem::getEnvAsString(x)));
      }
    }
  }
  std::string convertOrderStatus(const std::string& status) {
    return this->orderStatusOpenSet.find(status) != this->orderStatusOpenSet.end() ? CCAPI_EM_ORDER_STATUS_OPEN
    : CCAPI_EM_ORDER_STATUS_CLOSED;
  }
  virtual std::vector<Message> convertTextMessageToMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_DEBUG("textMessage = " + textMessage);
    rj::Document document;
    document.Parse(textMessage.c_str());
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({request.getCorrelationId()});
    std::vector<Element> elementList;
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        message.setType(Message::Type::CREATE_ORDER);
      }
      break;
      case Request::Operation::CANCEL_ORDER:
      {
        message.setType(Message::Type::CANCEL_ORDER);
      }
      break;
      case Request::Operation::GET_ORDER:
      {
        message.setType(Message::Type::GET_ORDER);
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        message.setType(Message::Type::GET_OPEN_ORDERS);
      }
      break;
      case Request::Operation::CANCEL_OPEN_ORDERS:
      {
        message.setType(Message::Type::CANCEL_OPEN_ORDERS);
      }
      break;
      default:
      CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    message.setElementList(this->extractOrderInfo(operation, document));
    std::vector<Message> messageList;
    messageList.push_back(std::move(message));
    return messageList;
  }
  void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    const std::vector<Message>& messageList = this->convertTextMessageToMessage(request, textMessage, timeReceived);
    Event event;
    event.setType(Event::Type::RESPONSE);
    event.addMessages(messageList);
    this->eventHandler(event);
  }
  virtual Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) {
    Element element;
    for (const auto& y : extractionFieldNameMap) {
      auto it = x.FindMember(y.second.first.c_str());
      if (it != x.MemberEnd()) {
        std::string value = y.second.second == JsonDataType::STRING ? it->value.GetString() :
          y.second.second == JsonDataType::INTEGER ? std::to_string(it->value.GetInt64()) :
          y.second.second == JsonDataType::BOOLEAN ? std::to_string(static_cast<int>(it->value.GetBool())) :
          "null";
        if (y.first == CCAPI_EM_ORDER_INSTRUMENT) {
          value = this->convertRestSymbolIdToInstrument(value);
        } else if (y.first == CCAPI_EM_ORDER_STATUS) {
          value = this->convertOrderStatus(value);
        } else if (y.first == CCAPI_EM_ORDER_SIDE) {
          value = UtilString::toLower(value).rfind("buy", 0) == 0 ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL;
        }
        element.insert(y.first, value);
      }
    }
    return element;
  }
  virtual std::vector<Element> extractOrderInfo(const Request::Operation operation, const rj::Document& document) = 0;
  std::string apiKeyName;
  std::string apiSecretName;
  std::string createOrderTarget;
  std::string cancelOrderTarget;
  std::string getOrderTarget;
  std::string getOpenOrdersTarget;
  std::string cancelOpenOrdersTarget;
  std::set<std::string> orderStatusOpenSet;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
