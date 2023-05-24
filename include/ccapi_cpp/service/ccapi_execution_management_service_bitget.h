#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET
#include "ccapi_cpp/service/ccapi_execution_management_service_bitget_base.h"
namespace ccapi {
class ExecutionManagementServiceBitget : public ExecutionManagementServiceBitgetBase {
 public:
  ExecutionManagementServiceBitget(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBitgetBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITGET;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/spot/v1/stream";
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
    this->apiKeyName = CCAPI_BITGET_API_KEY;
    this->apiSecretName = CCAPI_BITGET_API_SECRET;
    this->apiPassphraseName = CCAPI_BITGET_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->createOrderTarget = "/api/spot/v1/trade/orders";
    this->cancelOrderTarget = "/api/spot/v1/trade/cancel-order";
    this->getOrderTarget = "/api/spot/v1/trade/orderInfo";
    this->getOpenOrdersTarget = "/api/spot/v1/trade/open-orders";
    this->getAccountBalancesTarget = "/api/spot/v1/account/assets";
  }
  virtual ~ExecutionManagementServiceBitget() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void appendParam(Request::Operation operation, rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "quantity"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOrderId"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
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
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOrderId"},
                       {CCAPI_SYMBOL_ID, "symbol"},
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
        if (param.find("force") == param.end()) {
          document.AddMember("force", rj::Value("normal").Move(), allocator);
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
        req.method(http::verb::post);
        req.target(this->getOrderTarget);
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
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->getOpenOrdersTarget);
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
        req.target(this->getAccountBalancesTarget);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("quantity", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("fillQuantity", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("fillTotalAmount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    if (operation != Request::Operation::CANCEL_ORDER) {
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
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["coinName"].GetString());
          std::string available = x["available"].GetString();
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, available);
          std::string frozen = x["frozen"].GetString();
          std::string lock = x["lock"].GetString();
          element.insert(CCAPI_EM_QUANTITY_TOTAL, (Decimal(available).add(Decimal(frozen)).add(Decimal(lock))).toString());
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
      auto it1 = x.FindMember("accFillSz");
      auto it2 = x.FindMember("avgPx");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        auto it1Str = std::string(it1->value.GetString());
        auto it2Str = std::string(it2->value.GetString());
        if (!it1Str.empty() && !it2Str.empty()) {
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, UtilString::printDoubleScientific(std::stod(it1Str) * std::stod(it2Str)));
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
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage,
                     const TimePoint& timeReceived) override{
#else
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    WsConnection& wsConnection = *wsConnectionPtr;
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
    for (const auto& field : fieldSet) {
      std::string channel;
      if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
        channel = "orders";
      }
      for (const auto& instrument : instrumentSet) {
        rj::Value arg(rj::kObjectType);
        arg.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
        arg.AddMember("instId", rj::Value(instrument.c_str(), allocator).Move(), allocator);
        arg.AddMember("instType", rj::Value("spbl").Move(), allocator);
        args.PushBack(arg, allocator);
      }
    }
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBufferSubscribe;
    rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
    document.Accept(writerSubscribe);
    std::string sendString = stringBufferSubscribe.GetString();
    ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
    this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
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
}  // namespace ccapi
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
    const rj::Value& arg = document["arg"];
    std::string channel = std::string(arg["channel"].GetString());
    event.setType(Event::Type::SUBSCRIPTION_DATA);
    std::string instrument = arg["instId"].GetString();
    if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
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
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["fillPx"].GetString()));
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["fillSz"].GetString()));
              element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, std::string(x["execType"].GetString()) == "M" ? "1" : "0");
              element.insert(CCAPI_EM_ORDER_ID, std::string(x["ordId"].GetString()));
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(x["clOrdId"].GetString()));
              element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fillFee"].GetString()));
              element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fillFeeCcy"].GetString()));
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
            const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("ordId", JsonDataType::STRING)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdId", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("px", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("sz", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
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
}
;
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_H_
