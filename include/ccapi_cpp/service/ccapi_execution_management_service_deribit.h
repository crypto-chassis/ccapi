#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_DERIBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_DERIBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceDeribit : public ExecutionManagementService {
 public:
  ExecutionManagementServiceDeribit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                    ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_DERIBIT;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/api/v2";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    //     try {
    //       this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->clientIdName = CCAPI_DERIBIT_CLIENT_ID;
    this->clientSecretName = CCAPI_DERIBIT_CLIENT_SECRET;
    this->setupCredential({this->clientIdName, this->clientSecretName});
    this->restTarget = "/api/v2";
    this->createOrderBuyTarget = "/private/buy";
    this->createOrderSellTarget = "/private/sell";
    this->cancelOrderTarget = "/private/cancel";
    this->getOrderTarget = "/private/get_order_state";
    this->getOpenOrdersTarget = "/private/get_open_orders_by_instrument";
    this->cancelOpenOrdersTarget = "/private/cancel_all_by_instrument";
    this->getAccountBalancesTarget = "/private/get_account_summary";
    this->getAccountPositionsTarget = "/private/get_positions";
  }
  virtual ~ExecutionManagementServiceDeribit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void onOpen(wspp::connection_hdl hdl) override {
    ExecutionManagementService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    auto now = UtilTime::now();
    this->appendParam(document, allocator, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), "public/set_heartbeat",
                      {
                          {"interval", "10"},
                      });
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string msg = stringBuffer.GetString();
    ErrorCode ec;
    this->send(hdl, msg, wspp::frame::opcode::text, ec);
    if (ec) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
    }
  }
  void onClose(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->subscriptionJsonrpcIdSetByConnectionIdMap.erase(wsConnection.id);
    this->authorizationJsonrpcIdSetByConnectionIdMap.erase(wsConnection.id);
    ExecutionManagementService::onClose(hdl);
  }
