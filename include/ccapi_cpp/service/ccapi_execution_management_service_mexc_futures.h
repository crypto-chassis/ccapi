#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_MEXC_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_MEXC_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceMexcFutures : public ExecutionManagementService {
 public:
  ExecutionManagementServiceMexcFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                        ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_MEXC_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
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
    this->apiKeyName = CCAPI_MEXC_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_MEXC_FUTURES_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/api/v1/private/order/submit";
    this->cancelOrderTarget = "/api/v1/private/order/cancel";
    this->cancelOrderWithExternalOidTarget = "/api/v1/private/order/cancel_with_external";
    this->getOrderTarget = "/api/v1/private/order/get/{order_id}";
    this->getOrderWithExternalOidTarget = "/api/v1/private/order/external/{symbol}/{external_oid}";
    this->getOpenOrdersTarget = "/api/v1/private/order/list/open_orders/{symbol}";
    this->cancelOpenOrdersTarget = "/api/v1/private/order/cancel_all";
    this->getAccountBalancesTarget = "/api/v1/private/account/assets";
    this->getAccountPositionsTarget = "/api/v1/private/position/open_positions";
  }
  virtual ~ExecutionManagementServiceMexcFutures() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"method":"ping"})", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"method":"ping"})", ec);
  }
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"0\"")); }
  void createSignature(std::string& signature, std::string& queryString, const std::string& reqMethod, const std::string& host, const std::string& path,
                       const std::map<std::string, std::string>& queryParamMap, const std::map<std::string, std::string>& credential) {
    std::string preSignedText;
    preSignedText += reqMethod;
    preSignedText += "\n";
    preSignedText += host;
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    int i = 0;
    for (const auto& kv : queryParamMap) {
      queryString += kv.first;
      queryString += "=";
      queryString += kv.second;
      if (i < queryParamMap.size() - 1) {
        queryString += "&";
      }
      i++;
    }
    preSignedText += queryString;
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
  }
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    std::map<std::string, std::string> queryParamMap;
    if (!queryString.empty()) {
      for (const auto& x : UtilString::split(queryString, "&")) {
        auto y = UtilString::split(x, "=");
        queryParamMap.insert(std::make_pair(y.at(0), y.at(1)));
      }
    }
    std::string signature;
    this->createSignature(signature, queryString, methodString, this->hostRest, path, queryParamMap, credential);
    if (!queryString.empty()) {
      queryString += "&";
    }
    queryString += "Signature=";
    queryString += Url::urlEncode(signature);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::map<std::string, std::string>& queryParamMap,
                   const std::map<std::string, std::string>& credential) {
    std::string signature;
    std::string queryString;
    this->createSignature(signature, queryString, std::string(req.method_string()), this->hostRest, path, queryParamMap, credential);
    queryString += "&Signature=";
    queryString += Url::urlEncode(signature);
    req.target(path + "?" + queryString);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& paramString, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = apiKey;
    preSignedText += req.base().at("Request-Time").to_string();
    preSignedText += paramString;
    CCAPI_LOGGER_TRACE("preSignedText = " + preSignedText);
    CCAPI_LOGGER_TRACE("apiSecret = " + apiSecret);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("Signature", signature);
  }
  void appendParam(Request::Operation operation, rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "vol"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "externalOid"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_EM_ORDER_LEVERAGE, "leverage"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == CCAPI_EM_ORDER_SIDE || key == CCAPI_EM_ORDER_QUANTITY || key == CCAPI_EM_ORDER_LEVERAGE || key == "vol" || key == "vol" || key == "vol" ||
          key == "leverage" || key == "side" || key == "type" || key == "openType" || key == "positionId" || key == "positionMode") {
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), std::stoll(value), allocator);
      } else if (key == "reduceOnly") {
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
      } else {
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
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
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("ApiKey", apiKey);
    std::string timeStr = UtilTime::getISOTimestamp(now);
    req.set("Request-Time", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(operation, document, allocator, param);
        if (param.find("type") == param.end()) {
          document.AddMember("type", rj::Value("1").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        const auto& marginType = request.getMarginType();
        if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
          document.AddMember("openType", 2, allocator);
        } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
          document.AddMember("openType", 1, allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, credential);
      } break;
      // case Request::Operation::CANCEL_ORDER: {
      //   req.method(http::verb::post);
      //   req.target(this->cancelOrderTarget);
      //   const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
      //   rj::Document document;
      //   document.SetObject();
      //   rj::Document::AllocatorType& allocator = document.GetAllocator();
      //   this->appendParam(operation, document, allocator, param);
      //   if (!symbolId.empty()) {
      //     this->appendSymbolId(document, allocator, symbolId);
      //   }
      //   rj::StringBuffer stringBuffer;
      //   rj::Writer<rj::StringBuffer> writer(stringBuffer);
      //   document.Accept(writer);
      //   auto body = stringBuffer.GetString();
      //   this->signRequest(req, body, credential);
      // } break;
      // case Request::Operation::GET_ORDER: {
      //   req.method(http::verb::get);
      //   std::string queryString;
      //   const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
      //   this->appendParam(queryString, param,
      //                     {
      //                         {CCAPI_EM_ORDER_ID, "ordId"},
      //                         {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"},
      //                         {CCAPI_SYMBOL_ID, "symbol"},
      //                     });
      //   if (!symbolId.empty()) {
      //     this->appendSymbolId(queryString, symbolId);
      //   }
      //   if (queryString.back() == '&') {
      //     queryString.pop_back();
      //   }
      //   req.target(this->getOrderTarget + "?" + queryString);
      //   this->signRequest(req, "", credential);
      // } break;
      // case Request::Operation::GET_OPEN_ORDERS: {
      //   req.method(http::verb::get);
      //   std::string queryString;
      //   const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
      //   this->appendParam(queryString, param,
      //                     {
      //                         {CCAPI_EM_ORDER_TYPE, "ordType"},
      //                         {CCAPI_SYMBOL_ID, "symbol"},
      //                     });
      //   if (!symbolId.empty()) {
      //     this->appendSymbolId(queryString, symbolId);
      //   }
      //   if (queryString.back() == '&') {
      //     queryString.pop_back();
      //   }
      //   req.target(queryString.empty() ? this->getOpenOrdersTarget : this->getOpenOrdersTarget + "?" + queryString);
      //   this->signRequest(req, "", credential);
      // } break;
      // case Request::Operation::GET_ACCOUNT_BALANCES: {
      //   req.method(http::verb::get);
      //   req.target(this->getAccountBalancesTarget);
      //   this->signRequest(req, "", credential);
      // } break;
      // case Request::Operation::GET_ACCOUNT_POSITIONS: {
      //   req.method(http::verb::get);
      //   req.target(this->getAccountPositionsTarget);
      //   this->signRequest(req, "", credential);
      // } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    // this->extractOrderInfoFromRequest(elementList, document);
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const rj::Document& document) {
    // const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
    //     {CCAPI_EM_ORDER_ID, std::make_pair("ordId", JsonDataType::STRING)},
    //     {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdId", JsonDataType::STRING)},
    //     {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
    //     {CCAPI_EM_ORDER_QUANTITY, std::make_pair("sz", JsonDataType::STRING)},
    //     {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("px", JsonDataType::STRING)},
    //     {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
    //     {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
    //     {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    // const rj::Value& data = document["data"];
    // if (data.IsObject()) {
    //   Element element;
    //   this->extractOrderInfo(element, data, extractionFieldNameMap);
    //   elementList.emplace_back(std::move(element));
    // } else {
    //   for (const auto& x : data.GetArray()) {
    //     Element element;
    //     this->extractOrderInfo(element, x, extractionFieldNameMap);
    //     elementList.emplace_back(std::move(element));
    //   }
    // }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    // switch (request.getOperation()) {
    //   case Request::Operation::GET_ACCOUNT_BALANCES: {
    //     for (const auto& x : document["data"][0]["details"].GetArray()) {
    //       Element element;
    //       element.insert(CCAPI_EM_ASSET, x["ccy"].GetString());
    //       std::string availEq = x["availEq"].GetString();
    //       element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, availEq.empty() ? x["availBal"].GetString() : availEq);
    //       element.insert(CCAPI_EM_QUANTITY_TOTAL, x["eq"].GetString());
    //       elementList.emplace_back(std::move(element));
    //     }
    //   } break;
    //   case Request::Operation::GET_ACCOUNT_POSITIONS: {
    //     for (const auto& x : document["data"].GetArray()) {
    //       Element element;
    //       element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    //       element.insert(CCAPI_EM_POSITION_SIDE, x["posSide"].GetString());
    //       std::string availPos = x["availPos"].GetString();
    //       std::string positionQuantity = availPos.empty() ? x["pos"].GetString() : availPos;
    //       element.insert(CCAPI_EM_POSITION_QUANTITY, positionQuantity);
    //       element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgPx"].GetString());
    //       element.insert(CCAPI_EM_POSITION_LEVERAGE, x["lever"].GetString());
    //       elementList.emplace_back(std::move(element));
    //     }
    //   } break;
    //   default:
    //     CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    // }
  }
  void extractOrderInfo(Element& element, const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap,
                        const std::map<std::string, std::function<std::string(const std::string&)>> conversionMap = {}) override {
    // ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
    // {
    //   auto it1 = x.FindMember("accFillSz");
    //   auto it2 = x.FindMember("avgPx");
    //   if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
    //     auto it1Str = std::string(it1->value.GetString());
    //     auto it2Str = std::string(it2->value.GetString());
    //     if (!it1Str.empty() && !it2Str.empty()) {
    //       element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, Decimal(UtilString::printDoubleScientific(std::stod(it1Str) *
    //       std::stod(it2Str))).toString());
    //     }
    //   }
    // }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    // rj::Document document;
    // document.SetObject();
    // auto& allocator = document.GetAllocator();
    // document.AddMember("op", rj::Value("login").Move(), allocator);
    // rj::Value arg(rj::kObjectType);
    // auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    // auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    // auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    // std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    // arg.AddMember("apiKey", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    // arg.AddMember("passphrase", rj::Value(apiPassphrase.c_str(), allocator).Move(), allocator);
    // arg.AddMember("timestamp", rj::Value(ts.c_str(), allocator).Move(), allocator);
    // std::string signData = ts + "GET" + "/users/self/verify";
    // std::string sign = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData));
    // arg.AddMember("sign", rj::Value(sign.c_str(), allocator).Move(), allocator);
    // rj::Value args(rj::kArrayType);
    // args.PushBack(arg, allocator);
    // document.AddMember("args", args, allocator);
    // rj::StringBuffer stringBuffer;
    // rj::Writer<rj::StringBuffer> writer(stringBuffer);
    // document.Accept(writer);
    // std::string sendString = stringBuffer.GetString();
    // sendStringList.push_back(sendString);
    return sendStringList;
  }
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    // if (textMessage != "pong") {
    //   rj::Document document;
    //   document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    //   auto it = document.FindMember("event");
    //   std::string eventStr = it != document.MemberEnd() ? it->value.GetString() : "";
    //   if (eventStr == "login") {
    //     rj::Document document;
    //     document.SetObject();
    //     auto& allocator = document.GetAllocator();
    //     document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    //     rj::Value args(rj::kArrayType);
    //     const auto& fieldSet = subscription.getFieldSet();
    //     const auto& instrumentSet = subscription.getInstrumentSet();
    //     for (const auto& field : fieldSet) {
    //       std::string channel;
    //       if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
    //         channel = "orders";
    //       }
    //       for (const auto& instrument : instrumentSet) {
    //         rj::Value arg(rj::kObjectType);
    //         arg.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
    //         arg.AddMember("symbol", rj::Value(instrument.c_str(), allocator).Move(), allocator);
    //         arg.AddMember("instType", rj::Value("ANY").Move(), allocator);
    //         args.PushBack(arg, allocator);
    //       }
    //     }
    //     document.AddMember("args", args, allocator);
    //     rj::StringBuffer stringBufferSubscribe;
    //     rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
    //     document.Accept(writerSubscribe);
    //     std::string sendString = stringBufferSubscribe.GetString();
    //     ErrorCode ec;
    //     #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
this->send(wsConnectionPtr, sendString, ec);
#endif
    //     if (ec) {
    //       this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
    //     }
    //   } else {
    //     Event event = this->createEvent(subscription, textMessage, document, eventStr, timeReceived);
    //     if (!event.getMessageList().empty()) {
    //       this->eventHandler(event, nullptr);
    //     }
    //   }
    // }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& eventStr,
                    const TimePoint& timeReceived) {
    Event event;
    // std::vector<Message> messageList;
    // Message message;
    // message.setTimeReceived(timeReceived);
    // const auto& correlationId = subscription.getCorrelationId();
    // message.setCorrelationIdList({correlationId});
    // const auto& fieldSet = subscription.getFieldSet();
    // const auto& instrumentSet = subscription.getInstrumentSet();
    // if (eventStr.empty()) {
    //   auto it = document.FindMember("op");
    //   std::string op = it != document.MemberEnd() ? it->value.GetString() : "";
    //   if (op == "order" || op == "cancel-order") {
    //     event.setType(Event::Type::RESPONSE);
    //     std::string code = document["code"].GetString();
    //     if (code != "0") {
    //       message.setType(Message::Type::RESPONSE_ERROR);
    //       Element element;
    //       element.insert(CCAPI_ERROR_MESSAGE, textMessage);
    //       message.setElementList({element});
    //       message.setSecondaryCorrelationIdMap({
    //           {correlationId, document["id"].GetString()},
    //       });
    //       messageList.emplace_back(std::move(message));
    //     } else {
    //       std::vector<Element> elementList;
    //       if (op == "order") {
    //         message.setType(Message::Type::CREATE_ORDER);
    //       } else if (op == "cancel-order") {
    //         message.setType(Message::Type::CANCEL_ORDER);
    //       }
    //       this->extractOrderInfoFromRequest(elementList, document);
    //       message.setElementList(elementList);
    //       message.setSecondaryCorrelationIdMap({
    //           {correlationId, document["id"].GetString()},
    //       });
    //       messageList.emplace_back(std::move(message));
    //     }
    //   } else {
    //     const rj::Value& arg = document["arg"];
    //     const rj::Value& data = document["data"];
    //     std::string channel = std::string(arg["channel"].GetString());
    //     event.setType(Event::Type::SUBSCRIPTION_DATA);
    //     std::string instrument = arg["symbol"].GetString();
    //     if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
    //       if (channel == "orders") {
    //         if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
    //           for (const auto& x : data.GetArray()) {
    //             std::string tradeId = x["tradeId"].GetString();
    //             if (!tradeId.empty()) {
    //               Message message;
    //               message.setTimeReceived(timeReceived);
    //               message.setCorrelationIdList({subscription.getCorrelationId()});
    //               message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["fillTime"].GetString()))));
    //               message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
    //               std::vector<Element> elementList;
    //               Element element;
    //               element.insert(CCAPI_TRADE_ID, tradeId);
    //               element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["fillPx"].GetString()));
    //               element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["fillSz"].GetString()));
    //               element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
    //               element.insert(CCAPI_EM_POSITION_SIDE, std::string(x["posSide"].GetString()));
    //               element.insert(CCAPI_IS_MAKER, std::string(x["execType"].GetString()) == "M" ? "1" : "0");
    //               element.insert(CCAPI_EM_ORDER_ID, std::string(x["ordId"].GetString()));
    //               element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(x["clOrdId"].GetString()));
    //               element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
    //               element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fillFee"].GetString()));
    //               element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fillFeeCcy"].GetString()));
    //               elementList.emplace_back(std::move(element));
    //               message.setElementList(elementList);
    //               messageList.emplace_back(std::move(message));
    //             }
    //           }
    //         }
    //         if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
    //           for (const auto& x : data.GetArray()) {
    //             Message message;
    //             message.setTimeReceived(timeReceived);
    //             message.setCorrelationIdList({subscription.getCorrelationId()});
    //             message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["uTime"].GetString()))));
    //             message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
    //             const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
    //                 {CCAPI_EM_ORDER_ID, std::make_pair("ordId", JsonDataType::STRING)},
    //                 {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdId", JsonDataType::STRING)},
    //                 {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
    //                 {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("px", JsonDataType::STRING)},
    //                 {CCAPI_EM_ORDER_QUANTITY, std::make_pair("sz", JsonDataType::STRING)},
    //                 {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
    //                 {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
    //             };
    //             Element info;
    //             this->extractOrderInfo(info, x, extractionFieldNameMap);
    //             info.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
    //             std::vector<Element> elementList;
    //             elementList.emplace_back(std::move(info));
    //             message.setElementList(elementList);
    //             messageList.emplace_back(std::move(message));
    //           }
    //         }
    //       }
    //     }
    //   }
    // } else if (eventStr == "subscribe") {
    //   event.setType(Event::Type::SUBSCRIPTION_STATUS);
    //   message.setType(Message::Type::SUBSCRIPTION_STARTED);
    //   Element element;
    //   element.insert(CCAPI_INFO_MESSAGE, textMessage);
    //   message.setElementList({element});
    //   messageList.emplace_back(std::move(message));
    // } else if (eventStr == "error") {
    //   event.setType(Event::Type::SUBSCRIPTION_STATUS);
    //   message.setType(Message::Type::SUBSCRIPTION_FAILURE);
    //   Element element;
    //   element.insert(CCAPI_ERROR_MESSAGE, textMessage);
    //   message.setElementList({element});
    //   messageList.emplace_back(std::move(message));
    // }
    // event.setMessageList(messageList);
    return event;
  }
  std::string cancelOrderWithExternalOidTarget, getOrderWithExternalOidTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_MEXC_FUTURES_H_
