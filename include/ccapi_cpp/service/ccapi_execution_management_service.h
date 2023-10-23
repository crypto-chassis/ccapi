#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#include <sys/stat.h>

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
/**
 * The ExecutionManagementService class inherits from the Service class and provides implemenations more specific to execution management such as order
 * submission, order cancellation, etc..
 */
class ExecutionManagementService : public Service {
 public:
  enum class JsonDataType {
    STRING,
    INTEGER,
    BOOLEAN,
    // DOUBLE, shouldn't be needed because double in a json response needs to parsed as string to preserve its precision
  };
  ExecutionManagementService(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
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
  void subscribe(std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrlWs = " + this->baseUrlWs);
    if (this->shouldContinue.load()) {
      for (auto& subscription : subscriptionList) {
        boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<ExecutionManagementService>(), subscription]() mutable {
          auto now = UtilTime::now();
          subscription.setTimeSent(now);
          auto credential = subscription.getCredential();
          if (credential.empty()) {
            credential = that->credentialDefault;
          }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          WsConnection wsConnection(that->baseUrlWs, "", {subscription}, credential);
          that->prepareConnect(wsConnection);
#else
                                std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> streamPtr(nullptr);
                                try {
                                  streamPtr = that->createWsStream(that->serviceContextPtr->ioContextPtr, that->serviceContextPtr->sslContextPtr);
                                } catch (const beast::error_code& ec) {
                                  CCAPI_LOGGER_TRACE("fail");
                                  that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "create stream", {subscription.getCorrelationId()});
                                  return;
                                }
                                std::shared_ptr<WsConnection> wsConnectionPtr(new WsConnection(that->baseUrlWs, "", {subscription}, credential, streamPtr));
                                CCAPI_LOGGER_WARN("about to subscribe with new wsConnectionPtr " + toString(*wsConnectionPtr));
                                that->prepareConnect(wsConnectionPtr);
#endif
        });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  static std::map<std::string, std::string> convertHeaderStringToMap(const std::string& input) {
    std::map<std::string, std::string> output;
    if (!input.empty()) {
      for (const auto& x : UtilString::split(input, "\r\n")) {
        auto y = UtilString::split(x, ':');
        output.insert(std::make_pair(UtilString::trim(y.at(0)), UtilString::trim(y.at(1))));
      }
    }
    return output;
  }
  static std::string convertHeaderMapToString(const std::map<std::string, std::string>& input) {
    std::string output;
    int i = 0;
    for (const auto& x : input) {
      output += x.first;
      output += ":";
      output += x.second;
      if (i < input.size() - 1) {
        output += "\r\n";
      }
      ++i;
    }
    return output;
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  virtual std::vector<Message> convertTextMessageToMessageRest(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_DEBUG("textMessage = " + textMessage);
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({request.getCorrelationId()});
    std::vector<Element> elementList;
    Request::Operation operation = request.getOperation();
    message.setType(this->requestOperationToMessageTypeMap.at(operation));
    auto castedOperation = static_cast<int>(operation);
    if (castedOperation >= CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ORDER &&
        castedOperation < CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ACCOUNT) {
      this->extractOrderInfoFromRequest(elementList, request, operation, document);
      message.setElementList(elementList);
    } else if (castedOperation >= CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ACCOUNT) {
      this->extractAccountInfoFromRequest(elementList, request, operation, document);
      message.setElementList(elementList);
    }
    std::vector<Message> messageList;
    messageList.emplace_back(std::move(message));
    return messageList;
  }
  void processSuccessfulTextMessageRest(int statusCode, const Request& request, const std::string& textMessage, const TimePoint& timeReceived,
                                        Queue<Event>* eventQueuePtr) override {
    Event event;
    if (this->doesHttpBodyContainError(textMessage)) {
      event.setType(Event::Type::RESPONSE);
      Message message;
      message.setType(Message::Type::RESPONSE_ERROR);
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({request.getCorrelationId()});
      Element element;
      element.insert(CCAPI_HTTP_STATUS_CODE, "200");
      element.insert(CCAPI_ERROR_MESSAGE, UtilString::trim(textMessage));
      message.setElementList({element});
      event.setMessageList({message});
    } else {
      event.setType(Event::Type::RESPONSE);
      if (request.getOperation() == Request::Operation::GENERIC_PRIVATE_REQUEST) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(Message::Type::GENERIC_PRIVATE_REQUEST);
        Element element;
        element.insert(CCAPI_HTTP_STATUS_CODE, std::to_string(statusCode));
        element.insert(CCAPI_HTTP_BODY, textMessage);
        message.setElementList({element});
        const std::vector<std::string>& correlationIdList = {request.getCorrelationId()};
        CCAPI_LOGGER_TRACE("correlationIdList = " + toString(correlationIdList));
        message.setCorrelationIdList(correlationIdList);
        event.addMessages({message});
      } else {
        const std::vector<Message>& messageList = this->convertTextMessageToMessageRest(request, textMessage, timeReceived);
        event.addMessages(messageList);
      }
    }
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, eventQueuePtr);
    }
  }
  virtual void extractOrderInfo(Element& element, const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap,
                                const std::map<std::string, std::function<std::string(const std::string&)>> conversionMap = {}) {
    for (const auto& y : extractionFieldNameMap) {
      auto it = x.FindMember(y.second.first.c_str());
      if (it != x.MemberEnd() && !it->value.IsNull()) {
        std::string value = y.second.second == JsonDataType::STRING    ? it->value.GetString()
                            : y.second.second == JsonDataType::INTEGER ? std::string(it->value.GetString())
                            : y.second.second == JsonDataType::BOOLEAN ? std::to_string(static_cast<int>(it->value.GetBool()))
                                                                       : "null";
        if (y.first == CCAPI_EM_ORDER_SIDE) {
          value = UtilString::toLower(value).rfind("buy", 0) == 0 ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL;
        }
        auto it2 = conversionMap.find(y.first);
        if (it2 != conversionMap.end()) {
          value = it2->second(value);
        }
        element.insert(y.first, value);
      }
    }
  }
  virtual void convertRequestForWebsocketCustom(rj::Document& document, rj::Document::AllocatorType& allocator, const WsConnection& wsConnection,
                                                const Request& request, int wsRequestId, const TimePoint& now, const std::string& symbolId,
                                                const std::map<std::string, std::string>& credential) {
    auto errorMessage = "Websocket unimplemented operation " + Request::operationToString(request.getOperation()) + " for exchange " + request.getExchange();
    throw std::runtime_error(errorMessage);
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  virtual void logonToExchange(const WsConnection& wsConnection, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    CCAPI_LOGGER_INFO("about to logon to exchange");
    CCAPI_LOGGER_INFO("exchange is " + this->exchangeName);
    auto subscription = wsConnection.subscriptionList.at(0);
    std::vector<std::string> sendStringList = this->createSendStringListFromSubscription(wsConnection, subscription, now, credential);
    for (const auto& sendString : sendStringList) {
      CCAPI_LOGGER_INFO("sendString = " + sendString);
      ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
      this->send(wsConnectionPtr, sendString, ec);
#endif
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    }
  }
  void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto subscription = wsConnection.subscriptionList.at(0);
    this->onTextMessage(wsConnection, subscription, textMessage, timeReceived);
    this->onPongByMethod(PingPongMethod::WEBSOCKET_APPLICATION_LEVEL, hdl, textMessage, timeReceived, false);
  }
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    Service::onOpen(hdl);
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto correlationId = wsConnection.subscriptionList.at(0).getCorrelationId();
    this->wsConnectionByCorrelationIdMap.insert({correlationId, wsConnection});
    this->correlationIdByConnectionIdMap.insert({wsConnection.id, correlationId});
    auto credential = wsConnection.credential;
    this->logonToExchange(wsConnection, now, credential);
  }
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    if (this->correlationIdByConnectionIdMap.find(wsConnection.id) != this->correlationIdByConnectionIdMap.end()) {
      this->wsConnectionByCorrelationIdMap.erase(this->correlationIdByConnectionIdMap.at(wsConnection.id));
      this->correlationIdByConnectionIdMap.erase(wsConnection.id);
    }
    this->wsRequestIdByConnectionIdMap.erase(wsConnection.id);
    Service::onClose(hdl);
  }
  virtual void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage,
                             const TimePoint& timeReceived) {}
