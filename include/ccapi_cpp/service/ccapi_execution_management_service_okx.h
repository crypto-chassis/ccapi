#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"

namespace ccapi {
class ExecutionManagementServiceOkx : public ExecutionManagementService {
 public:
  ExecutionManagementServiceOkx(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_OKX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + CCAPI_OKX_PRIVATE_WS_PATH;
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
    this->apiKeyName = CCAPI_OKX_API_KEY;
    this->apiSecretName = CCAPI_OKX_API_SECRET;
    this->apiPassphraseName = CCAPI_OKX_API_PASSPHRASE;
    this->apiXSimulatedTradingName = CCAPI_OKX_API_X_SIMULATED_TRADING;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName, this->apiXSimulatedTradingName});
    this->createOrderTarget = "/api/v5/trade/order";
    this->cancelOrderTarget = "/api/v5/trade/cancel-order";
    this->getOrderTarget = "/api/v5/trade/order";
    this->getOpenOrdersTarget = "/api/v5/trade/orders-pending";
    this->getAccountBalancesTarget = "/api/v5/account/balance";
    this->getAccountPositionsTarget = "/api/v5/account/positions";
  }

  virtual ~ExecutionManagementServiceOkx() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, "ping", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"0\"")); }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("OK-ACCESS-TIMESTAMP").to_string();
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
    headerString += "OK-ACCESS-SIGN:" + signature;
  }

  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("OK-ACCESS-TIMESTAMP").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("OK-ACCESS-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }

  void appendParam(Request::Operation operation, rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "sz"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "px"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"},
                       {CCAPI_SYMBOL_ID, "instId"},
                       {CCAPI_EM_ORDER_ID, "ordId"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
    if (operation == Request::Operation::CREATE_ORDER && param.find("tag") == param.end()) {
      rjValue.AddMember("tag", CCAPI_OKX_API_BROKER_CODE, allocator);
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
    rjValue.AddMember("instId", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }

  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "instId=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("OK-ACCESS-KEY", apiKey);
    std::string timeStr = UtilTime::getISOTimestamp<std::chrono::milliseconds>(now);
    req.set("OK-ACCESS-TIMESTAMP", timeStr);
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    req.set("OK-ACCESS-PASSPHRASE", apiPassphrase);
    auto apiXSimulatedTrading = mapGetWithDefault(credential, this->apiXSimulatedTradingName);
    if (apiXSimulatedTrading == "1") {
      req.set("x-simulated-trading", "1");
    }
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
        if (param.find("tdMode") == param.end()) {
          document.AddMember("tdMode", rj::Value("cash").Move(), allocator);
        }
        if (param.find("ordType") == param.end()) {
          document.AddMember("ordType", rj::Value("limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.target(this->cancelOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(operation, document, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "ordId"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"},
                              {CCAPI_SYMBOL_ID, "instId"},
                          });
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_TYPE, "ordType"},
                              {CCAPI_SYMBOL_ID, "instId"},
                          });
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(queryString.empty() ? this->getOpenOrdersTarget : this->getOpenOrdersTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        req.target(this->getAccountBalancesTarget);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        req.target(this->getAccountPositionsTarget);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, const WsConnection& wsConnection, const Request& request,
                                  int wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    document.SetObject();
    const auto& secondaryCorrelationId = request.getSecondaryCorrelationId();
    document.AddMember("id", rj::Value((secondaryCorrelationId.empty() ? std::to_string(wsRequestId) : secondaryCorrelationId).c_str(), allocator).Move(),
                       allocator);
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER: {
        document.AddMember("op", rj::Value("order").Move(), allocator);
        rj::Value args(rj::kArrayType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value arg(rj::kObjectType);
        this->appendParam(operation, arg, allocator, param);
        if (param.find("tdMode") == param.end()) {
          arg.AddMember("tdMode", rj::Value("cash").Move(), allocator);
        }
        if (param.find("ordType") == param.end()) {
          arg.AddMember("ordType", rj::Value("limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(arg, allocator, symbolId);
        }
        args.PushBack(arg, allocator);
        document.AddMember("args", args, allocator);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        document.AddMember("op", rj::Value("cancel-order").Move(), allocator);
        rj::Value args(rj::kArrayType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value arg(rj::kObjectType);
        this->appendParam(operation, arg, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(arg, allocator, symbolId);
        }
        args.PushBack(arg, allocator);
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

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const rj::Document& document) {
    const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("ordId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("sz", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("px", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instId", JsonDataType::STRING)}};
    const rj::Value& data = document["data"];
    if (data.IsObject()) {
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else {
      for (const auto& x : data.GetArray()) {
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
        for (const auto& x : document["data"][0]["details"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["ccy"].GetString());
          std::string availEq = x["availEq"].GetString();
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, availEq.empty() ? x["availBal"].GetString() : availEq);
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["cashBal"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["posSide"].GetString());
          std::string availPos = x["availPos"].GetString();
          std::string positionQuantity = availPos.empty() ? x["pos"].GetString() : availPos;
          element.insert(CCAPI_EM_POSITION_QUANTITY, positionQuantity);
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgPx"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["lever"].GetString());
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
      auto it1 = x.FindMember("accFillSz");
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

  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("login").Move(), allocator);
    rj::Value arg(rj::kObjectType);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    arg.AddMember("apiKey", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    arg.AddMember("passphrase", rj::Value(apiPassphrase.c_str(), allocator).Move(), allocator);
    arg.AddMember("timestamp", rj::Value(ts.c_str(), allocator).Move(), allocator);
    std::string signData = ts + "GET" + "/users/self/verify";
    std::string sign = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData));
    arg.AddMember("sign", rj::Value(sign.c_str(), allocator).Move(), allocator);
    rj::Value args(rj::kArrayType);
    args.PushBack(arg, allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }

  void onTextMessage(
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage
#else
      std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView
#endif
      ,
      const TimePoint& timeReceived) override {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
    std::string textMessage(textMessageView);
#endif
    if (textMessage != "pong") {
      rj::Document document;
      document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
      auto it = document.FindMember("event");
      std::string eventStr = it != document.MemberEnd() ? it->value.GetString() : "";
      if (eventStr == "login") {
        rj::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("subscribe").Move(), allocator);
        rj::Value args(rj::kArrayType);
        const auto& fieldSet = subscription.getFieldSet();
        const auto& instrumentSet = subscription.getInstrumentSet();
        if (instrumentSet.empty()) {
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            rj::Value arg(rj::kObjectType);
            arg.AddMember("channel", rj::Value("orders").Move(), allocator);
            arg.AddMember("instType", rj::Value("ANY").Move(), allocator);
            args.PushBack(arg, allocator);
          }
          if (fieldSet.find(CCAPI_EM_POSITION_UPDATE) != fieldSet.end()) {
            rj::Value arg(rj::kObjectType);
            arg.AddMember("channel", rj::Value("positions").Move(), allocator);
            arg.AddMember("instType", rj::Value("ANY").Move(), allocator);
            args.PushBack(arg, allocator);
          }
        } else {
          for (const auto& instrument : instrumentSet) {
            if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
              rj::Value arg(rj::kObjectType);
              arg.AddMember("channel", rj::Value("orders").Move(), allocator);
              arg.AddMember("instId", rj::Value(instrument.c_str(), allocator).Move(), allocator);
              arg.AddMember("instType", rj::Value("ANY").Move(), allocator);
              args.PushBack(arg, allocator);
            }
            if (fieldSet.find(CCAPI_EM_POSITION_UPDATE) != fieldSet.end()) {
              rj::Value arg(rj::kObjectType);
              arg.AddMember("channel", rj::Value("positions").Move(), allocator);
              arg.AddMember("instId", rj::Value(instrument.c_str(), allocator).Move(), allocator);
              arg.AddMember("instType", rj::Value("ANY").Move(), allocator);
              args.PushBack(arg, allocator);
            }
          }
        }
        if (fieldSet.find(CCAPI_EM_BALANCE_UPDATE) != fieldSet.end()) {
          rj::Value arg(rj::kObjectType);
          arg.AddMember("channel", rj::Value("balance_and_position").Move(), allocator);
          args.PushBack(arg, allocator);
        }
        document.AddMember("args", args, allocator);
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
      } else {
        Event event = this->createEvent(subscription, textMessage, document, eventStr, timeReceived);
        if (!event.getMessageList().empty()) {
          this->eventHandler(event, nullptr);
        }
      }
    }
  }

  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& eventStr,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    const auto& correlationId = subscription.getCorrelationId();
    message.setCorrelationIdList({correlationId});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (eventStr.empty()) {
      auto it = document.FindMember("op");
      std::string op = it != document.MemberEnd() ? it->value.GetString() : "";
      if (op == "order" || op == "cancel-order") {
        event.setType(Event::Type::RESPONSE);
        std::string code = document["code"].GetString();
        if (code != "0") {
          message.setType(Message::Type::RESPONSE_ERROR);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          message.setSecondaryCorrelationIdMap({
              {correlationId, document["id"].GetString()},
          });
          messageList.emplace_back(std::move(message));
        } else {
          std::vector<Element> elementList;
          if (op == "order") {
            message.setType(Message::Type::CREATE_ORDER);
          } else if (op == "cancel-order") {
            message.setType(Message::Type::CANCEL_ORDER);
          }
          this->extractOrderInfoFromRequest(elementList, document);
          message.setElementList(elementList);
          message.setSecondaryCorrelationIdMap({
              {correlationId, document["id"].GetString()},
          });
          messageList.emplace_back(std::move(message));
        }
      } else {
        const rj::Value& arg = document["arg"];
        const rj::Value& data = document["data"];
        std::string channel = std::string(arg["channel"].GetString());
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        if (channel == "orders") {
          if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            for (const auto& x : data.GetArray()) {
              std::string tradeId = x["tradeId"].GetString();
              if (!tradeId.empty()) {
                Message message;
                message.setTimeReceived(timeReceived);
                message.setCorrelationIdList({subscription.getCorrelationId()});
                message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["fillTime"].GetString()))));
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                std::vector<Element> elementList;
                Element element;
                element.insert(CCAPI_TRADE_ID, tradeId);
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["fillPx"].GetString()));
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["fillSz"].GetString()));
                element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                element.insert(CCAPI_EM_POSITION_SIDE, std::string(x["posSide"].GetString()));
                element.insert(CCAPI_IS_MAKER, std::string(x["execType"].GetString()) == "M" ? "1" : "0");
                element.insert(CCAPI_EM_ORDER_ID, std::string(x["ordId"].GetString()));
                element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(x["clOrdId"].GetString()));
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, x["instId"].GetString());
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fillFee"].GetString()));
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fillFeeCcy"].GetString()));
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            }
          }
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
            for (const auto& x : data.GetArray()) {
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["uTime"].GetString()))));
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("ordId", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdId", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("px", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("sz", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, std::make_pair("avgPx", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FEE_QUANTITY, std::make_pair("fee", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_FEE_ASSET, std::make_pair("feeCcy", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instId", JsonDataType::STRING)},
              };
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        } else if (channel == "balance_and_position") {
          for (const auto& x : data.GetArray()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["pTime"].GetString()))));
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
            std::vector<Element> elementList;
            for (const auto& y : x["balData"].GetArray()) {
              Element element;
              element.insert(CCAPI_EM_ASSET, y["ccy"].GetString());
              element.insert(CCAPI_EM_QUANTITY_TOTAL, y["cashBal"].GetString());
              elementList.emplace_back(std::move(element));
            }
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        } else if (channel == "positions") {
          for (const auto& x : data.GetArray()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["uTime"].GetString()))));
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
            element.insert(CCAPI_EM_POSITION_SIDE, x["posSide"].GetString());
            element.insert(CCAPI_EM_POSITION_QUANTITY, x["pos"].GetString());
            element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgPx"].GetString());
            element.insert(CCAPI_EM_UNREALIZED_PNL, x["upl"].GetString());
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (eventStr == "subscribe") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else if (eventStr == "error") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    }
    event.setMessageList(messageList);
    return event;
  }

  std::string apiPassphraseName;
  std::string apiXSimulatedTradingName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKX_H_
