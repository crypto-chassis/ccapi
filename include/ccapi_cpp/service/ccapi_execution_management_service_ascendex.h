#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_ASCENDEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_ASCENDEX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_ASCENDEX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceAscendex : public ExecutionManagementService {
 public:
  ExecutionManagementServiceAscendex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                     ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_ASCENDEX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
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
    this->apiKeyName = CCAPI_ASCENDEX_API_KEY;
    this->apiSecretName = CCAPI_ASCENDEX_API_SECRET;
    this->apiAccountGroupName = CCAPI_ASCENDEX_API_ACCOUNT_GROUP;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiAccountGroupName});
    this->createOrderTarget = "/api/pro/v1/cash/order";
    this->cancelOrderTarget = "/api/pro/v1/cash/order";
    this->getOrderTarget = "/api/pro/v1/cash/order/status";
    this->getOpenOrdersTarget = "/api/pro/v1/cash/order/open";
    this->cancelOpenOrdersTarget = "/api/pro/v1/cash/order/all";
    this->getAccountBalancesTarget = "/api/pro/v1/cash/balance";
  }
  virtual ~ExecutionManagementServiceAscendex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("code":0)") == std::string::npos; }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }
  void onOpen(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, R"({"op":"ping"})", ec); }
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override { wsConnectionPtr->status = WsConnection::Status::OPEN; }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("x-auth-timestamp").to_string();
    preSignedText += "+";
    auto splitted = UtilString::split(path, '/');
    std::vector<std::string> subSplitted(splitted.begin() + 6, splitted.begin() + splitted.size());
    preSignedText += UtilString::join(subSplitted, "/");
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("x-auth-signature", signature);
    if (!body.empty()) {
      req.body() = body;
      req.prepare_payload();
    }
  }
  void signRequest(http::request<http::string_body>& req, const std::string& apiPath, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("x-auth-timestamp").to_string();
    preSignedText += "+";
    preSignedText += apiPath;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("x-auth-signature", signature);
  }
  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "orderQty"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "orderPrice"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "id"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
      }
      if (value != "null") {
        if (value == "true" || value == "false") {
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
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
  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    rjValue.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "instId=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::accept, "application/json");
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("x-auth-key", apiKey);
    req.set("x-auth-timestamp", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto& accountGroup = mapGetWithDefault(credential, this->apiAccountGroupName);
        req.target("/" + accountGroup + this->createOrderTarget);
        this->signRequest(req, "order", credential);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        if (param.find("time") == param.end()) {
          document.AddMember("time", rj::Value(static_cast<int64_t>(std::stoll(req.base().at("x-auth-timestamp").to_string()))).Move(), allocator);
        }
        if (param.find("orderType") == param.end()) {
          document.AddMember("orderType", rj::Value("limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto& accountGroup = mapGetWithDefault(credential, this->apiAccountGroupName);
        req.target("/" + accountGroup + this->cancelOrderTarget);
        this->signRequest(req, "order", credential);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        if (param.find("time") == param.end()) {
          document.AddMember("time", rj::Value(static_cast<int64_t>(std::stoll(req.base().at("x-auth-timestamp").to_string()))).Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto& accountGroup = mapGetWithDefault(credential, this->apiAccountGroupName);
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderId"},
                          });
        req.target("/" + accountGroup + this->getOrderTarget + "?" + queryString);
        this->signRequest(req, "order/status", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto& accountGroup = mapGetWithDefault(credential, this->apiAccountGroupName);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        std::string target("/" + accountGroup + this->getOpenOrdersTarget);
        if (!queryString.empty()) {
          target += "?" + queryString;
        }
        req.target(target);
        this->signRequest(req, "order/open", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto& accountGroup = mapGetWithDefault(credential, this->apiAccountGroupName);
        req.target("/" + accountGroup + this->cancelOpenOrdersTarget);
        this->signRequest(req, "order/all", credential);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto& accountGroup = mapGetWithDefault(credential, this->apiAccountGroupName);
        req.target("/" + accountGroup + this->getAccountBalancesTarget);
        this->signRequest(req, "balance", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, const WsConnection& wsConnection, const Request& request,
                                  int wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    document.SetObject();
    document.AddMember("id", rj::Value(std::to_string(wsRequestId).c_str(), allocator).Move(), allocator);
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        document.AddMember("op", rj::Value("req").Move(), allocator);
        document.AddMember("action", rj::Value("place-Order").Move(), allocator);
        document.AddMember("account", rj::Value("cash").Move(), allocator);
        rj::Value args(rj::kObjectType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(args, allocator, param);
        if (param.find("time") == param.end()) {
          args.AddMember("time", rj::Value(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()).Move(), allocator);
        }
        if (param.find("orderType") == param.end()) {
          args.AddMember("orderType", rj::Value("limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(args, allocator, symbolId);
        }
        document.AddMember("args", args, allocator);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        document.AddMember("op", rj::Value("req").Move(), allocator);
        document.AddMember("action", rj::Value("cancel-Order").Move(), allocator);
        document.AddMember("account", rj::Value("cash").Move(), allocator);
        rj::Value args(rj::kObjectType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(args, allocator, param);
        if (param.find("time") == param.end()) {
          args.AddMember("time", rj::Value(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()).Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(args, allocator, symbolId);
        }
        document.AddMember("args", args, allocator);
      } break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnection, request, wsRequestId, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    this->extractOrderInfoFromRequest(elementList, document);
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const rj::Value& value) {
    const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("orderQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumFilledQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    auto itData = value.FindMember("data");
    if (itData != value.MemberEnd()) {
      const rj::Value& data = itData->value;
      if (data.IsObject()) {
        Element element;
        if (data.FindMember("info") != data.MemberEnd()) {
          this->extractOrderInfo(element, data["info"], extractionFieldNameMap);
        } else {
          this->extractOrderInfo(element, data, extractionFieldNameMap);
        }
        elementList.emplace_back(std::move(element));
      } else {
        for (const auto& x : data.GetArray()) {
          Element element;
          this->extractOrderInfo(element, x, extractionFieldNameMap);
          elementList.emplace_back(std::move(element));
        }
      }
    } else {
      Element element;
      this->extractOrderInfo(element, value, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["availableBalance"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["totalBalance"].GetString());
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
      auto it1 = x.FindMember("cumFilledQty");
      auto it2 = x.FindMember("avgPx");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        auto it1Str = std::string(it1->value.GetString());
        auto it2Str = std::string(it2->value.GetString());
        if (!it1Str.empty() && !it2Str.empty()) {
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                         Decimal(UtilString::printDoubleScientific(std::stod(it1Str) * std::stod(it2Str))).toString());
        }
      }
    }
  }
  void subscribe(std::vector<Subscription>& subscriptionList) override {
    if (this->shouldContinue.load()) {
      for (auto& subscription : subscriptionList) {
        boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<ExecutionManagementServiceAscendex>(), subscription]() mutable {
          auto now = UtilTime::now();
          subscription.setTimeSent(now);
          auto credential = subscription.getCredential();
          if (credential.empty()) {
            credential = that->credentialDefault;
          }
          const auto& accountGroup = mapGetWithDefault(credential, that->apiAccountGroupName);
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          WsConnection wsConnection(that->baseUrlWs + "/" + accountGroup + "/api/pro/v1/stream", "", {subscription}, credential);
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
                                std::shared_ptr<WsConnection> wsConnectionPtr(new WsConnection(that->baseUrlWs + "/" + accountGroup + "/api/pro/v1/stream", "", {subscription}, credential, streamPtr));
                                CCAPI_LOGGER_WARN("about to subscribe with new wsConnectionPtr " + toString(*wsConnectionPtr));
                                that->prepareConnect(wsConnectionPtr);
#endif
        });
      }
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("auth").Move(), allocator);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    int64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    document.AddMember("key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    document.AddMember("t", rj::Value(t).Move(), allocator);
    std::string signData = std::to_string(t) + "+stream";
    std::string sign = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData));
    document.AddMember("sig", rj::Value(sign.c_str(), allocator).Move(), allocator);
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
#endif
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    auto mIt = document.FindMember("m");
    if (mIt != document.MemberEnd()) {
      std::string m = mIt->value.GetString();
      if (m == "order") {
        auto itAction = document.FindMember("action");
        if (itAction != document.MemberEnd()) {
          std::string action = itAction->value.GetString();
          event.setType(Event::Type::RESPONSE);
          std::string status = document["status"].GetString();
          if (status == "Err") {
            message.setType(Message::Type::RESPONSE_ERROR);
            Element element;
            element.insert(CCAPI_ERROR_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          } else {
            std::vector<Element> elementList;
            if (action == "place-order") {
              message.setType(Message::Type::CREATE_ORDER);
            } else if (action == "cancel-order") {
              message.setType(Message::Type::CANCEL_ORDER);
            }
            this->extractOrderInfoFromRequest(elementList, document["info"]);
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        } else {
          const rj::Value& data = document["data"];
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          const auto& fieldSet = subscription.getFieldSet();
          const auto& instrumentSet = subscription.getInstrumentSet();
          std::string instrument = data["s"].GetString();
          if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
            message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(data["t"].GetString()))));
            if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("sd", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("p", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("q", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cfq", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("st", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("s", JsonDataType::STRING)},
              };
              Element info;
              this->extractOrderInfo(info, data, extractionFieldNameMap);
              auto it = data.FindMember("ap");
              if (it != data.MemberEnd() && !it->value.IsNull()) {
                info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                            Decimal(UtilString::printDoubleScientific(std::stod(it->value.GetString()) * std::stod(data["cfq"].GetString()))).toString());
              }
              info.insert(CCAPI_EM_BASE_ASSET_QUANTITY_AVAILABLE_FOR_TRADING, data["bab"].GetString());
              info.insert(CCAPI_EM_BASE_ASSET_QUANTITY_TOTAL, data["btb"].GetString());
              info.insert(CCAPI_EM_QUOTE_ASSET_QUANTITY_AVAILABLE_FOR_TRADING, data["qab"].GetString());
              info.insert(CCAPI_EM_QUOTE_ASSET_QUANTITY_TOTAL, data["qtb"].GetString());
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        }
      } else if (m == "sub") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        message.setType(Message::Type::SUBSCRIPTION_STARTED);
        Element element;
        element.insert(CCAPI_INFO_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      } else if (m == "auth") {
        rj::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("sub").Move(), allocator);
        document.AddMember("ch", rj::Value("order:cash").Move(), allocator);
        rj::StringBuffer stringBufferSubscribe;
        rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
        document.Accept(writerSubscribe);
        std::string sendString = stringBufferSubscribe.GetString();
        ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
        this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
        this->send(wsConnectionPtr, sendString, ec);
#endif
#else
        this->send(wsConnectionPtr, sendString, ec);
#endif
        if (ec) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
        }
      } else if (m == "connected") {
        std::string type = document["type"].GetString();
        if (type == "unauth") {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          ExecutionManagementService::onOpen(hdl);
#else
          ExecutionManagementService::onOpen(wsConnectionPtr);
#endif
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string apiAccountGroupName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_ASCENDEX_H_