#else
  virtual void logonToExchange(std::shared_ptr<WsConnection> wsConnectionPtr, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    CCAPI_LOGGER_INFO("about to logon to exchange");
    CCAPI_LOGGER_INFO("exchange is " + this->exchangeName);
    WsConnection& wsConnection = *wsConnectionPtr;
    auto subscription = wsConnection.subscriptionList.at(0);
    std::vector<std::string> sendStringList = this->createSendStringListFromSubscription(wsConnection, subscription, now, credential);
    for (const auto& sendString : sendStringList) {
      CCAPI_LOGGER_INFO("sendString = " + sendString);
      ErrorCode ec;
      this->send(wsConnectionPtr, sendString, ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    }
  }
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived) override {
    auto subscription = wsConnectionPtr->subscriptionList.at(0);
    this->onTextMessage(wsConnectionPtr, subscription, textMessageView, timeReceived);
    this->onPongByMethod(PingPongMethod::WEBSOCKET_APPLICATION_LEVEL, wsConnectionPtr, timeReceived, false);
  }
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    Service::onOpen(wsConnectionPtr);
    auto now = UtilTime::now();
    WsConnection& wsConnection = *wsConnectionPtr;
    auto correlationId = wsConnection.subscriptionList.at(0).getCorrelationId();
    this->wsConnectionByCorrelationIdMap.insert({correlationId, wsConnectionPtr});
    this->correlationIdByConnectionIdMap.insert({wsConnection.id, correlationId});
    auto credential = wsConnection.credential;
    this->logonToExchange(wsConnectionPtr, now, credential);
  }
  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = *wsConnectionPtr;
    if (this->correlationIdByConnectionIdMap.find(wsConnection.id) != this->correlationIdByConnectionIdMap.end()) {
      this->wsConnectionByCorrelationIdMap.erase(this->correlationIdByConnectionIdMap.at(wsConnection.id));
      this->correlationIdByConnectionIdMap.erase(wsConnection.id);
    }
    this->wsRequestIdByConnectionIdMap.erase(wsConnection.id);
    Service::onClose(wsConnectionPtr, ec);
  }
  virtual void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                             const TimePoint& timeReceived) {}