#else
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    ExecutionManagementService::onOpen(wsConnectionPtr);
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    auto now = UtilTime::now();
    this->appendParam(document, allocator, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), "public/set_heartbeat",
                      {
                          {"interval", "10"},
                      });
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string msg = stringBuffer.GetString();
    ErrorCode ec;
    this->send(wsConnectionPtr, msg, ec);
    if (ec) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
    }
  }
  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    this->subscriptionJsonrpcIdSetByConnectionIdMap.erase(wsConnectionPtr->id);
    this->authorizationJsonrpcIdSetByConnectionIdMap.erase(wsConnectionPtr->id);
    ExecutionManagementService::onClose(wsConnectionPtr, ec);
  }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& httpMethodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    std::string authorizationHeader("deri-hmac-sha256 id=");
    authorizationHeader += mapGetWithDefault(credential, this->clientIdName);
    authorizationHeader += ",ts=";
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    authorizationHeader += ts;
    authorizationHeader += ",sig=";
    std::string nonce = ts;
    auto requestData = httpMethodString;
    requestData += "\n";
    std::string target = path;
    if (!queryString.empty()) {
      target += "?" + queryString;
    }
    requestData += target;
    requestData += "\n";
    requestData += body;
    requestData += "\n";
    std::string stringToSign = ts;
    stringToSign += "\n";
    stringToSign += nonce;
    stringToSign += "\n";
    stringToSign += requestData;
    auto clientSecret = mapGetWithDefault(credential, this->clientSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, clientSecret, stringToSign, true);
    authorizationHeader += signature;
    authorizationHeader += ",nonce=";
    authorizationHeader += nonce;
    headerString += "\r\nAuthorization:" + authorizationHeader;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    std::string authorizationHeader("deri-hmac-sha256 id=");
    authorizationHeader += mapGetWithDefault(credential, this->clientIdName);
    authorizationHeader += ",ts=";
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    authorizationHeader += ts;
    authorizationHeader += ",sig=";
    std::string nonce = ts;
    auto requestData = std::string(req.method_string());
    requestData += "\n";
    requestData += req.target().to_string();
    requestData += "\n";
    requestData += body;
    requestData += "\n";
    std::string stringToSign = ts;
    stringToSign += "\n";
    stringToSign += nonce;
    stringToSign += "\n";
    stringToSign += requestData;
    auto clientSecret = mapGetWithDefault(credential, this->clientSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, clientSecret, stringToSign, true);
    authorizationHeader += signature;
    authorizationHeader += ",nonce=";
    authorizationHeader += nonce;
    req.set(http::field::authorization, authorizationHeader);
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, int64_t requestId, const std::string& appMethod,
                   const std::map<std::string, std::string>& param, const std::map<std::string, std::string> standardizationMap = {}) {
    document.AddMember("jsonrpc", rj::Value("2.0").Move(), allocator);
    document.AddMember("id", rj::Value(requestId).Move(), allocator);
    document.AddMember("method", rj::Value(appMethod.c_str(), allocator).Move(), allocator);
    rj::Value params(rj::kObjectType);
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      if (key != CCAPI_EM_ORDER_SIDE) {
        auto value = kv.second;
        if (value != "null") {
          if (value == "true" || value == "false") {
            params.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
          } else {
            params.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
          }
        }
      }
    }
    document.AddMember("params", params, allocator);
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document["params"].AddMember("instrument_name", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void prepareReq(http::request<http::string_body>& req, const std::map<std::string, std::string>& param, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential, const std::string& jsonrpcMethod,
                  const std::map<std::string, std::string> standardizationMap = {}) {
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    req.set(beast::http::field::content_type, "application/json");
    req.method(http::verb::post);
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    this->appendParam(document, allocator, requestId, jsonrpcMethod, param, standardizationMap);
    if (!symbolId.empty()) {
      this->appendSymbolId(document, allocator, symbolId);
    }
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    auto body = stringBuffer.GetString();
    req.body() = body;
    req.prepare_payload();
    req.target(this->restTarget);
    this->signRequest(req, body, now, credential);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string side = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_SIDE));
        std::string jsonrpcMethod;
        if (side == CCAPI_EM_ORDER_SIDE_BUY) {
          jsonrpcMethod = this->createOrderBuyTarget;
        } else if (side == CCAPI_EM_ORDER_SIDE_SELL) {
          jsonrpcMethod = this->createOrderSellTarget;
        }
        this->prepareReq(req, param, now, symbolId, credential, jsonrpcMethod,
                         {
                             {CCAPI_EM_ORDER_QUANTITY, "amount"},
                             {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                         });
      } break;
      case Request::Operation::CANCEL_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->cancelOrderTarget,
                         {
                             {CCAPI_EM_ORDER_ID, "order_id"},
                         });
      } break;
      case Request::Operation::GET_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getOrderTarget,
                         {
                             {CCAPI_EM_ORDER_ID, "order_id"},
                         });
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getOpenOrdersTarget);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->cancelOpenOrdersTarget);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getAccountBalancesTarget,
                         {
                             {CCAPI_EM_ASSET, "currency"},
                         });
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getAccountPositionsTarget,
                         {
                             {CCAPI_EM_ASSET, "currency"},
                         });
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("direction", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("order_state", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instrument_name", JsonDataType::STRING)}};
    if (operation == Request::Operation::CREATE_ORDER) {
      Element element;
      this->extractOrderInfo(element, document["result"]["order"], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::GET_ORDER) {
      Element element;
      this->extractOrderInfo(element, document["result"], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : document["result"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        const rj::Value& result = document["result"];
        element.insert(CCAPI_EM_ASSET, result["currency"].GetString());
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, result["margin_balance"].GetString());
        elementList.emplace_back(std::move(element));
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["result"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["instrument_name"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["average_price"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  void extractOrderInfo(Element& element, const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap,
                        const std::map<std::string, std::function<std::string(const std::string&)>> conversionMap = {}) override {
    ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("filled_amount");
      auto it2 = x.FindMember("average_price");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(
            CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
            Decimal(UtilString::printDoubleScientific(std::stod(it1->value.GetString()) * (it2->value.IsNull() ? 0 : std::stod(it2->value.GetString()))))
                .toString());
      }
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    std::string clientId = mapGetWithDefault(credential, this->clientIdName);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    std::string nonce = ts;
    std::string stringToSign = ts + "\n" + nonce + "\n";
    auto clientSecret = mapGetWithDefault(credential, this->clientSecretName);
    std::string signature = Hmac::hmac(Hmac::ShaVersion::SHA256, clientSecret, stringToSign, true);
    this->appendParam(document, allocator, requestId, "public/auth",
                      {
                          {"grant_type", "client_signature"},
                          {"client_id", clientId},
                          {"timestamp", ts},
                          {"signature", signature},
                          {"nonce", nonce},
                      });
    this->authorizationJsonrpcIdSetByConnectionIdMap[wsConnection.id].insert(requestId);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto subscription = wsConnection.subscriptionList.at(0);
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    Event event = this->createEvent(wsConnection, hdl, subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
#else
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    std::string textMessage(textMessageView);
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    Event event = this->createEvent(wsConnectionPtr, subscription, textMessageView, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  Event createEvent(const WsConnection& wsConnection, wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage,
                    const rj::Document& document, const TimePoint& timeReceived) {
#else
  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    std::string textMessage(textMessageView);
    const WsConnection& wsConnection = *wsConnectionPtr;
#endif
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    auto it = document.FindMember("id");
    if (document.FindMember("error") != document.MemberEnd()) {
      const rj::Value& error = document["error"];
      if (error.FindMember("code") != error.MemberEnd() && std::string(error["code"].GetString()) != "0") {
        int64_t requestId = std::stoll(it->value.GetString());
        if (this->authorizationJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).find(requestId) !=
            this->authorizationJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).end()) {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          message.setType(Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          if (it != document.MemberEnd()) {
            this->authorizationJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).erase(std::stoll(it->value.GetString()));
          }
        }
      }
    } else {
      auto itResult = document.FindMember("result");
      if (itResult != document.MemberEnd()) {
        int64_t requestId = std::stoll(it->value.GetString());
        if (itResult->value.IsObject()) {
          if (this->authorizationJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).find(requestId) !=
              this->authorizationJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).end()) {
            rj::Document document;
            document.SetObject();
            rj::Document::AllocatorType& allocator = document.GetAllocator();
            document.AddMember("jsonrpc", rj::Value("2.0").Move(), allocator);
            document.AddMember("method", rj::Value("private/subscribe").Move(), allocator);
            int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(timeReceived.time_since_epoch()).count();
            document.AddMember("id", rj::Value(requestId).Move(), allocator);
            this->subscriptionJsonrpcIdSetByConnectionIdMap[wsConnection.id].insert(requestId);
            rj::Value channels(rj::kArrayType);
            for (const auto& instrument : instrumentSet) {
              for (const auto& field : fieldSet) {
                std::string exchangeSubscriptionId;
                if (field == CCAPI_EM_ORDER_UPDATE) {
                  exchangeSubscriptionId = "user.orders." + instrument + ".raw";
                } else if (field == CCAPI_EM_PRIVATE_TRADE) {
                  exchangeSubscriptionId = "user.trades." + instrument + ".raw";
                }
                channels.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
              }
            }
            rj::Value params(rj::kObjectType);
            params.AddMember("channels", channels, allocator);
            document.AddMember("params", params, allocator);
            rj::StringBuffer stringBuffer;
            rj::Writer<rj::StringBuffer> writer(stringBuffer);
            document.Accept(writer);
            std::string sendString = stringBuffer.GetString();
            ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
            this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
            this->send(wsConnectionPtr, sendString, ec);
#endif
            if (ec) {
              this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
            }
            if (it != document.MemberEnd()) {
              this->authorizationJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).erase(std::stoll(it->value.GetString()));
            }
          }
        } else if (itResult->value.IsArray()) {
          if (this->subscriptionJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).find(requestId) !=
              this->subscriptionJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).end()) {
            event.setType(Event::Type::SUBSCRIPTION_STATUS);
            message.setType(Message::Type::SUBSCRIPTION_STARTED);
            Element element;
            element.insert(CCAPI_INFO_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
            if (it != document.MemberEnd()) {
              this->subscriptionJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).erase(std::stoll(it->value.GetString()));
            }
          }
        }
      } else if (document.FindMember("method") != document.MemberEnd()) {
        std::string method = document["method"].GetString();
        if (method == "subscription") {
          const rj::Value& params = document["params"];
          std::string exchangeSubscriptionId = params["channel"].GetString();
          std::smatch match;
          std::string instrument;
          std::string field;
          if (std::regex_search(exchangeSubscriptionId, match, std::regex("user\\.orders\\.(.+)\\.raw"))) {
            instrument = match[1].str();
            field = CCAPI_EM_ORDER_UPDATE;
          } else if (std::regex_search(exchangeSubscriptionId, match, std::regex("user\\.trades\\.(.+)\\.raw"))) {
            instrument = match[1].str();
            field = CCAPI_EM_PRIVATE_TRADE;
          }
          if (instrumentSet.find(instrument) != instrumentSet.end()) {
            event.setType(Event::Type::SUBSCRIPTION_DATA);
            if (field == CCAPI_EM_PRIVATE_TRADE && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
              for (const auto& x : params["data"].GetArray()) {
                Message message;
                message.setTimeReceived(timeReceived);
                message.setCorrelationIdList({subscription.getCorrelationId()});
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["timestamp"].GetString()))));
                std::vector<Element> elementList;
                Element element;
                element.insert(CCAPI_TRADE_ID, std::string(x["trade_id"].GetString()));
                element.insert(CCAPI_SEQUENCE_NUMBER, std::string(x["trade_seq"].GetString()));
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["price"].GetString()));
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["amount"].GetString()));
                element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["direction"].GetString()) == "Buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                element.insert(CCAPI_IS_MAKER, std::string(x["liquidity"].GetString()) == "M" ? "1" : "0");
                element.insert(CCAPI_EM_ORDER_ID, std::string(x["order_id"].GetString()));
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fee"].GetString()));
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fee_currency"].GetString()));
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            } else if (field == CCAPI_EM_ORDER_UPDATE && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("direction", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("amount", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_amount", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("order_state", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
              };
              const rj::Value& x = params["data"];
              message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["last_update_timestamp"].GetString()))));
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              auto it1 = x.FindMember("filled_amount");
              auto it2 = x.FindMember("average_price");
              if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
                info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                            Decimal(UtilString::printDoubleScientific(std::stod(it1->value.GetString()) *
                                                                      (it2->value.IsNull() ? 0 : std::stod(it2->value.GetString()))))
                                .toString());
              }
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        } else if (method == "heartbeat") {
          std::string type = document["params"]["type"].GetString();
          if (type == "test_request") {
            rj::Document document;
            document.SetObject();
            rj::Document::AllocatorType& allocator = document.GetAllocator();
            auto now = UtilTime::now();
            this->appendParam(document, allocator, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), "/public/test", {});
            rj::StringBuffer stringBuffer;
            rj::Writer<rj::StringBuffer> writer(stringBuffer);
            document.Accept(writer);
            std::string msg = stringBuffer.GetString();
            ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
            this->send(wsConnection.hdl, msg, wspp::frame::opcode::text, ec);
#else
            this->send(wsConnectionPtr, msg, ec);
#endif
            if (ec) {
              this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
            }
          }
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::map<std::string, std::set<int64_t>> subscriptionJsonrpcIdSetByConnectionIdMap;
  std::map<std::string, std::set<int64_t>> authorizationJsonrpcIdSetByConnectionIdMap;
  std::string restTarget;
  std::string clientIdName;
  std::string clientSecretName;
  std::string createOrderBuyTarget;
  std::string createOrderSellTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_DERIBIT_H_
