#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GEMINI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GEMINI_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceGemini : public ExecutionManagementService {
 public:
  ExecutionManagementServiceGemini(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GEMINI;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v1/order/events";
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
    this->apiKeyName = CCAPI_GEMINI_API_KEY;
    this->apiSecretName = CCAPI_GEMINI_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/v1/order/new";
    this->cancelOrderTarget = "/v1/order/cancel";
    this->getOrderTarget = "/v1/order/status";
    this->getOpenOrdersTarget = "/v1/orders";
    this->cancelOpenOrdersTarget = "/v1/order/cancel/session";
    this->getAccountsTarget = "/v1/account/list";
    this->getAccountBalancesTarget = "/v1/balances";
  }
  virtual ~ExecutionManagementServiceGemini() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto headerMap = ExecutionManagementService::convertHeaderStringToMap(headerString);
    auto base64Payload = mapGetWithDefault(headerMap, std::string("X-GEMINI-PAYLOAD"));
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, base64Payload, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "X-GEMINI-SIGNATURE:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, rj::Document& document, rj::Document::AllocatorType& allocator,
                   const std::map<std::string, std::string>& param, const TimePoint& now, const std::map<std::string, std::string>& credential, int64_t nonce) {
    document.AddMember("request", rj::Value(req.target().to_string().c_str(), allocator).Move(), allocator);
    document.AddMember("nonce", rj::Value(nonce).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    auto body = stringBuffer.GetString();
    this->signRequest(req, body, credential);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto base64Payload = UtilAlgorithm::base64Encode(body);
    req.set("X-GEMINI-PAYLOAD", base64Payload);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, base64Payload, true);
    req.set("X-GEMINI-SIGNATURE", signature);
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      if (key == "order_id") {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(static_cast<int64_t>(std::stoll(value))).Move(), allocator);
      } else {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
      }
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("Content-Length", "0");
    req.set("Content-Type", "text/plain");
    req.set("X-GEMINI-APIKEY", apiKey);
    req.set("Cache-Control", "no-cache");
    int64_t nonce = this->generateNonce(now, request.getIndex());
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_SIDE, "side"},
                              {CCAPI_EM_ORDER_QUANTITY, "amount"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                          });
        this->appendSymbolId(document, allocator, symbolId);
        if (param.find("type") == param.end()) {
          document.AddMember("type", rj::Value("exchange limit").Move(), allocator);
        }
        this->signRequest(req, document, allocator, param, now, credential, nonce);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "order_id"},
                          });
        this->signRequest(req, document, allocator, param, now, credential, nonce);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "order_id"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, param, now, credential, nonce);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->getOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, {},
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, {}, now, credential, nonce);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->cancelOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, {},
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, {}, now, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::post);
        req.target(this->getAccountsTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->signRequest(req, document, allocator, {}, now, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        req.target(this->getAccountBalancesTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, {},
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, {}, now, credential, nonce);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("original_amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executed_amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document["details"]["cancelledOrders"].GetArray()) {
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, std::string(x.GetString()));
        elementList.emplace_back(std::move(element));
      }
    } else if (document.IsObject()) {
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
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, x["account"].GetString());
          element.insert(CCAPI_EM_ACCOUNT_TYPE, x["type"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  void extractOrderInfo(Element& element, const rj::Value& x,
                        const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("executed_amount");
      auto it2 = x.FindMember("avg_execution_price");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
      }
    }
    {
      auto it = x.FindMember("is_live");
      if (it != x.MemberEnd()) {
        element.insert("is_live", it->value.GetBool() ? "true" : "false");
      }
    }
    {
      auto it = x.FindMember("is_cancelled");
      if (it != x.MemberEnd()) {
        element.insert("is_cancelled", it->value.GetBool() ? "true" : "false");
      }
    }
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void prepareConnect(WsConnection& wsConnection) override {
    auto now = UtilTime::now();
    auto subscription = wsConnection.subscriptionList.at(0);
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    wsConnection.url += "?heartbeat=true";
    if (fieldSet == std::set<std::string>({CCAPI_EM_PRIVATE_TRADE})) {
      wsConnection.url += "&eventTypeFilter=fill";
    }
    if (!instrumentSet.empty()) {
      for (const auto& instrument : instrumentSet) {
        wsConnection.url += "&symbolFilter=" + instrument;
      }
    }
    wsConnection.url += "&apiSessionFilter=" + apiKey;
    wsConnection.headers.insert({"X-GEMINI-APIKEY", apiKey});
    int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string payload = R"({"request":"/v1/order/events","nonce":)" + std::to_string(nonce) + "}";
    auto base64Payload = UtilAlgorithm::base64Encode(payload);
    wsConnection.headers.insert({"X-GEMINI-PAYLOAD", base64Payload});
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, base64Payload, true);
    wsConnection.headers.insert({"X-GEMINI-SIGNATURE", signature});
    this->connect(wsConnection);
  }
