#pragma once

#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_UTA
#include "ccapi_cpp/service/ccapi_execution_management_service.h"

namespace ccapi {

class ExecutionManagementServiceBitgetUta : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBitgetUta(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITGET_UTA;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v3/ws/private";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->apiKeyName = CCAPI_BITGET_UTA_API_KEY;
    this->apiSecretName = CCAPI_BITGET_UTA_API_SECRET;
    this->apiPassphraseName = CCAPI_BITGET_UTA_API_PASSPHRASE;
    this->apiDemoTradingName = CCAPI_BITGET_UTA_API_DEMO_TRADING;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName, this->apiDemoTradingName});
    this->createOrderTarget = "/api/v3/trade/place-order";
    this->cancelOrderTarget = "/api/v3/trade/cancel-order";
    this->cancelOpenOrdersTarget = "/api/v3/trade/cancel-symbol-order";
    this->getOrderTarget = "/api/v3/trade/order-info";
    this->getOpenOrdersTarget = "/api/v3/trade/unfilled-orders";
    this->getAccountBalancesTarget = "/api/v3/account/assets";
    this->getAccountPositionsTarget = "/api/v3/position/current-position";
  }

  virtual ~ExecutionManagementServiceBitgetUta() {}
#ifndef CCAPI_EXPOSE_INTERNAL
 private:
#endif
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override {
    return !std::regex_search(bodyView.begin(), bodyView.end(), std::regex("\"code\":\\s*\"00000\""));
  }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = std::string(req.base().at("ACCESS-TIMESTAMP"));
    preSignedText += methodString;
    auto target = path;
    if (!queryString.empty()) {
      target += "?";
      target += queryString;
    }
    preSignedText += target;
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "ACCESS-SIGN:" + signature;
  }

  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = std::string(req.base().at("ACCESS-TIMESTAMP"));
    preSignedText += std::string(req.method_string());
    preSignedText += std::string(req.target());
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("ACCESS-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }

  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    rjValue.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }

  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }

  void prepareReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    req.set("locale", "en-US");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("ACCESS-KEY", apiKey);
    req.set("ACCESS-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    req.set("ACCESS-PASSPHRASE", apiPassphrase);
    auto apiDemoTrading = mapGetWithDefault(credential, this->apiDemoTradingName);
    if (apiDemoTrading == "1") {
      req.set("paptrading", "1");
    }
  }

  // ---------- request helpers ----------

  // JSON body params
  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "qty"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOid"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_EM_ORDER_TYPE, "orderType"},
                       {CCAPI_INSTRUMENT_TYPE, "category"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }

  // query string params
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOid"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_INSTRUMENT_TYPE, "category"},
                       {CCAPI_EM_POSITION_SIDE, "posSide"},
                       {CCAPI_LIMIT, "limit"},
                   }) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
  }

  static void popTrailingAmpersand(std::string& queryString) {
    if (!queryString.empty() && queryString.back() == '&') queryString.pop_back();
  }

  static std::string toTarget(const std::string& path, const std::string& queryString) { return queryString.empty() ? path : path + "?" + queryString; }

  // v3 trade endpoints take `category` (SPOT, MARGIN, USDT-FUTURES, COIN-FUTURES, USDC-FUTURES); default to SPOT.
  static void addDefaultCategory(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param) {
    if (param.find(CCAPI_INSTRUMENT_TYPE) == param.end() && param.find("category") == param.end()) {
      document.AddMember("category", rj::Value("SPOT").Move(), allocator);
    }
  }

  static void addDefaultCategory(std::string& queryString, const std::map<std::string, std::string>& param) {
    if (param.find(CCAPI_INSTRUMENT_TYPE) == param.end() && param.find("category") == param.end()) {
      queryString += "category=SPOT&";
    }
  }

  std::string finalizeBody(rj::Document& document) {
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    return stringBuffer.GetString();
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, request, now, symbolId, credential);
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.set("X-CHANNEL-API-CODE", CCAPI_BITGET_API_CHANNEL_API_CODE);
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        addDefaultCategory(document, allocator, param);
        if (param.find(CCAPI_EM_ORDER_TYPE) == param.end() && param.find("orderType") == param.end()) {
          document.AddMember("orderType", rj::Value("limit").Move(), allocator);
        }
        // timeInForce defaults to gtc for limit orders (ioc for market) on the exchange side, so it is not set here
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        this->signRequest(req, this->finalizeBody(document), credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.target(this->cancelOrderTarget);
        // cancel-order identifies the order by orderId / clientOid only (+ optional category); it takes no symbol
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        param.erase(CCAPI_SYMBOL_ID);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        addDefaultCategory(document, allocator, param);
        this->signRequest(req, this->finalizeBody(document), credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->cancelOpenOrdersTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        addDefaultCategory(document, allocator, param);
        // without a symbol every open order in the category is cancelled
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        this->signRequest(req, this->finalizeBody(document), credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        // order-info takes only orderId / clientOid
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        param.erase(CCAPI_SYMBOL_ID);
        param.erase(CCAPI_INSTRUMENT_TYPE);
        std::string queryString;
        this->appendParam(queryString, param);
        popTrailingAmpersand(queryString);
        req.target(toTarget(this->getOrderTarget, queryString));
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string queryString;
        this->appendParam(queryString, param);
        addDefaultCategory(queryString, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        popTrailingAmpersand(queryString);
        req.target(toTarget(this->getOpenOrdersTarget, queryString));
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        req.target(this->getAccountBalancesTarget);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        popTrailingAmpersand(queryString);
        req.target(toTarget(this->getAccountPositionsTarget, queryString));
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  // ---------- REST response parsing ----------

  static const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& orderFieldNameMap() {
    static const std::map<std::string_view, std::pair<std::string_view, JsonDataType>> m = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOid", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("qty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumExecQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, std::make_pair("cumExecValue", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("orderStatus", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
        {CCAPI_INSTRUMENT_TYPE, std::make_pair("category", JsonDataType::STRING)},
    };
    return m;
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const rj::Value& data = document["data"];
    if (data.IsObject()) {
      auto itList = data.FindMember("list");
      if (itList != data.MemberEnd() && itList->value.IsArray()) {
        // unfilled-orders and cancel-symbol-order return {"list": [...]}
        for (const auto& x : itList->value.GetArray()) {
          Element element;
          this->extractOrderInfo(element, x, orderFieldNameMap());
          elementList.emplace_back(std::move(element));
        }
      } else {
        // place-order, cancel-order and order-info return a single object
        Element element;
        this->extractOrderInfo(element, data, orderFieldNameMap());
        elementList.emplace_back(std::move(element));
      }
    } else if (data.IsArray()) {
      for (const auto& x : data.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, orderFieldNameMap());
        elementList.emplace_back(std::move(element));
      }
    }
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        const rj::Value& data = document["data"];
        for (const auto& x : data["assets"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["coin"].GetString());
          std::string available = x["available"].GetString();
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, available);
          auto itBalance = x.FindMember("balance");
          if (itBalance != x.MemberEnd() && itBalance->value.IsString()) {
            element.insert(CCAPI_EM_QUANTITY_TOTAL, itBalance->value.GetString());
          } else {
            std::string locked = x.HasMember("locked") ? x["locked"].GetString() : "0";
            element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(Decimal(available) + Decimal(locked)));
          }
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["data"]["list"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_INSTRUMENT_TYPE, x["category"].GetString());
          element.insert(CCAPI_EM_POSITION_ASSET, x["marginCoin"].GetString());
          element.insert(CCAPI_MARGIN_ASSET, x["marginCoin"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["posSide"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["total"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgPrice"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
          element.insert(CCAPI_EM_UNREALIZED_PNL, x["unrealisedPnl"].GetString());
          std::string marginMode = x["marginMode"].GetString();
          element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, marginMode == "crossed" ? CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN : CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  // ---------- private WebSocket ----------

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    // login is the same as v2
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
    std::string signData = ts + "GET" + "/user/verify";
    std::string sign = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData));
    arg.AddMember("sign", rj::Value(sign.c_str(), allocator).Move(), allocator);
    rj::Value args(rj::kArrayType);
    args.PushBack(arg, allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    sendStringList.push_back(stringBuffer.GetString());
    return sendStringList;
  }

  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    if (textMessageView == "pong") return;
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    auto it = document.FindMember("event");
    std::string eventStr = (it != document.MemberEnd() && it->value.IsString()) ? it->value.GetString() : "";
    if (eventStr == "login") {
      // v3 uses a single UTA instType; one topic covers every product category and symbol
      std::set<std::string> topicSet;
      for (const auto& field : subscription.getFieldSet()) {
        if (field == CCAPI_EM_ORDER_UPDATE) {
          topicSet.insert("order");
        } else if (field == CCAPI_EM_PRIVATE_TRADE) {
          topicSet.insert("fill");
        } else if (field == CCAPI_EM_BALANCE_UPDATE) {
          topicSet.insert("account");
        } else if (field == CCAPI_EM_POSITION_UPDATE) {
          topicSet.insert("position");
        }
      }
      rj::Document subscribeDocument;
      subscribeDocument.SetObject();
      auto& allocator = subscribeDocument.GetAllocator();
      subscribeDocument.AddMember("op", rj::Value("subscribe").Move(), allocator);
      rj::Value args(rj::kArrayType);
      for (const auto& topic : topicSet) {
        rj::Value arg(rj::kObjectType);
        arg.AddMember("instType", rj::Value("UTA").Move(), allocator);
        arg.AddMember("topic", rj::Value(topic.c_str(), allocator).Move(), allocator);
        args.PushBack(arg, allocator);
      }
      subscribeDocument.AddMember("args", args, allocator);
      rj::StringBuffer stringBufferSubscribe;
      rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
      subscribeDocument.Accept(writerSubscribe);
      ErrorCode ec;
      this->send(wsConnectionPtr, stringBufferSubscribe.GetString(), ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    } else {
      Event event = this->createEvent(subscription, textMessageView, document, eventStr, timeReceived);
      if (!event.getMessageList().empty()) {
        this->eventHandler(event, nullptr);
      }
    }
  }

  static std::string getStringOrEmpty(const rj::Value& x, const char* key) {
    auto it = x.FindMember(key);
    return (it != x.MemberEnd() && it->value.IsString()) ? it->value.GetString() : "";
  }

  Event createEvent(const Subscription& subscription, const std::string& textMessageView, const rj::Document& document, const std::string& eventStr,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (eventStr.empty()) {
      if (!document.HasMember("arg") || !document.HasMember("data")) return event;
      std::string topic = document["arg"]["topic"].GetString();
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = document["data"];
      auto instrumentAllowed = [&](const std::string& instrument) {
        return instrumentSet.empty() || instrument.empty() || instrumentSet.find(instrument) != instrumentSet.end();
      };
      if (topic == "fill") {
        if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          for (const auto& x : data.GetArray()) {
            std::string instrument = getStringOrEmpty(x, "symbol");
            if (!instrumentAllowed(instrument)) continue;
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            std::string execTime = getStringOrEmpty(x, "execTime");
            if (execTime.empty()) {
              execTime = getStringOrEmpty(x, "updatedTime");
            }
            if (!execTime.empty()) {
              message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(execTime)));
            }
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            Element element;
            element.insert(CCAPI_TRADE_ID, x["execId"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["execPrice"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["execQty"].GetString());
            element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            element.insert(CCAPI_IS_MAKER, std::string_view(x["tradeScope"].GetString()) == "maker" ? "1" : "0");
            element.insert(CCAPI_EM_ORDER_ID, x["orderId"].GetString());
            element.insert(CCAPI_EM_CLIENT_ORDER_ID, getStringOrEmpty(x, "clientOid"));
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            element.insert(CCAPI_INSTRUMENT_TYPE, x["category"].GetString());
            auto itFee = x.FindMember("feeDetail");
            if (itFee != x.MemberEnd() && itFee->value.IsArray() && itFee->value.Size() > 0) {
              const rj::Value& fee = itFee->value[0];
              element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, getStringOrEmpty(fee, "fee"));
              element.insert(CCAPI_EM_ORDER_FEE_ASSET, getStringOrEmpty(fee, "feeCoin"));
            }
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          }
        }
      } else if (topic == "order") {
        if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          for (const auto& x : data.GetArray()) {
            std::string instrument = getStringOrEmpty(x, "symbol");
            if (!instrumentAllowed(instrument)) continue;
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            std::string updatedTime = getStringOrEmpty(x, "updatedTime");
            if (!updatedTime.empty()) {
              message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(updatedTime)));
            }
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            Element info;
            this->extractOrderInfo(info, x, orderFieldNameMap());
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      } else if (topic == "account") {
        if (fieldSet.find(CCAPI_EM_BALANCE_UPDATE) != fieldSet.end()) {
          for (const auto& x : data.GetArray()) {
            std::vector<Element> elementList;
            auto addCoin = [&](const rj::Value& c) {
              Element element;
              element.insert(CCAPI_EM_ASSET, c["coin"].GetString());
              std::string available = getStringOrEmpty(c, "available");
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, available);
              std::string balance = getStringOrEmpty(c, "balance");
              if (!balance.empty()) {
                element.insert(CCAPI_EM_QUANTITY_TOTAL, balance);
              } else {
                std::string locked = getStringOrEmpty(c, "locked");
                element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(Decimal(available) + Decimal(locked.empty() ? "0" : locked)));
              }
              elementList.emplace_back(std::move(element));
            };
            auto itCoin = x.FindMember("coin");
            if (itCoin != x.MemberEnd() && itCoin->value.IsArray()) {
              for (const auto& c : itCoin->value.GetArray()) addCoin(c);
            } else if (itCoin != x.MemberEnd()) {
              addCoin(x);
            }
            if (elementList.empty()) continue;
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      } else if (topic == "position") {
        if (fieldSet.find(CCAPI_EM_POSITION_UPDATE) != fieldSet.end()) {
          for (const auto& x : data.GetArray()) {
            std::string instrument = getStringOrEmpty(x, "symbol");
            if (!instrumentAllowed(instrument)) continue;
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            std::string updatedTime = getStringOrEmpty(x, "updatedTime");
            if (updatedTime.empty()) {
              updatedTime = getStringOrEmpty(x, "createdTime");
            }
            if (!updatedTime.empty()) {
              message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(updatedTime)));
            }
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
            Element element;
            element.insert(CCAPI_INSTRUMENT, instrument);
            element.insert(CCAPI_EM_POSITION_SIDE, getStringOrEmpty(x, "posSide"));
            element.insert(CCAPI_EM_POSITION_QUANTITY, getStringOrEmpty(x, "size"));
            element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, getStringOrEmpty(x, "avgPrice"));
            element.insert(CCAPI_EM_POSITION_LEVERAGE, getStringOrEmpty(x, "leverage"));
            element.insert(CCAPI_EM_POSITION_ASSET, getStringOrEmpty(x, "marginCoin"));
            element.insert(CCAPI_EM_UNREALIZED_PNL, getStringOrEmpty(x, "unrealisedPnl"));
            std::string marginMode = getStringOrEmpty(x, "marginMode");
            if (!marginMode.empty()) {
              element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, marginMode == "crossed" ? CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN : CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
            }
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (eventStr == "subscribe") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessageView);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else if (eventStr == "error") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    }
    event.setMessageList(messageList);
    return event;
  }

  std::string apiDemoTradingName;
};

} /* namespace ccapi */
#endif
#endif