#endif
  void convertRequestForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, const TimePoint& now,
                                                  const std::string& symbolId, const std::map<std::string, std::string>& credential) {
    const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
    auto methodString = mapGetWithDefault(param, std::string(CCAPI_HTTP_METHOD));
    CCAPI_LOGGER_TRACE("methodString = " + methodString);
    auto headerString = mapGetWithDefault(param, std::string(CCAPI_HTTP_HEADERS));
    CCAPI_LOGGER_TRACE("headerString = " + headerString);
    auto path = mapGetWithDefault(param, std::string(CCAPI_HTTP_PATH));
    CCAPI_LOGGER_TRACE("path = " + path);
    auto queryString = mapGetWithDefault(param, std::string(CCAPI_HTTP_QUERY_STRING));
    CCAPI_LOGGER_TRACE("queryString = " + queryString);
    auto body = mapGetWithDefault(param, std::string(CCAPI_HTTP_BODY));
    CCAPI_LOGGER_TRACE("body = " + body);
    this->signReqeustForRestGenericPrivateRequest(req, request, methodString, headerString, path, queryString, body, now, credential);
    req.method(this->convertHttpMethodStringToMethod(methodString));
    if (!headerString.empty()) {
      auto splitted = UtilString::split(headerString, "\r\n");
      for (const auto& x : splitted) {
        auto splitted_2 = UtilString::split(x, ':');
        req.set(UtilString::trim(splitted_2.at(0)), UtilString::trim(splitted_2.at(1)));
      }
    }
    auto target = path;
    if (!queryString.empty()) {
      target += "?" + queryString;
    }
    req.target(target);
    if (!body.empty()) {
      req.body() = body;
      req.prepare_payload();
    }
  }
  int64_t generateNonce(const TimePoint& now, int requestIndex = 0) {
    int64_t nonce = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() + requestIndex;
    return nonce;
  }
  void sendRequestByWebsocket(Request& request, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("now = " + toString(now));
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<ExecutionManagementService>(), request]() mutable {
      auto now = UtilTime::now();
      CCAPI_LOGGER_DEBUG("request = " + toString(request));
      CCAPI_LOGGER_TRACE("now = " + toString(now));
      request.setTimeSent(now);
      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
      auto& correlationId = request.getCorrelationId();
      auto it = that->wsConnectionByCorrelationIdMap.find(correlationId);
      if (it == that->wsConnectionByCorrelationIdMap.end()) {
        that->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "Websocket connection was not found", {correlationId});
        return;
      }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      auto& wsConnection = it->second;
