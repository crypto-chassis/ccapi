#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "boost/shared_ptr.hpp"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/service/ccapi_service.h"
namespace ccapi {
class ExecutionManagementService : public Service {
 public:
  enum class JsonDataType {
    STRING,
    INTEGER,
    BOOLEAN,
    // DOUBLE, shouldn't be needed because double in a json response needs to parsed as string to preserve its precision
  };
  ExecutionManagementService(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             ServiceContextPtr serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->requestOperationToMessageTypeMap = {
        {Request::Operation::CREATE_ORDER, Message::Type::CREATE_ORDER},
        {Request::Operation::CANCEL_ORDER, Message::Type::CANCEL_ORDER},
        {Request::Operation::GET_ORDER, Message::Type::GET_ORDER},
        {Request::Operation::GET_OPEN_ORDERS, Message::Type::GET_OPEN_ORDERS},
        {Request::Operation::CANCEL_OPEN_ORDERS, Message::Type::CANCEL_OPEN_ORDERS},
        {Request::Operation::GET_ACCOUNTS, Message::Type::GET_ACCOUNTS},
        {Request::Operation::GET_ACCOUNT_BALANCES, Message::Type::GET_ACCOUNT_BALANCES},
        {Request::Operation::GET_ACCOUNT_POSITIONS, Message::Type::GET_ACCOUNT_POSITIONS},
    };
  }
  virtual ~ExecutionManagementService() {}
  // each subscription creates a unique websocket connection
  void subscribe(const std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrl = " + this->baseUrl);
    if (this->shouldContinue.load()) {
      for (const auto& subscription : subscriptionList) {
        wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [that = shared_from_base<ExecutionManagementService>(), subscription]() {
          WsConnection wsConnection(that->baseUrl, "", {subscription});
          that->prepareConnect(wsConnection);
        });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  virtual std::vector<Message> convertTextMessageToMessageRest(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_DEBUG("textMessage = " + textMessage);
    rj::Document document;
    document.Parse(textMessage.c_str());
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({request.getCorrelationId()});
    std::vector<Element> elementList;
    Request::Operation operation = request.getOperation();
    message.setType(this->requestOperationToMessageTypeMap.at(operation));
    auto castedOperation = static_cast<int>(operation);
    if (castedOperation >= Request::operationTypeExecutionManagementOrder && castedOperation < Request::operationTypeExecutionManagementAccount) {
      message.setElementList(this->extractOrderInfoFromRequest(request, operation, document));
    } else if (castedOperation >= Request::operationTypeExecutionManagementAccount) {
      message.setElementList(this->extractAccountInfoFromRequest(request, operation, document));
    }
    std::vector<Message> messageList;
    messageList.push_back(std::move(message));
    return messageList;
  }
  void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    const std::vector<Message>& messageList = this->convertTextMessageToMessageRest(request, textMessage, timeReceived);
    Event event;
    event.setType(Event::Type::RESPONSE);
    event.addMessages(messageList);
    this->eventHandler(event);
  }
  virtual Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) {
    Element element;
    for (const auto& y : extractionFieldNameMap) {
      auto it = x.FindMember(y.second.first.c_str());
      if (it != x.MemberEnd() && !it->value.IsNull()) {
        std::string value = y.second.second == JsonDataType::STRING
                                ? it->value.GetString()
                                : y.second.second == JsonDataType::INTEGER
                                      ? std::to_string(it->value.GetInt64())
                                      : y.second.second == JsonDataType::BOOLEAN ? std::to_string(static_cast<int>(it->value.GetBool())) : "null";
        if (y.first == CCAPI_EM_ORDER_INSTRUMENT) {
          value = this->convertRestSymbolIdToInstrument(value);
        } else if (y.first == CCAPI_EM_ORDER_SIDE) {
          value = UtilString::toLower(value).rfind("buy", 0) == 0 ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL;
        }
        element.insert(y.first, value);
      }
    }
    return element;
  }
  virtual void logonToExchange(const WsConnection& wsConnection, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    CCAPI_LOGGER_INFO("exchange is " + this->exchangeName);
    auto subscription = wsConnection.subscriptionList.at(0);
    std::vector<std::string> sendStringList = this->createSendStringListFromSubscription(subscription, now, credential);
    for (const auto& sendString : sendStringList) {
      CCAPI_LOGGER_INFO("sendString = " + sendString);
      ErrorCode ec;
      this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    }
  }
  void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto subscription = wsConnection.subscriptionList.at(0);
    rj::Document document;
    document.Parse(textMessage.c_str());
    auto event = this->createEvent(subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event);
    }
  }
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    Service::onOpen(hdl);
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_INFO("about to logon to exchange");
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->logonToExchange(wsConnection, now, credential);
  }
  virtual Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const TimePoint& timeReceived) {
    return {};
  }
  virtual std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) {
    return {};
  }
  virtual std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) {
    return {};
  }
  virtual std::vector<std::string> createSendStringListFromSubscription(const Subscription& subscription, const TimePoint& now,
                                                                        const std::map<std::string, std::string>& credential) {
    return {};
  }
  std::string apiKeyName;
  std::string apiSecretName;
  std::string createOrderTarget;
  std::string cancelOrderTarget;
  std::string getOrderTarget;
  std::string getOpenOrdersTarget;
  std::string cancelOpenOrdersTarget;
  std::string getAccountsTarget;
  std::string getAccountBalancesTarget;
  std::string getAccountPositionsTarget;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
