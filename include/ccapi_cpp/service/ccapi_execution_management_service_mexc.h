#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_MEXC_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_MEXC_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceMexc : public ExecutionManagementService {
 public:
  ExecutionManagementServiceMexc(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->pingListenKeyIntervalSeconds = 600;
    this->exchangeName = CCAPI_EXCHANGE_NAME_MEXC;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->apiKeyName = CCAPI_MEXC_API_KEY;
    this->apiSecretName = CCAPI_MEXC_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = CCAPI_MEXC_CREATE_ORDER_PATH;
    this->cancelOrderTarget = "/api/v3/order";
    this->getOrderTarget = "/api/v3/order";
    this->getOpenOrdersTarget = "/api/v3/openOrders";
    this->cancelOpenOrdersTarget = "/api/v3/openOrders";
    this->listenKeyTarget = CCAPI_MEXC_LISTEN_KEY_PATH;
    this->getAccountBalancesTarget = "/api/v3/account";
  }
  virtual ~ExecutionManagementServiceMexc() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  // bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("code":0)") == std::string::npos; }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"method":"PING"})", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"method":"PING"})", ec);
  }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    if (queryString.find("timestamp=") == std::string::npos) {
      if (!queryString.empty()) {
        queryString += "&";
      }
      queryString += "timestamp=";
      queryString += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, queryString, true);
    queryString += "&signature=";
    queryString += signature;
  }
  void signRequest(std::string& queryString, const std::map<std::string, std::string>& param, const TimePoint& now,
                   const std::map<std::string, std::string>& credential) {
    if (param.find("timestamp") == param.end()) {
      queryString += "timestamp=";
      queryString += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
      queryString += "&";
    }
    if (queryString.back() == '&') {
      queryString.pop_back();
    }
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, queryString, true);
    queryString += "&signature=";
    queryString += signature;
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
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void prepareReq(http::request<http::string_body>& req, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-MEXC-APIKEY", apiKey);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, credential);
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.set("source", CCAPI_MEXC_API_SOURCE);
        req.method(http::verb::post);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_SIDE, "side"},
                              {CCAPI_EM_ORDER_QUANTITY, "quantity"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "newClientOrderId"},
                          });
        this->appendSymbolId(queryString, symbolId);
        if (param.find("type") == param.end()) {
          queryString += "type=LIMIT&";
        }
        this->signRequest(queryString, param, now, credential);
        req.target(std::string(this->createOrderTarget) + "?" + queryString);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderId"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "origClientOrderId"},
                          });
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->cancelOrderTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderId"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "origClientOrderId"},
                          });
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->getOrderTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        this->signRequest(queryString, {}, now, credential);
        req.target(this->getOpenOrdersTarget + "?" + queryString);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        std::string queryString;
        this->appendParam(queryString, {});
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, {}, now, credential);
        req.target(this->cancelOpenOrdersTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        this->signRequest(queryString, {}, now, credential);
        req.target(this->getAccountBalancesTarget + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    std::map<std::string, std::pair<std::string, JsonDataType> > extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executedQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("cummulativeQuoteQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      extractionFieldNameMap.insert({CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("origClientOrderId", JsonDataType::STRING)});
    } else {
      extractionFieldNameMap.insert({CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)});
    }
    if (document.IsObject()) {
      Element element;
      this->extractOrderInfo(element, document, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else {
      for (const auto& x : document.GetArray()) {
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
        for (const auto& x : document["balances"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, Decimal(x["free"].GetString()).add(Decimal(x["locked"].GetString())).toString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void prepareConnect(WsConnection& wsConnection) override {
    auto now = UtilTime::now();
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-MEXC-APIKEY", apiKey);
    std::string queryString;
    this->signRequest(queryString, {}, now, credential);
    req.target(this->listenKeyTarget + "?" + queryString);
    this->sendRequest(
        req,
        [wsConnection, that = shared_from_base<ExecutionManagementServiceMexc>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<ExecutionManagementServiceMexc>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              std::string listenKey = document["listenKey"].GetString();
              std::string url = that->baseUrlWs + "?listenKey=" + listenKey;
              thisWsConnection.url = url;
              that->connect(thisWsConnection);
              that->extraPropertyByConnectionIdMap[thisWsConnection.id].insert({
                  {"listenKey", listenKey},
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
  void onOpen(wspp::connection_hdl hdl) override {
    ExecutionManagementService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->setPingListenKeyTimer(wsConnection);
  }
  void setPingListenKeyTimer(const WsConnection& wsConnection) {
    this->pingListenKeyTimerMapByConnectionIdMap[wsConnection.id] = this->serviceContextPtr->tlsClientPtr->set_timer(
        this->pingListenKeyIntervalSeconds * 1000, [wsConnection, that = shared_from_base<ExecutionManagementServiceMexc>()](ErrorCode const& ec) {
          if (ec) {
            return;
          }
          auto now = UtilTime::now();
          that->setPingListenKeyTimer(wsConnection);
          http::request<http::string_body> req;
          req.set(http::field::host, that->hostRest);
          req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
          req.method(http::verb::put);
          std::string queryString;
          std::map<std::string, std::string> params;
          auto listenKey = that->extraPropertyByConnectionIdMap.at(wsConnection.id).at("listenKey");
          params.insert({"listenKey", listenKey});
          for (const auto& param : params) {
            queryString += param.first + "=" + Url::urlEncode(param.second);
            queryString += "&";
          }
          auto credential = wsConnection.subscriptionList.at(0).getCredential();
          if (credential.empty()) {
            credential = that->credentialDefault;
          }
          auto apiKey = mapGetWithDefault(credential, that->apiKeyName);
          req.set("X-MEXC-APIKEY", apiKey);
          that->signRequest(queryString, {}, now, credential);
          req.target(that->listenKeyTarget + "?" + queryString);
          that->sendRequest(
              req,
              [wsConnection, that_2 = that->shared_from_base<ExecutionManagementServiceMexc>()](const beast::error_code& ec) {
                CCAPI_LOGGER_ERROR("ping listen key fail");
                that_2->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping listen key");
              },
              [wsConnection, that_2 = that->shared_from_base<ExecutionManagementServiceMexc>()](const http::response<http::string_body>& res) {
                CCAPI_LOGGER_DEBUG("ping listen key success");
              },
              that->sessionOptions.httpRequestTimeoutMilliSeconds);
        });
  }
  void onClose(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    if (this->pingListenKeyTimerMapByConnectionIdMap.find(wsConnection.id) != this->pingListenKeyTimerMapByConnectionIdMap.end()) {
      this->pingListenKeyTimerMapByConnectionIdMap.at(wsConnection.id)->cancel();
      this->pingListenKeyTimerMapByConnectionIdMap.erase(wsConnection.id);
    }
    ExecutionManagementService::onClose(hdl);
  }
#else
  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    auto now = UtilTime::now();
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-MEXC-APIKEY", apiKey);
    std::string queryString;
    this->signRequest(queryString, {}, now, credential);
    req.target(this->listenKeyTarget + "?" + queryString);
    this->sendRequest(
        req, [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceMexc>()](const beast::error_code& ec) { that->onFail_(wsConnectionPtr); },
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceMexc>()](const http::response<http::string_body>& res) {
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              std::string listenKey = document["listenKey"].GetString();
              std::string url = that->baseUrlWs + "?listenKey=" + listenKey;
              wsConnectionPtr->url = url;
              that->connect(wsConnectionPtr);
              that->extraPropertyByConnectionIdMap[wsConnectionPtr->id].insert({
                  {"listenKey", listenKey},
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
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    ExecutionManagementService::onOpen(wsConnectionPtr);
    this->setPingListenKeyTimer(wsConnectionPtr);
  }
  void setPingListenKeyTimer(std::shared_ptr<WsConnection> wsConnectionPtr) {
    TimerPtr timerPtr(
        new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(this->pingListenKeyIntervalSeconds * 1000)));
    timerPtr->async_wait([wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceMexc>()](ErrorCode const& ec) {
      if (ec) {
        return;
      }
      auto now = UtilTime::now();
      that->setPingListenKeyTimer(wsConnectionPtr);
      http::request<http::string_body> req;
      req.set(http::field::host, that->hostRest);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.method(http::verb::put);
      std::string queryString;
      std::map<std::string, std::string> params;
      auto listenKey = that->extraPropertyByConnectionIdMap.at(wsConnectionPtr->id).at("listenKey");
      params.insert({"listenKey", listenKey});
      for (const auto& param : params) {
        queryString += param.first + "=" + Url::urlEncode(param.second);
        queryString += "&";
      }
      auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
      if (credential.empty()) {
        credential = that->credentialDefault;
      }
      auto apiKey = mapGetWithDefault(credential, that->apiKeyName);
      req.set("X-MEXC-APIKEY", apiKey);
      that->signRequest(queryString, {}, now, credential);
      req.target(that->listenKeyTarget + "?" + queryString);
      that->sendRequest(
          req,
          [wsConnectionPtr, that_2 = that->shared_from_base<ExecutionManagementServiceMexc>()](const beast::error_code& ec) {
            CCAPI_LOGGER_ERROR("ping listen key fail");
            that_2->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping listen key");
          },
          [wsConnectionPtr, that_2 = that->shared_from_base<ExecutionManagementServiceMexc>()](const http::response<http::string_body>& res) {
            CCAPI_LOGGER_DEBUG("ping listen key success");
          },
          that->sessionOptions.httpRequestTimeoutMilliSeconds);
    });
    this->pingListenKeyTimerMapByConnectionIdMap[wsConnectionPtr->id] = timerPtr;
  }
  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    if (this->pingListenKeyTimerMapByConnectionIdMap.find(wsConnectionPtr->id) != this->pingListenKeyTimerMapByConnectionIdMap.end()) {
      this->pingListenKeyTimerMapByConnectionIdMap.at(wsConnectionPtr->id)->cancel();
      this->pingListenKeyTimerMapByConnectionIdMap.erase(wsConnectionPtr->id);
    }
    ExecutionManagementService::onClose(wsConnectionPtr, ec);
  }
#endif
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("method", rj::Value("SUBSCRIPTION").Move(), allocator);
    rj::Value params(rj::kArrayType);
    const auto& fieldSet = subscription.getFieldSet();
    for (const auto& field : subscription.getFieldSet()) {
      std::string channelId;
      if (field == CCAPI_EM_ORDER_UPDATE) {
        channelId = "spot@private.orders.v3.api";
      } else if (field == CCAPI_EM_PRIVATE_TRADE) {
        channelId = "spot@private.deals.v3.api";
      }
      params.PushBack(rj::Value(channelId.c_str(), allocator).Move(), allocator);
    }
    document.AddMember("params", params, allocator);
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
#endif
    Event event;
    std::vector<Message> messageList;
    if (document.IsObject() && document.HasMember("code") && std::string(document["code"].GetString()) == "0") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::string msg = document["msg"].GetString();
      if (msg != "PONG") {
        bool success = msg != "no subscription success";
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        message.setType(success ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(success ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    } else {
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      std::string c = std::string(document["c"].GetString());
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& d = document["d"];
      std::string instrument = document["s"].GetString();
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
        if (c == "spot@private.deals.v3.api" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(d["T"].GetString()))));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, std::string(d["t"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(d["p"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(d["v"].GetString()));
          element.insert(CCAPI_EM_ORDER_SIDE, std::string(d["S"].GetString()) == "1" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_IS_MAKER, std::string(d["m"].GetString()));
          element.insert(CCAPI_EM_ORDER_ID, std::string(d["i"].GetString()));
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
          {
            auto it = d.FindMember("c");
            if (it != d.MemberEnd() && !it->value.IsNull()) {
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(it->value.GetString()));
            }
          }
          elementList.emplace_back(std::move(element));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        } else if (c == "spot@private.orders.v3.api" && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(document["t"].GetString()))));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
              {CCAPI_EM_ORDER_ID, std::make_pair("i", JsonDataType::STRING)},
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("c", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("p", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("v", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cv", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("ca", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, std::make_pair("ap", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("V", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_STATUS, std::make_pair("s", JsonDataType::STRING)},
          };
          Element info;
          info.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
          info.insert(CCAPI_EM_ORDER_SIDE, std::string(d["S"].GetString()) == "1" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          this->extractOrderInfo(info, d, extractionFieldNameMap);
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string listenKeyTarget;
  int pingListenKeyIntervalSeconds;
  std::map<std::string, TimerPtr> pingListenKeyTimerMapByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_MEXC_H_
