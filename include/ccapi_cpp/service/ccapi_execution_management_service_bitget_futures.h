#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_bitget_base.h"
namespace ccapi {
class ExecutionManagementServiceBitgetFutures : public ExecutionManagementServiceBitgetBase {
 public:
  ExecutionManagementServiceBitgetFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBitgetBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITGET_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v2/ws/private";
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
    this->apiKeyName = CCAPI_BITGET_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_BITGET_FUTURES_API_SECRET;
    this->apiPassphraseName = CCAPI_BITGET_FUTURES_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->createOrderTarget = "/api/v2/mix/order/place-order";
    this->cancelOrderTarget = "/api/v2/mix/order/cancel-order";
    this->getOrderTarget = "/api/v2/mix/order/detail";
    this->getOpenOrdersTarget = "/api/v2/mix/order/orders-pending";
    this->getAllOpenOrdersTarget = "/api/v2/mix/order/orders-pending";
    this->cancelOpenOrdersTarget = "/api/v2/mix/order/cancel-all-orders";
    this->getAccountBalancesTarget = "/api/v2/mix/account/accounts";
    this->getAccountPositionsTarget = "/api/v2/mix/position/single-position";
    this->getAccountAllPositionsTarget = "/api/v2/mix/position/all-position";
  }
  virtual ~ExecutionManagementServiceBitgetFutures() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void appendParam(Request::Operation operation, rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_MARGIN_ASSET, "marginCoin"},
                       {CCAPI_MARGIN_MODE, "marginMode"},
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOid"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_INSTRUMENT_TYPE, "productType"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_MARGIN_ASSET, "marginCoin"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOid"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_INSTRUMENT_TYPE, "productType"},
                   }) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
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
        this->appendParam(operation, document, allocator, param);
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
        this->appendParam(queryString, param);
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
        this->appendParam(queryString, param);
        std::string path;
        if (symbolId.empty()) {
          path = this->getAllOpenOrdersTarget;
        } else {
          this->appendSymbolId(queryString, symbolId);
          path = this->getOpenOrdersTarget;
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(path + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->cancelOpenOrdersTarget);
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
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(this->getAccountBalancesTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param);
        std::string path;
        if (symbolId.empty()) {
          path = this->getAccountAllPositionsTarget;
        } else {
          this->appendSymbolId(queryString, symbolId);
          path = this->getAccountPositionsTarget;
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(path + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOid", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("baseVolume", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    if (operation == Request::Operation::CREATE_ORDER || operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::GET_ORDER) {
      const rj::Value& data = document["data"];
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      const rj::Value& data = document["data"]["entrustedList"];
      for (const auto& x : data.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
        const rj::Value& successList = document["data"]["successList"];
        for (const auto& x : successList.GetArray()) {
            Element element;
            this->extractOrderInfo(element, x, extractionFieldNameMap);
            elementList.emplace_back(std::move(element));
        }
        const rj::Value& failureList = document["data"]["failureList"];
        for (const auto& x : failureList.GetArray()) {
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
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["marginCoin"].GetString());
          std::string available = x["crossedMaxAvailable"].GetString();
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, available);
          std::string locked = x["locked"].GetString();
          element.insert(CCAPI_EM_QUANTITY_TOTAL, (Decimal(available).add(Decimal(locked))).toString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_MARGIN_ASSET, x["marginCoin"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["holdSide"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["total"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["openPriceAvg"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
          element.insert(CCAPI_EM_UNREALIZED_PNL, x["unrealizedPL"].GetString());
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
      auto it1 = x.FindMember("accBaseVolume");
      auto it2 = x.FindMember("priceAvg");
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
    std::string signData = ts + "GET" + "/user/verify";
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
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage,
                     const TimePoint& timeReceived) override {
#else
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
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
        const auto& instrumentType = subscription.getInstrumentType();
        for (const auto& field : fieldSet) {
            if (field == CCAPI_EM_ORDER_UPDATE || field == CCAPI_EM_PRIVATE_TRADE) {
                for (const auto& instrument : instrumentSet) {
                    rj::Value arg(rj::kObjectType); 
                    arg.AddMember("channel", rj::Value("orders", allocator).Move(), allocator);
                    arg.AddMember("instId", rj::Value(instrument.c_str(), allocator).Move(), allocator);
                    arg.AddMember("instType", rj::Value(instrumentType.c_str(), allocator).Move(), allocator);
                    args.PushBack(arg, allocator); 
                }
            } else if (field == CCAPI_EM_POSITION_UPDATE) {
                rj::Value arg(rj::kObjectType); 
                arg.AddMember("channel", rj::Value("positions", allocator).Move(), allocator);
                arg.AddMember("instId", rj::Value("default", allocator).Move(), allocator);
                arg.AddMember("instType", rj::Value(instrumentType.c_str(), allocator).Move(), allocator);
                args.PushBack(arg, allocator);
            } else if (field == CCAPI_EM_BALANCE_UPDATE) {
                rj::Value arg(rj::kObjectType); 
                arg.AddMember("channel", rj::Value("account", allocator).Move(), allocator);
                arg.AddMember("coin", rj::Value("default", allocator).Move(), allocator);
                arg.AddMember("instType", rj::Value(instrumentType.c_str(), allocator).Move(), allocator);
                args.PushBack(arg, allocator);
            }
        }
        document.AddMember("args", args, allocator);
        rj::StringBuffer stringBufferSubscribe;
        rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
        document.Accept(writerSubscribe);
        std::string sendString = stringBufferSubscribe.GetString();
        ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
        this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
        this->send(wsConnectionPtr, sendString, ec);
#endif
        if (ec) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
        }
      } else {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
        Event event = this->createEvent(wsConnection, wsConnection.hdl, subscription, textMessage, document, eventStr, timeReceived);
#else
        Event event = this->createEvent(wsConnectionPtr, subscription, textMessageView, document, eventStr, timeReceived);
#endif
        if (!event.getMessageList().empty()) {
          this->eventHandler(event, nullptr);
        }
      }
    }
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  Event createEvent(const WsConnection& wsConnection, wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage,
                    const rj::Document& document, const std::string& eventStr, const TimePoint& timeReceived) {
#else
  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const std::string& eventStr, const TimePoint& timeReceived) {
    std::string textMessage(textMessageView);
#endif
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    const auto& correlationId = subscription.getCorrelationId();
    message.setCorrelationIdList({correlationId});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (eventStr.empty()) {
      const rj::Value& arg = document["arg"];
      std::string channel = std::string(arg["channel"].GetString());
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      std::string instId = (channel == "account") ? "coin" : "instId";
      std::string instrument = arg[instId.c_str()].GetString();
      if (instrumentSet.empty() || instrument == "default" || instrumentSet.find(instrument) != instrumentSet.end()) {
        if (channel == "orders") {
          if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            const rj::Value& data = document["data"];
            for (const auto& x : data.GetArray()) {
              auto itTradeId = x.FindMember("tradeId");
              std::string tradeId = itTradeId != x.MemberEnd() ? itTradeId->value.GetString() : "";
              if (!tradeId.empty()) {
                Message message;
                message.setTimeReceived(timeReceived);
                message.setCorrelationIdList({subscription.getCorrelationId()});
                message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["fillTime"].GetString()))));
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                std::vector<Element> elementList;
                Element element;
                element.insert(CCAPI_TRADE_ID, tradeId);
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["fillPrice"].GetString()));
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["baseVolume"].GetString()));
                element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                element.insert(CCAPI_IS_MAKER, std::string(x["tradeScope"].GetString()) == "M" ? "1" : "0");
                element.insert(CCAPI_EM_ORDER_ID, std::string(x["orderId"].GetString()));          
                auto itClientOId = x.FindMember("clientOId");
                std::string clientOId = itClientOId != x.MemberEnd() ? itClientOId->value.GetString() : "";
                if(!clientOId.empty()){
                      element.insert(CCAPI_EM_CLIENT_ORDER_ID, clientOId);
                }
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fillFee"].GetString()));
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fillFeeCoin"].GetString()));
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            }
          }
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
            const rj::Value& data = document["data"];
            for (const auto& x : data.GetArray()) {
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["uTime"].GetString()))));
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOId", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accBaseVolume", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
              };
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              info.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        } else if (channel == "positions"){
          const rj::Value& data = document["data"];
          for (const auto& x : data.GetArray()) {
            auto time = TimePoint(std::chrono::milliseconds(std::stoll(x["uTime"].GetString())));
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(time);
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
            element.insert(CCAPI_EM_POSITION_SIDE, x["holdSide"].GetString());
            element.insert(CCAPI_EM_POSITION_QUANTITY, x["total"].GetString());
            element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["openPriceAvg"].GetString());
            element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        } else if (channel == "account"){
          const rj::Value& data = document["data"];
          for (const auto& x : data.GetArray()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_EM_ASSET, x["marginCoin"].GetString());
            element.insert(CCAPI_EM_QUANTITY_TOTAL, x["equity"].GetString());
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
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
  std::string getAllOpenOrdersTarget, getAccountAllPositionsTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_FUTURES_H_
