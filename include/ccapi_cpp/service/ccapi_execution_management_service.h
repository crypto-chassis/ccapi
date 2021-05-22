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
    DOUBLE,
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
    };
  }
  virtual ~ExecutionManagementService() {}
  // void stop() override { Service::stop(); }
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
                                      : y.second.second == JsonDataType::BOOLEAN
                                            ? std::to_string(static_cast<int>(it->value.GetBool()))
                                            : y.second.second == JsonDataType::DOUBLE ? std::to_string(it->value.GetDouble()) : "null";
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
  void prepareConnect(WsConnection& wsConnection) {
    if (this->exchangeName == CCAPI_EXCHANGE_NAME_KUCOIN) {
      // kucoin needs special treatment
    } else {
      this->connect(wsConnection);
    }
  }
  void connect(WsConnection& wsConnection) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    wsConnection.status = WsConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("connection initialization on dummy id " + wsConnection.id);
    std::string url = wsConnection.url;
    this->serviceContextPtr->tlsClientPtr->set_tls_init_handler(
        std::bind(&ExecutionManagementService::onTlsInit, shared_from_base<ExecutionManagementService>(), std::placeholders::_1));
    CCAPI_LOGGER_DEBUG("endpoint tls init handler set");
    ErrorCode ec;
    TlsClient::connection_ptr con = this->serviceContextPtr->tlsClientPtr->get_connection(url, ec);
    wsConnection.id = this->connectionAddressToString(con);
    CCAPI_LOGGER_DEBUG("connection initialization on actual id " + wsConnection.id);
    if (ec) {
      CCAPI_LOGGER_FATAL("connection initialization error: " + ec.message());
    }
    this->wsConnectionByIdMap.insert(std::pair<std::string, WsConnection>(wsConnection.id, wsConnection));
    CCAPI_LOGGER_DEBUG("this->wsConnectionByIdMap = " + toString(this->wsConnectionByIdMap));
    // this->instrumentGroupByWsConnectionIdMap.insert(std::pair<std::string, std::string>(wsConnection.id, wsConnection.instrumentGroup));
    // CCAPI_LOGGER_DEBUG("this->instrumentGroupByWsConnectionIdMap = " + toString(this->instrumentGroupByWsConnectionIdMap));
    con->set_open_handler(std::bind(&ExecutionManagementService::onOpen, shared_from_base<ExecutionManagementService>(), std::placeholders::_1));
    con->set_fail_handler(std::bind(&ExecutionManagementService::onFail, shared_from_base<ExecutionManagementService>(), std::placeholders::_1));
    con->set_close_handler(std::bind(&ExecutionManagementService::onClose, shared_from_base<ExecutionManagementService>(), std::placeholders::_1));
    con->set_message_handler(
        std::bind(&ExecutionManagementService::onMessage, shared_from_base<ExecutionManagementService>(), std::placeholders::_1, std::placeholders::_2));
    if (this->sessionOptions.enableCheckPingPongWebsocketProtocolLevel) {
      con->set_pong_handler(
          std::bind(&ExecutionManagementService::onPong, shared_from_base<ExecutionManagementService>(), std::placeholders::_1, std::placeholders::_2));
    }
    con->set_ping_handler(
        std::bind(&ExecutionManagementService::onPing, shared_from_base<ExecutionManagementService>(), std::placeholders::_1, std::placeholders::_2));
    this->serviceContextPtr->tlsClientPtr->connect(con);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onOpen(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
    wsConnection.hdl = hdl;
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " established");
    auto urlBase = UtilString::split(wsConnection.url, "?").at(0);
    this->connectNumRetryOnFailByConnectionUrlMap[urlBase] = 0;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    std::vector<std::string> correlationIdList;
    for (const auto& subscription : wsConnection.subscriptionList) {
      correlationIdList.push_back(subscription.getCorrelationId());
    }
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnection.id);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
    if (this->enableCheckPingPongWebsocketProtocolLevel) {
      this->setPingPongTimer(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnection, hdl,
                             [that = shared_from_base<ExecutionManagementService>()](wspp::connection_hdl hdl, ErrorCode& ec) { that->ping(hdl, "", ec); });
    }
    if (this->enableCheckPingPongWebsocketApplicationLevel) {
      this->setPingPongTimer(
          PingPongMethod::WEBSOCKET_APPLICATION_LEVEL, wsConnection, hdl,
          [that = shared_from_base<ExecutionManagementService>()](wspp::connection_hdl hdl, ErrorCode& ec) { that->pingOnApplicationLevel(hdl, ec); });
    }
    // auto instrumentGroup = wsConnection.instrumentGroup;
    // for (const auto& subscription : wsConnection.subscriptionList) {
    // auto instrument = subscription.getInstrument();
    // this->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
    // this->prepareSubscription(wsConnection, subscription);
    // }
    // CCAPI_LOGGER_INFO("about to subscribe to exchange");
    // this->subscribeToExchange(wsConnection);
    CCAPI_LOGGER_INFO("about to logon to exchange");
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->logonToExchange(wsConnection, now, credential);
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
  void onFail_(WsConnection& wsConnection) {
    wsConnection.status = WsConnection::Status::FAILED;
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "connection " + toString(wsConnection) + " has failed before opening");
    WsConnection thisWsConnection = wsConnection;
    this->wsConnectionByIdMap.erase(thisWsConnection.id);
    // this->instrumentGroupByWsConnectionIdMap.erase(thisWsConnection.id);
    auto urlBase = UtilString::split(thisWsConnection.url, "?").at(0);
    long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[urlBase], 6)));
    CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(thisWsConnection.id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(thisWsConnection.id)->cancel();
    }
    this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] = this->serviceContextPtr->tlsClientPtr->set_timer(
        seconds * 1000, [thisWsConnection, that = shared_from_base<ExecutionManagementService>(), urlBase](ErrorCode const& ec) {
          if (that->wsConnectionByIdMap.find(thisWsConnection.id) == that->wsConnectionByIdMap.end()) {
            if (ec) {
              CCAPI_LOGGER_ERROR("wsConnection = " + toString(thisWsConnection) + ", connect retry on fail timer error: " + ec.message());
              that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
            } else {
              CCAPI_LOGGER_INFO("about to retry");
              auto thatWsConnection = thisWsConnection;
              thatWsConnection.assignDummyId();
              that->prepareConnect(thatWsConnection);
              that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
            }
          }
        });
  }
  virtual void onFail(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->clearStates(wsConnection);
    this->onFail_(wsConnection);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void clearStates(WsConnection& wsConnection) {
    CCAPI_LOGGER_INFO("clear states for wsConnection " + toString(wsConnection));
    // this->fieldByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->optionMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->subscriptionListByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->correlationIdListByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.erase(wsConnection.id);
    // this->snapshotBidByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->snapshotAskByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->processedInitialTradeByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap.erase(wsConnection.id);
    this->lastPongTpByMethodByConnectionIdMap.erase(wsConnection.id);
    // this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // if (this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) != this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.end()) {
    //   for (const auto& x : this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
    //     for (const auto& y : x.second) {
    //       y.second->cancel();
    //     }
    //   }
    //   this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // }
    // this->openByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->highByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->lowByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    // this->closeByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->extraPropertyByConnectionIdMap.erase(wsConnection.id);
    if (this->pingTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pingTimerByMethodByConnectionIdMap.end()) {
      for (const auto& x : this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id)) {
        x.second->cancel();
      }
      this->pingTimerByMethodByConnectionIdMap.erase(wsConnection.id);
    }
    if (this->pongTimeOutTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pongTimeOutTimerByMethodByConnectionIdMap.end()) {
      for (const auto& x : this->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id)) {
        x.second->cancel();
      }
      this->pongTimeOutTimerByMethodByConnectionIdMap.erase(wsConnection.id);
    }
    auto urlBase = UtilString::split(wsConnection.url, "?").at(0);
    this->connectNumRetryOnFailByConnectionUrlMap.erase(urlBase);
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(wsConnection.id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      this->connectRetryOnFailTimerByConnectionIdMap.erase(wsConnection.id);
    }
    // this->orderBookChecksumByConnectionIdSymbolIdMap.erase(wsConnection.id);
  }
  virtual void onClose(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    TlsClient::connection_ptr con = this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(con);
    wsConnection.status = WsConnection::Status::CLOSED;
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " is closed");
    std::stringstream s;
    s << "close code: " << con->get_remote_close_code() << " (" << websocketpp::close::status::get_string(con->get_remote_close_code())
      << "), close reason: " << con->get_remote_close_reason();
    std::string reason = s.str();
    CCAPI_LOGGER_INFO("reason is " + reason);
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_DOWN);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnection.id);
    element.insert(CCAPI_REASON, reason);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " is closed");
    this->clearStates(wsConnection);
    WsConnection thisWsConnection = wsConnection;
    this->wsConnectionByIdMap.erase(wsConnection.id);
    // this->instrumentGroupByWsConnectionIdMap.erase(wsConnection.id);
    if (this->shouldContinue.load()) {
      thisWsConnection.assignDummyId();
      this->prepareConnect(thisWsConnection);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onMessage(wspp::connection_hdl hdl, TlsClient::message_ptr msg) {
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_DEBUG("received a message from connection " + toString(wsConnection));
    if (wsConnection.status != WsConnection::Status::OPEN && !this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id]) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }
    auto opcode = msg->get_opcode();
    CCAPI_LOGGER_DEBUG("opcode = " + toString(opcode));
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
      std::string textMessage = msg->get_payload();
      CCAPI_LOGGER_DEBUG("received a text message: " + textMessage);
      try {
        this->onTextMessage(hdl, textMessage, now);
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR("textMessage = " + textMessage);
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, e);
      }
    } else if (opcode == websocketpp::frame::opcode::binary) {
      // #if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)
      //       if (this->exchangeName == CCAPI_EXCHANGE_NAME_HUOBI || this->exchangeName == CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP ||
      //           this->exchangeName == CCAPI_EXCHANGE_NAME_OKEX) {
      //         std::string decompressed;
      //         std::string payload = msg->get_payload();
      //         try {
      //           ErrorCode ec = this->inflater.decompress(reinterpret_cast<const uint8_t*>(&payload[0]), payload.size(), decompressed);
      //           if (ec) {
      //             CCAPI_LOGGER_FATAL(ec.message());
      //           }
      //           CCAPI_LOGGER_DEBUG("decompressed = " + decompressed);
      //           this->onTextMessage(hdl, decompressed, now);
      //         } catch (const std::exception& e) {
      //           std::stringstream ss;
      //           ss << std::hex << std::setfill('0');
      //           for (int i = 0; i < payload.size(); ++i) {
      //             ss << std::setw(2) << static_cast<unsigned>(reinterpret_cast<const uint8_t*>(&payload[0])[i]);
      //           }
      //           CCAPI_LOGGER_ERROR("binaryMessage = " + ss.str());
      //           this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, e);
      //         }
      //         ErrorCode ec = this->inflater.inflate_reset();
      //         if (ec) {
      //           this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "decompress");
      //         }
      //       }
      // #endif
    }
  }
  void onPong(wspp::connection_hdl hdl, std::string payload) {
    auto now = UtilTime::now();
    this->onPongByMethod(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, hdl, payload, now);
  }
  void onPongByMethod(PingPongMethod method, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_TRACE(pingPongMethodToString(method) + ": received a pong from " + toString(wsConnection));
    this->lastPongTpByMethodByConnectionIdMap[wsConnection.id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  bool onPing(wspp::connection_hdl hdl, std::string payload) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_TRACE("received a ping from " + toString(wsConnection));
    CCAPI_LOGGER_FUNCTION_EXIT;
    return true;
  }
  // void close(WsConnection& wsConnection, wspp::connection_hdl hdl, wspp::close::status::value const code, std::string const& reason, ErrorCode& ec) {
  //   if (wsConnection.status == WsConnection::Status::CLOSING) {
  //     CCAPI_LOGGER_WARN("websocket connection is already in the state of closing");
  //     return;
  //   }
  //   wsConnection.status = WsConnection::Status::CLOSING;
  //   this->serviceContextPtr->tlsClientPtr->close(hdl, code, reason, ec);
  // }
  void send(wspp::connection_hdl hdl, std::string const& payload, wspp::frame::opcode::value op, ErrorCode& ec) {
    this->serviceContextPtr->tlsClientPtr->send(hdl, payload, op, ec);
  }
  void ping(wspp::connection_hdl hdl, std::string const& payload, ErrorCode& ec) { this->serviceContextPtr->tlsClientPtr->ping(hdl, payload, ec); }
  virtual void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) {}
  void setPingPongTimer(PingPongMethod method, WsConnection& wsConnection, wspp::connection_hdl hdl,
                        std::function<void(wspp::connection_hdl, ErrorCode&)> pingMethod) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliSeconds = this->pingIntervalMilliSecondsByMethodMap[method];
    auto pongTimeoutMilliSeconds = this->pongTimeoutMilliSecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliSeconds = " + toString(pingIntervalMilliSeconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliSeconds = " + toString(pongTimeoutMilliSeconds));
    if (pingIntervalMilliSeconds <= pongTimeoutMilliSeconds) {
      return;
    }
    if (wsConnection.status == WsConnection::Status::OPEN) {
      if (this->pingTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).find(method) != this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).at(method)->cancel();
      }
      this->pingTimerByMethodByConnectionIdMap[wsConnection.id][method] = this->serviceContextPtr->tlsClientPtr->set_timer(
          pingIntervalMilliSeconds - pongTimeoutMilliSeconds,
          [wsConnection, that = shared_from_base<ExecutionManagementService>(), hdl, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
            if (that->wsConnectionByIdMap.find(wsConnection.id) != that->wsConnectionByIdMap.end()) {
              if (ec) {
                CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", ping timer error: " + ec.message());
                that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
              } else {
                if (that->wsConnectionByIdMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                  ErrorCode ec;
                  pingMethod(hdl, ec);
                  if (ec) {
                    that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping");
                  }
                  if (pongTimeoutMilliSeconds <= 0) {
                    return;
                  }
                  if (that->pongTimeOutTimerByMethodByConnectionIdMap.find(wsConnection.id) != that->pongTimeOutTimerByMethodByConnectionIdMap.end() &&
                      that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id).find(method) !=
                          that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id).end()) {
                    that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id).at(method)->cancel();
                  }
                  that->pongTimeOutTimerByMethodByConnectionIdMap[wsConnection.id][method] = that->serviceContextPtr->tlsClientPtr->set_timer(
                      pongTimeoutMilliSeconds, [wsConnection, that, hdl, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
                        if (that->wsConnectionByIdMap.find(wsConnection.id) != that->wsConnectionByIdMap.end()) {
                          if (ec) {
                            CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", pong time out timer error: " + ec.message());
                            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                          } else {
                            if (that->wsConnectionByIdMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                              auto now = UtilTime::now();
                              if (that->lastPongTpByMethodByConnectionIdMap.find(wsConnection.id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                                  that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).find(method) !=
                                      that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).end() &&
                                  std::chrono::duration_cast<std::chrono::milliseconds>(
                                      now - that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).at(method))
                                          .count() >= pongTimeoutMilliSeconds) {
                                auto thisWsConnection = wsConnection;
                                ErrorCode ec;
                                that->close(thisWsConnection, hdl, websocketpp::close::status::normal, "pong timeout", ec);
                                if (ec) {
                                  that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
                                }
                                that->shouldProcessRemainingMessageOnClosingByConnectionIdMap[thisWsConnection.id] = true;
                              } else {
                                auto thisWsConnection = wsConnection;
                                that->setPingPongTimer(method, thisWsConnection, hdl, pingMethod);
                              }
                            }
                          }
                        }
                      });
                }
              }
            }
          });
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto subscription = wsConnection.subscriptionList.at(0);
    rj::Document document;
    document.Parse(textMessage.c_str());
    auto eventType = this->getEventType(document);
    Event event;
    event.setType(eventType);
    if (eventType == Event::Type::SUBSCRIPTION_STATUS) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      event.addMessages({message});
    } else if (eventType == Event::Type::SUBSCRIPTION_DATA) {
      const std::vector<Message>& messageList = this->convertDocumentToMessage(subscription, document, timeReceived);
      event.addMessages(messageList);
    }
    if (!event.getMessageList().empty()) {
      this->eventHandler(event);
    }
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
  virtual Event::Type getEventType(const rj::Document& document) { return Event::Type::UNKNOWN; }
  virtual std::vector<Message> convertDocumentToMessage(const Subscription& subscription, const rj::Document& document, const TimePoint& timeReceived) {
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
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