#else
      auto wsConnectionPtr = it->second;
      auto& wsConnection = *wsConnectionPtr;
#endif
      CCAPI_LOGGER_TRACE("wsConnection = " + toString(wsConnection));
      auto instrument = request.getInstrument();
      auto symbolId = instrument;
      CCAPI_LOGGER_TRACE("symbolId = " + symbolId);
      ErrorCode ec;
      rj::Document document;
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      auto credential = request.getCredential();
      if (credential.empty()) {
        credential = that->credentialDefault;
      }
      that->convertRequestForWebsocket(document, allocator, wsConnection, request, ++that->wsRequestIdByConnectionIdMap[wsConnection.id], now, symbolId,
                                       credential);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      std::string sendString = stringBuffer.GetString();
      CCAPI_LOGGER_TRACE("sendString = " + sendString);
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      that->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
      that->send(wsConnectionPtr, sendString,  ec);
#endif
      if (ec) {
        that->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
      }
    });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const std::string& wsRequestId, const TimePoint& now,
                                     const std::string& symbolId, const std::map<std::string, std::string>& credential) {}
  virtual void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, const WsConnection& wsConnection,
                                          const Request& request, int wsRequestId, const TimePoint& now, const std::string& symbolId,
                                          const std::map<std::string, std::string>& credential) {}
  virtual void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                           const rj::Document& document) {}
  virtual void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                             const rj::Document& document) {}
  virtual std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription,
                                                                        const TimePoint& now, const std::map<std::string, std::string>& credential) {
    return {};
  }
  virtual void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                                       std::string& headerString, std::string& path, std::string& queryString, std::string& body,
                                                       const TimePoint& now, const std::map<std::string, std::string>& credential) {}
  std::string createOrderTarget;
  std::string cancelOrderTarget;
  std::string getOrderTarget;
  std::string getOpenOrdersTarget;
  std::string cancelOpenOrdersTarget;
  std::string getAccountsTarget;
  std::string getAccountBalancesTarget;
  std::string getAccountPositionsTarget;
  std::map<std::string, std::string> correlationIdByConnectionIdMap;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  std::map<std::string, WsConnection> wsConnectionByCorrelationIdMap;
#else
  std::map<std::string, std::shared_ptr<WsConnection>>
      wsConnectionByCorrelationIdMap;  // TODO(cryptochassis): for consistency, to be renamed to wsConnectionPtrByCorrelationIdMap
#endif
  std::map<std::string, int> wsRequestIdByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