#else
  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    auto now = UtilTime::now();
    auto subscription = wsConnectionPtr->subscriptionList.at(0);
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    wsConnectionPtr->url += "?heartbeat=true";
    if (fieldSet == std::set<std::string>({CCAPI_EM_PRIVATE_TRADE})) {
      wsConnectionPtr->url += "&eventTypeFilter=fill";
    }
    if (!instrumentSet.empty()) {
      for (const auto& instrument : instrumentSet) {
        wsConnectionPtr->url += "&symbolFilter=" + instrument;
      }
    }
    wsConnectionPtr->url += "&apiSessionFilter=" + apiKey;
    wsConnectionPtr->headers.insert({"X-GEMINI-APIKEY", apiKey});
    int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string payload = R"({"request":"/v1/order/events","nonce":)" + std::to_string(nonce) + "}";
    auto base64Payload = UtilAlgorithm::base64Encode(payload);
    wsConnectionPtr->headers.insert({"X-GEMINI-PAYLOAD", base64Payload});
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, base64Payload, true);
    wsConnectionPtr->headers.insert({"X-GEMINI-SIGNATURE", signature});
    this->connect(wsConnectionPtr);
  }
#endif
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
    if (document.IsObject()) {
      std::string type = document["type"].GetString();
      if (type == "subscription_ack") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        Message message;
        message.setType(Message::Type::SUBSCRIPTION_STARTED);
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        Element element;
        element.insert(CCAPI_INFO_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    } else if (document.IsArray()) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      for (const auto& x : document.GetArray()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        std::string type = x["type"].GetString();
        if (this->websocketOrderUpdateTypeSet.find(type) != websocketOrderUpdateTypeSet.end()) {
          std::string instrument = std::string(x["symbol"].GetString());
          if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
            auto it = x.FindMember("timestampms");
            if (it != x.MemberEnd()) {
              message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(it->value.GetString()))));
            }
            if (type == "fill" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
              std::vector<Element> elementList;
              Element element;
              const rj::Value& fill = x["fill"];
              element.insert(CCAPI_TRADE_ID, fill["trade_id"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, fill["price"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, fill["amount"].GetString());
              element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, std::string(fill["liquidity"].GetString()) == "Taker" ? "0" : "1");
              element.insert(CCAPI_EM_ORDER_ID, x["order_id"].GetString());
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["client_order_id"].GetString());
              element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, fill["fee"].GetString());
              element.insert(CCAPI_EM_ORDER_FEE_ASSET, fill["fee_currency"].GetString());
              elementList.emplace_back(std::move(element));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
            if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("original_amount", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("remaining_amount", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executed_amount", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
              };
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              {
                auto it1 = x.FindMember("executed_amount");
                auto it2 = x.FindMember("avg_execution_price");
                if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
                  info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                              std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
                }
              }
              {
                auto it = x.FindMember("is_live");
                if (it != x.MemberEnd()) {
                  info.insert("is_live", it->value.GetBool() ? "1" : "0");
                }
              }
              {
                auto it = x.FindMember("is_cancelled");
                if (it != x.MemberEnd()) {
                  info.insert("is_cancelled", it->value.GetBool() ? "1" : "0");
                }
              }
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::set<std::string> websocketOrderUpdateTypeSet{"accepted", "rejected", "booked", "fill", "cancelled", "cancel_rejected", "closed"};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GEMINI_H_
