#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_KUCOIN) || defined(CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceKucoinBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceKucoinBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                       ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~ExecutionManagementServiceKucoinBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"200000\"")); }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void onOpen(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
  }
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"id\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"type\":\"ping\"}", wspp::frame::opcode::text, ec);
  }
  void prepareConnect(WsConnection& wsConnection) override {
    auto now = UtilTime::now();
    http::request<http::string_body> req;
    req.set(http::field::host, this->hostRest);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    req.target("/api/v1/bullet-private");
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->prepareReq(req, now, credential);
    this->signRequest(req, "", credential);
    this->sendRequest(
        req,
        [wsConnection, that = shared_from_base<ExecutionManagementServiceKucoinBase>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<ExecutionManagementServiceKucoinBase>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              const rj::Value& instanceServer = document["data"]["instanceServers"][0];
              urlWebsocketBase += std::string(instanceServer["endpoint"].GetString());
              urlWebsocketBase += "?token=";
              urlWebsocketBase += std::string(document["data"]["token"].GetString());
              thisWsConnection.url = urlWebsocketBase;
              that->connect(thisWsConnection);
              that->extraPropertyByConnectionIdMap[thisWsConnection.id].insert({
                  {"pingInterval", std::string(instanceServer["pingInterval"].GetString())},
                  {"pingTimeout", std::string(instanceServer["pingTimeout"].GetString())},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(thisWsConnection);
        },
        this->sessionOptions.httpRequestTimeoutMilliSeconds);
  }
#else
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override { wsConnectionPtr->status = WsConnection::Status::OPEN; }
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr, "{\"id\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"type\":\"ping\"}", ec);
  }
  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    auto now = UtilTime::now();
    http::request<http::string_body> req;
    req.set(http::field::host, this->hostRest);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    req.target("/api/v1/bullet-private");
    auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->prepareReq(req, now, credential);
    this->signRequest(req, "", credential);
    this->sendRequest(
        req,
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceKucoinBase>()](const beast::error_code& ec) { that->onFail_(wsConnectionPtr); },
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceKucoinBase>()](const http::response<http::string_body>& res) {
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              const rj::Value& instanceServer = document["data"]["instanceServers"][0];
              urlWebsocketBase += std::string(instanceServer["endpoint"].GetString());
              urlWebsocketBase += "?token=";
              urlWebsocketBase += std::string(document["data"]["token"].GetString());
              wsConnectionPtr->url = urlWebsocketBase;
              that->connect(wsConnectionPtr);
              that->extraPropertyByConnectionIdMap[wsConnectionPtr->id].insert({
                  {"pingInterval", std::string(instanceServer["pingInterval"].GetString())},
                  {"pingTimeout", std::string(instanceServer["pingTimeout"].GetString())},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(wsConnectionPtr);
        },
        this->sessionOptions.httpRequestTimeoutMilliSeconds);
  }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("KC-API-TIMESTAMP").to_string();
    preSignedText += methodString;
    auto target = path;
    if (!queryString.empty()) {
      target += queryString;
    }
    preSignedText += target;
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "KC-API-SIGN:" + signature;
  }
  void signRequestPartner(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    req.set("KC-API-PARTNER", CCAPI_KUCOIN_API_PARTNER_PLATFORM_ID);
    auto preSignedText = req.base().at("KC-API-TIMESTAMP").to_string();
    preSignedText += CCAPI_KUCOIN_API_PARTNER_PLATFORM_ID;
    preSignedText += req.base().at("KC-API-KEY").to_string();
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, CCAPI_KUCOIN_API_PARTNER_PRIVATE_KEY, preSignedText));
    req.set("KC-API-PARTNER-SIGN", signature);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("KC-API-TIMESTAMP").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("KC-API-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void signApiPassphrase(http::request<http::string_body>& req, const std::string& apiPassphrase, const std::string& apiSecret) {
    req.set("KC-API-PASSPHRASE", UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, apiPassphrase)));
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOid"},
                       {CCAPI_EM_ORDER_LEVERAGE, "leverage"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      if (key == "cancelAfter") {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(static_cast<int64_t>(std::stoll(value))), allocator);
      } else if (value == "true" || value == "false") {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
      } else {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
      }
    }
  }
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("KC-API-KEY", apiKey);
    req.set("KC-API-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    // auto apiKeyVersion = mapGetWithDefault(credential, this->apiKeyVersionName);
    req.set("KC-API-KEY-VERSION", "2");
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    this->signApiPassphrase(req, apiPassphrase, apiSecret);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, now, credential);
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN || request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN
                       ? this->createOrderMarginTarget
                       : this->createOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequestPartner(req, body, credential);
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        bool useOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = useOrderId                                            ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client-order/" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        auto target =
            useOrderId ? std::regex_replace(this->cancelOrderTarget, std::regex("<id>"), id) : std::regex_replace("/api/v1/order/<id>", std::regex("<id>"), id);
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        bool useOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = useOrderId                                            ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        auto target = std::regex_replace(useOrderId ? this->getOrderTarget : this->getOrderByClientOrderIdTarget, std::regex("<id>"), Url::urlEncode(id));
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        auto target = this->getOpenOrdersTarget;
        target += "?status=active";
        target += std::string("&tradeType=") + (request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN      ? "MARGIN_TRADE"
                                                : request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN ? "MARGIN_ISOLATED_TRADE"
                                                                                                                  : "TRADE");
        if (!symbolId.empty()) {
          target += "&symbol=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        auto target = this->cancelOpenOrdersTarget;
        target += std::string("?tradeType=") + (request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN      ? "MARGIN_TRADE"
                                                : request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN ? "MARGIN_ISOLATED_TRADE"
                                                                                                                  : "TRADE");
        if (!symbolId.empty()) {
          target += "&symbol=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::get);
        std::string target = this->getAccountsTarget;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        if (!param.empty()) {
          std::string queryString;
          this->appendParam(queryString, param,
                            {
                                {CCAPI_EM_ASSET, "currency"},
                                {CCAPI_EM_ACCOUNT_TYPE, "type"},
                            });
          target += "?" + queryString;
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto target = this->getAccountBalancesTarget;
        if (this->isDerivatives) {
          if (!param.empty()) {
            std::string queryString;
            this->appendParam(queryString, param,
                              {
                                  {CCAPI_EM_ASSET, "currency"},
                              });
            target += "?" + queryString;
          }
        } else {
          auto accountId = param.find(CCAPI_EM_ACCOUNT_ID) != param.end() ? param.at(CCAPI_EM_ACCOUNT_ID) : "";
          this->substituteParam(target, {
                                            {"<accountId>", accountId},
                                        });
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getAccountPositionsTarget);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOid", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("dealSize", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("dealFunds", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    const rj::Value& data = document["data"];
    if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      if (data.FindMember("cancelledOrderIds") != data.MemberEnd()) {
        for (const auto& x : data["cancelledOrderIds"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ORDER_ID, x.GetString());
          elementList.emplace_back(std::move(element));
        }
      } else {
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, data["cancelledOrderId"].GetString());
        element.insert(CCAPI_EM_CLIENT_ORDER_ID, data["clientOid"].GetString());
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::CREATE_ORDER) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, data["orderId"].GetString());
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : data["items"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::GET_ORDER) {
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  void extractOrderInfo(Element& element, const rj::Value& x,
                        const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
    {
      auto it = x.FindMember("isActive");
      if (it != x.MemberEnd()) {
        element.insert("isActive", it->value.GetBool() ? "true" : "false");
      }
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::string topic;
    const auto& fieldSet = subscription.getFieldSet();
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      topic = this->topicTradeOrders;
    }
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    static int nextRequestId = 1;
    std::string requestId = std::to_string(nextRequestId);
    nextRequestId += 1;
    document.AddMember("id", rj::Value(requestId.c_str(), allocator).Move(), allocator);
    document.AddMember("type", rj::Value("subscribe").Move(), allocator);
    document.AddMember("privateChannel", true, allocator);
    document.AddMember("response", true, allocator);
    document.AddMember("topic", rj::Value(topic.c_str(), allocator).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
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
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
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
    std::string type = document["type"].GetString();
    if (type == "message") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      std::string topic = document["topic"].GetString();
      if (topic == this->topicTradeOrders) {
        const rj::Value& data = document["data"];
        std::string instrument = data["symbol"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          std::string ts = std::string(data["ts"].GetString());
          auto time = UtilTime::makeTimePoint({std::stoll(ts.substr(0, ts.length() - 9)), std::stoll(ts.substr(ts.length() - 9))});
          std::string dataType = data["type"].GetString();
          if (dataType == "match" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(time);
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_TRADE_ID, data["tradeId"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["matchPrice"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["matchSize"].GetString());
            element.insert(CCAPI_EM_ORDER_SIDE, std::string(data["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            element.insert(CCAPI_IS_MAKER, std::string(data["liquidity"].GetString()) == "taker" ? "0" : "1");
            element.insert(CCAPI_EM_ORDER_ID, data["orderId"].GetString());
            element.insert(CCAPI_EM_CLIENT_ORDER_ID, data["clientOid"].GetString());
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(time);
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            std::map<std::string, std::pair<std::string, JsonDataType> > extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOid", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filledSize", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("remainSize", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
            };
            extractionFieldNameMap.insert({CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)});
            Element info;
            this->extractOrderInfo(info, data, extractionFieldNameMap);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (type == "ack") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else if (type == "error") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else if (type == "welcome") {
      this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] =
          std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingInterval"));
      this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] =
          std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingTimeout"));
      if (this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] <=
          this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL]) {
        this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] =
            this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] - 1;
      }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      ExecutionManagementService::onOpen(hdl);
#else
      ExecutionManagementService::onOpen(wsConnectionPtr);
#endif
    } else if (type == "pong") {
      auto now = UtilTime::now();
      this->lastPongTpByMethodByConnectionIdMap[wsConnection.id][PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] = now;
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string apiPassphraseName;
  bool isDerivatives{};
  std::string topicTradeOrders;
  std::string createOrderMarginTarget;
  std::string getOrderByClientOrderIdTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_BASE_H_
