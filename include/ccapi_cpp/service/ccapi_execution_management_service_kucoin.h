#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceKucoin : public ExecutionManagementService {
 public:
  ExecutionManagementServiceKucoin(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_KUCOIN;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_KUCOIN_API_KEY;
    this->apiSecretName = CCAPI_KUCOIN_API_SECRET;
    this->apiPassphraseName = CCAPI_KUCOIN_API_PASSPHRASE;
    this->apiKeyVersionName = CCAPI_KUCOIN_API_KEY_VERSION;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName, this->apiKeyVersionName});
    this->createOrderTarget = "/api/v1/orders";
    this->cancelOrderTarget = "/api/v1/orders/<id>";
    this->getOrderTarget = "/api/v1/orders/<id>";
    this->getOpenOrdersTarget = "/api/v1/orders";
    this->cancelOpenOrdersTarget = "/api/v1/orders";
    this->getAccountsTarget = "/api/v1/accounts";
    this->getAccountBalancesTarget = "/api/v1/accounts/<accountId>";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceKucoin() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
  }
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override {
    return !std::regex_search(body, std::regex("\"code\":\\s*\"200000\""));
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
        [wsConnection, that = shared_from_base<ExecutionManagementServiceKucoin>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<ExecutionManagementServiceKucoin>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse(body.c_str());
              const rj::Value& instanceServer = document["data"]["instanceServers"][0];
              urlWebsocketBase += std::string(instanceServer["endpoint"].GetString());
              urlWebsocketBase += "?token=";
              urlWebsocketBase += std::string(document["data"]["token"].GetString());
              thisWsConnection.url = urlWebsocketBase;
              that->connect(thisWsConnection);
              that->extraPropertyByConnectionIdMap[thisWsConnection.id].insert({
                  {"pingInterval", std::to_string(instanceServer["pingInterval"].GetInt())},
                  {"pingTimeout", std::to_string(instanceServer["pingTimeout"].GetInt())},
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
  void signApiPassphrase(http::request<http::string_body>& req, const std::string& apiKeyVersion, const std::string& apiPassphrase,
                         const std::string& apiSecret) {
    if (apiKeyVersion == "2") {
      req.set("KC-API-PASSPHRASE", UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, apiPassphrase)));
    } else {
      req.set("KC-API-PASSPHRASE", apiPassphrase);
    }
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOid"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
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
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("KC-API-KEY", apiKey);
    req.set("KC-API-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    auto apiKeyVersion = mapGetWithDefault(credential, this->apiKeyVersionName);
    req.set("KC-API-KEY-VERSION", apiKeyVersion);
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    this->signApiPassphrase(req, apiKeyVersion, apiPassphrase, apiSecret);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, now, credential);
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        bool useOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = useOrderId ? param.at(CCAPI_EM_ORDER_ID)
                                    : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client-order/" + param.at(CCAPI_EM_CLIENT_ORDER_ID) : "";
        auto target =
            useOrderId ? std::regex_replace(this->cancelOrderTarget, std::regex("<id>"), id) : std::regex_replace("/api/v1/order/<id>", std::regex("<id>"), id);
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        bool useOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = useOrderId ? param.at(CCAPI_EM_ORDER_ID)
                                    : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client-order/" + param.at(CCAPI_EM_CLIENT_ORDER_ID) : "";
        auto target =
            useOrderId ? std::regex_replace(this->getOrderTarget, std::regex("<id>"), id) : std::regex_replace("/api/v1/order/<id>", std::regex("<id>"), id);
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        auto target = this->getOpenOrdersTarget;
        target += "?status=active";
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
        if (!symbolId.empty()) {
          target += "?symbol=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::get);
        req.target(this->getAccountsTarget);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto target = this->getAccountBalancesTarget;
        auto accountId = param.find(CCAPI_EM_ACCOUNT_ID) != param.end() ? param.at(CCAPI_EM_ACCOUNT_ID) : "";
        this->substituteParam(target, {
                                          {"<accountId>", accountId},
                                      });
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOid", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    std::vector<Element> elementList;
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
        auto element = this->extractOrderInfo(x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::GET_ORDER) {
      auto element = this->extractOrderInfo(data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
    return elementList;
  }
  Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    Element element = ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap);
    {
      auto it = x.FindMember("isActive");
      if (it != x.MemberEnd()) {
        element.insert("isActive", it->value.GetBool() ? "true" : "false");
      }
    }
    return element;
  }
  std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    std::vector<Element> elementList;
    const auto& data = document["data"];
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : data.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, x["id"].GetString());
          element.insert(CCAPI_EM_ACCOUNT_TYPE, x["type"].GetString());
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        element.insert(CCAPI_EM_ASSET, data["currency"].GetString());
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, data["available"].GetString());
        elementList.emplace_back(std::move(element));
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return elementList;
  }
  std::vector<std::string> createSendStringListFromSubscription(const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::string topic;
    auto fieldSet = subscription.getFieldSet();
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      topic = "/spotMarket/tradeOrders";
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
  void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto subscription = wsConnection.subscriptionList.at(0);
    rj::Document document;
    document.Parse(textMessage.c_str());
    Event event = this->createEvent(hdl, subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event);
    }
  }
  Event createEvent(wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage, const rj::Document& document,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    std::string type = document["type"].GetString();
    if (type == "message") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      auto fieldSet = subscription.getFieldSet();
      auto instrumentSet = subscription.getInstrumentSet();
      std::string topic = document["topic"].GetString();
      if (topic == "/spotMarket/tradeOrders") {
        const rj::Value& data = document["data"];
        auto instrument = this->convertWebsocketSymbolIdToInstrument(data["symbol"].GetString());
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          std::string ts = std::to_string(data["ts"].GetInt64());
          message.setTime(UtilTime::makeTimePoint({std::stoll(ts.substr(0, ts.length() - 9)), std::stoll(ts.substr(ts.length() - 9))}));
          std::string dataType = data["type"].GetString();
          if (dataType == "match" && (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end() || fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end())) {
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_TRADE_ID, data["tradeId"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["matchPrice"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["matchSize"].GetString());
            element.insert(CCAPI_EM_ORDER_SIDE, std::string(data["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            element.insert(CCAPI_IS_MAKER, std::string(data["liquidity"].GetString()) == "taker" ? "0" : "1");
            element.insert(CCAPI_EM_ORDER_ID, data["orderId"].GetString());
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.push_back(std::move(message));
          } else if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
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
            auto info = this->extractOrderInfo(data, extractionFieldNameMap);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.push_back(std::move(message));
          }
        }
      }
    } else if (type == "ack") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.push_back(std::move(message));
    } else if (type == "error") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.push_back(std::move(message));
    } else if (type == "welcome") {
      ExecutionManagementService::onOpen(hdl);
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string apiPassphraseName;
  std::string apiKeyVersionName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_H_
