#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_CRYPTOCOM_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_CRYPTOCOM_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceCryptocom : public ExecutionManagementService {
 public:
  ExecutionManagementServiceCryptocom(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_CRYPTOCOM;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v2/user";
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
    this->apiKeyName = CCAPI_CRYPTOCOM_API_KEY;
    this->apiSecretName = CCAPI_CRYPTOCOM_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->pathPrefix = "/v2/";
    this->createOrderMethod = "private/create-order";
    this->cancelOrderMethod = "private/cancel-order";
    this->getOrderMethod = "private/get-order-detail";
    this->getOpenOrdersMethod = "private/get-open-orders";
    this->cancelOpenOrdersMethod = "private/cancel-all-orders";
    this->getAccountBalancesMethod = "private/get-account-summary";
  }
  virtual ~ExecutionManagementServiceCryptocom() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& httpMethodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    document.AddMember("api_key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    std::string paramString;
    auto itParams = document.FindMember("params");
    if (itParams != document.MemberEnd()) {
      const rj::Value& params = itParams->value;
      std::map<std::string, std::string> sortedParams;
      for (auto itr = params.MemberBegin(); itr != params.MemberEnd(); ++itr) {
        sortedParams.insert({itr->name.GetString(), itr->value.GetString()});
      }
      for (const auto& x : sortedParams) {
        paramString += x.first;
        paramString += x.second;
      }
    }
    std::string appMethod = document["method"].GetString();
    std::string preSignedText(appMethod);
    auto itId = document.FindMember("id");
    if (itId != document.MemberEnd()) {
      preSignedText += itId->value.GetString();
    }
    preSignedText += apiKey;
    preSignedText += document["nonce"].GetString();
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    document.AddMember("sig", rj::Value(signature.c_str(), allocator).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    body = stringBuffer.GetString();
  }
  void signRequest(http::request<http::string_body>& req, rj::Document& document, rj::Document::AllocatorType& allocator, int64_t requestId,
                   const std::string& appMethod, const std::map<std::string, std::string>& param, const TimePoint& now,
                   const std::map<std::string, std::string>& credential) {
    std::string body;
    this->signRequest(body, document, allocator, requestId, appMethod, param, now, credential);
    req.body() = body;
    req.prepare_payload();
    req.target(this->pathPrefix + appMethod);
  }
  void signRequest(std::string& body, rj::Document& document, rj::Document::AllocatorType& allocator, int64_t requestId, const std::string& appMethod,
                   const std::map<std::string, std::string>& param, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    document.AddMember("api_key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    document.AddMember("nonce", rj::Value(nonce).Move(), allocator);
    std::string paramsString;
    for (const auto& kv : param) {
      paramsString += kv.first;
      paramsString += kv.second;
    }
    std::string preSignedText(appMethod);
    preSignedText += std::to_string(requestId);
    preSignedText += apiKey;
    preSignedText += paramsString;
    preSignedText += std::to_string(nonce);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    document.AddMember("sig", rj::Value(signature.c_str(), allocator).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    body = stringBuffer.GetString();
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, int64_t requestId, const std::string& appMethod,
                   const std::map<std::string, std::string>& param) {
    document.AddMember("id", rj::Value(requestId).Move(), allocator);
    document.AddMember("method", rj::Value(appMethod.c_str(), allocator).Move(), allocator);
    rj::Value params(rj::kObjectType);
    for (const auto& kv : param) {
      auto key = kv.first;
      auto value = kv.second;
      if (value != "null") {
        if (value == "true" || value == "false") {
          params.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          params.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
    }
    document.AddMember("params", params, allocator);
  }
  void prepareReq(http::request<http::string_body>& req, const std::map<std::string, std::string>& param, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential, const std::string& appMethod,
                  const std::map<std::string, std::string> standardizationMap = {}) {
    req.set(beast::http::field::content_type, "application/json");
    req.method(http::verb::post);
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    std::map<std::string, std::string> paramCopy;
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      paramCopy.insert({key, value});
    }
    if (appMethod == this->createOrderMethod) {
      if (paramCopy.find("type") == paramCopy.end()) {
        paramCopy.insert({"type", "LIMIT"});
      }
    }
    if (!symbolId.empty()) {
      paramCopy.insert({"instrument_name", symbolId});
    }
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    this->appendParam(document, allocator, requestId, appMethod, paramCopy);
    this->signRequest(req, document, allocator, requestId, appMethod, paramCopy, now, credential);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->createOrderMethod,
                         {
                             {CCAPI_EM_CLIENT_ORDER_ID, "client_oid"},
                             {CCAPI_EM_ORDER_SIDE, "side"},
                             {CCAPI_EM_ORDER_QUANTITY, "quantity"},
                             {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                         });
      } break;
      case Request::Operation::CANCEL_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->cancelOrderMethod,
                         {
                             {CCAPI_EM_ORDER_ID, "order_id"},
                         });
      } break;
      case Request::Operation::GET_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getOrderMethod,
                         {
                             {CCAPI_EM_ORDER_ID, "order_id"},
                         });
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getOpenOrdersMethod);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->cancelOpenOrdersMethod);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->prepareReq(req, param, now, symbolId, credential, this->getAccountBalancesMethod,
                         {
                             {CCAPI_EM_ASSET, "currency"},
                         });
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, const WsConnection& wsConnection, const Request& request,
                                  int wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        std::map<std::string, std::string> paramCopy;
        std::map<std::string, std::string> standardizationMap = {
            {CCAPI_EM_CLIENT_ORDER_ID, "client_oid"},
            {CCAPI_EM_ORDER_SIDE, "side"},
            {CCAPI_EM_ORDER_QUANTITY, "quantity"},
            {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
        };
        for (const auto& kv : param) {
          auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
          auto value = kv.second;
          paramCopy.insert({key, value});
        }
        if (paramCopy.find("type") == paramCopy.end()) {
          paramCopy.insert({"type", "LIMIT"});
        }
        if (!symbolId.empty()) {
          paramCopy.insert({"instrument_name", symbolId});
        }
        this->appendParam(document, allocator, wsRequestId, this->createOrderMethod, paramCopy);
        int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        document.AddMember("nonce", rj::Value(nonce).Move(), allocator);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        std::map<std::string, std::string> paramCopy;
        std::map<std::string, std::string> standardizationMap = {
            {CCAPI_EM_ORDER_ID, "order_id"},
        };
        for (const auto& kv : param) {
          auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
          auto value = kv.second;
          paramCopy.insert({key, value});
        }
        if (!symbolId.empty()) {
          paramCopy.insert({"instrument_name", symbolId});
        }
        this->appendParam(document, allocator, wsRequestId, this->cancelOrderMethod, paramCopy);
        int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        document.AddMember("nonce", rj::Value(nonce).Move(), allocator);
      } break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnection, request, wsRequestId, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    this->extractOrderInfoFromRequest(elementList, operation, document);
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request::Operation operation, const rj::Document& document) {
    const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_oid", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("quantity", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumulative_quantity", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("cumulative_value", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instrument_name", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::CREATE_ORDER) {
      Element element;
      this->extractOrderInfo(element, document["result"], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_ORDER) {
      Element element;
      this->extractOrderInfo(element, document["result"]["order_info"], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : document["result"]["order_list"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      Element element;
      elementList.emplace_back(std::move(element));
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        for (const auto& x : document["result"]["accounts"].GetArray()) {
          if (std::string(x["balance"].GetString()) == "0") {
            continue;
          }
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["balance"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    std::string appMethod = "public/auth";
    std::map<std::string, std::string> param;
    this->appendParam(document, allocator, requestId, appMethod, param);
    std::string body;
    this->signRequest(body, document, allocator, requestId, appMethod, param, now, credential);

    sendStringList.push_back(body);
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
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    std::string method = document["method"].GetString();
    auto itCode = document.FindMember("code");
    auto itResult = document.FindMember("result");
    if (method == "subscribe") {
      if (itResult == document.MemberEnd()) {
        if (itCode != document.MemberEnd()) {
          std::string code = itCode->value.GetString();
          if (code != "0") {
            event.setType(Event::Type::SUBSCRIPTION_STATUS);
            message.setType(Message::Type::SUBSCRIPTION_FAILURE);
            Element element;
            element.insert(CCAPI_ERROR_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          } else {
            event.setType(Event::Type::SUBSCRIPTION_STATUS);
            message.setType(Message::Type::SUBSCRIPTION_STARTED);
            Element element;
            element.insert(CCAPI_INFO_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          }
        }
      } else {
        const rj::Value& result = itResult->value;
        std::string exchangeSubscriptionId = result["subscription"].GetString();
        std::smatch match;
        std::string instrument;
        std::string field;
        if (std::regex_search(exchangeSubscriptionId, match, std::regex("user\\.order\\.(.+)"))) {
          instrument = match[1].str();
          field = CCAPI_EM_ORDER_UPDATE;
        } else if (std::regex_search(exchangeSubscriptionId, match, std::regex("user\\.trade\\.(.+)"))) {
          instrument = match[1].str();
          field = CCAPI_EM_PRIVATE_TRADE;
        }
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          if (field == CCAPI_EM_PRIVATE_TRADE && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            for (const auto& x : result["data"].GetArray()) {
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
              message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["create_time"].GetString()))));
              std::vector<Element> elementList;
              Element element;
              element.insert(CCAPI_TRADE_ID, std::string(x["trade_id"].GetString()));
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["traded_price"].GetString()));
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["traded_quantity"].GetString()));
              element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "BUY" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, std::string(x["liquidity_indicator"].GetString()) == "MAKER" ? "1" : "0");
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
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_oid", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("quantity", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumulative_quantity", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("cumulative_value", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instrument_name", JsonDataType::STRING)},
            };
            for (const auto& x : result["data"].GetArray()) {
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["update_time"].GetString()))));
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        }
      }
    } else if (method == "private/create-order" || method == "private/cancel-order") {
      if (itCode != document.MemberEnd()) {
        event.setType(Event::Type::RESPONSE);
        std::string code = itCode->value.GetString();
        if (code != "0") {
          message.setType(Message::Type::RESPONSE_ERROR);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
        } else {
          std::vector<Element> elementList;
          Request::Operation operation;
          if (method == "private/create-order") {
            message.setType(Message::Type::CREATE_ORDER);
            operation = Request::Operation::CREATE_ORDER;
          } else if (method == "private/cancel-order") {
            message.setType(Message::Type::CANCEL_ORDER);
            operation = Request::Operation::CANCEL_ORDER;
          }
          this->extractOrderInfoFromRequest(elementList, operation, document);
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    } else if (method == "public/heartbeat") {
      std::string id = document["id"].GetString();
      std::string msg = R"({"id":)" + id + R"(,"method":"public/respond-heartbeat"})";
      ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      this->send(wsConnection.hdl, msg, wspp::frame::opcode::text, ec);
#else
      this->send(wsConnectionPtr, msg, ec);
#endif
      if (ec) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
      }
    } else if (method == "public/auth") {
      if (itCode != document.MemberEnd()) {
        std::string code = itCode->value.GetString();
        if (code != "0") {
          event.setType(Event::Type::AUTHORIZATION_STATUS);
          message.setType(Message::Type::AUTHORIZATION_FAILURE);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
        } else {
          event.setType(Event::Type::AUTHORIZATION_STATUS);
          message.setType(Message::Type::AUTHORIZATION_SUCCESS);
          Element element;
          element.insert(CCAPI_INFO_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(timeReceived.time_since_epoch()).count();
          document.AddMember("id", rj::Value(requestId).Move(), allocator);
          document.AddMember("method", rj::Value("subscribe").Move(), allocator);
          rj::Value channels(rj::kArrayType);
          if (instrumentSet.empty()) {
            for (const auto& field : fieldSet) {
              std::string exchangeSubscriptionId;
              if (field == CCAPI_EM_ORDER_UPDATE) {
                exchangeSubscriptionId = "user.order.";
              } else if (field == CCAPI_EM_PRIVATE_TRADE) {
                exchangeSubscriptionId = "user.trade.";
              }
              channels.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
            }
          } else {
            for (const auto& instrument : instrumentSet) {
              for (const auto& field : fieldSet) {
                std::string exchangeSubscriptionId;
                if (field == CCAPI_EM_ORDER_UPDATE) {
                  exchangeSubscriptionId = "user.order." + instrument;
                } else if (field == CCAPI_EM_PRIVATE_TRADE) {
                  exchangeSubscriptionId = "user.trade." + instrument;
                }
                channels.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
              }
            }
          }
          rj::Value params(rj::kObjectType);
          params.AddMember("channels", channels, allocator);
          document.AddMember("params", params, allocator);
          int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(timeReceived.time_since_epoch()).count();
          document.AddMember("nonce", rj::Value(nonce).Move(), allocator);
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
          this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
          this->send(wsConnectionPtr, sendString, ec);
#endif
          if (ec) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
          }
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string pathPrefix;
  std::string createOrderMethod;
  std::string cancelOrderMethod;
  std::string getOrderMethod;
  std::string getOpenOrdersMethod;
  std::string cancelOpenOrdersMethod;
  std::string getAccountBalancesMethod;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_CRYPTOCOM_H_
