#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceOkex : public ExecutionManagementService {
 public:
  ExecutionManagementServiceOkex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_OKEX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + CCAPI_OKEX_PRIVATE_WS_PATH;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_OKEX_API_KEY;
    this->apiSecretName = CCAPI_OKEX_API_SECRET;
    this->apiPassphraseName = CCAPI_OKEX_API_PASSPHRASE;
    this->apiXSimulatedTradingName = CCAPI_OKEX_API_X_SIMULATED_TRADING;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName, this->apiXSimulatedTradingName});
    this->createOrderTarget = "/api/v5/trade/order";
    this->cancelOrderTarget = "/api/v5/trade/cancel-order";
    this->getOrderTarget = "/api/v5/trade/order";
    this->getOpenOrdersTarget = "/api/v5/trade/orders-pending";
    this->getAccountBalancesTarget = "/api/v5/account/balance";
    this->getAccountPositionsTarget = "/api/v5/account/positions";
  }
  virtual ~ExecutionManagementServiceOkex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"0\"")); }
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
  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
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
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
      }
      rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
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
    std::string timeStr = UtilTime::getISOTimestamp(now);
    req.set("OK-ACCESS-TIMESTAMP", timeStr.substr(0, timeStr.length() - 7) + "Z");
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    req.set("OK-ACCESS-PASSPHRASE", apiPassphrase);
    auto apiXSimulatedTrading = mapGetWithDefault(credential, this->apiXSimulatedTradingName);
    if (apiXSimulatedTrading == "1") {
      req.set("x-simulated-trading", "1");
    }
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
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
        this->appendParam(document, allocator, param);
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
        req.target(this->getOpenOrdersTarget + "?" + queryString);
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
    document.AddMember("id", rj::Value(std::to_string(wsRequestId).c_str(), allocator).Move(), allocator);
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        document.AddMember("op", rj::Value("order").Move(), allocator);
        rj::Value args(rj::kArrayType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value arg(rj::kObjectType);
        this->appendParam(arg, allocator, param);
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
        this->appendParam(arg, allocator, param);
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
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
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
          element.insert(CCAPI_EM_POSITION_COST, std::to_string(std::stod(x["avgPx"].GetString()) * std::stod(positionQuantity)));
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["lever"].GetString());
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
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::to_string(std::stod(it1Str) * std::stod(it2Str)));
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
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage, const rj::Document& document,
                     const TimePoint& timeReceived) override {
    auto it = document.FindMember("event");
    std::string eventStr = it != document.MemberEnd() ? it->value.GetString() : "";
    if (eventStr == "login") {
      rj::Document document;
      document.SetObject();
      auto& allocator = document.GetAllocator();
      document.AddMember("op", rj::Value("subscribe").Move(), allocator);
      rj::Value args(rj::kArrayType);
      auto fieldSet = subscription.getFieldSet();
      auto instrumentSet = subscription.getInstrumentSet();
      for (const auto& field : fieldSet) {
        std::string channel;
        if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          channel = "orders";
        }
        for (const auto& instrument : instrumentSet) {
          rj::Value arg(rj::kObjectType);
          arg.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
          arg.AddMember("instId", rj::Value(instrument.c_str(), allocator).Move(), allocator);
          arg.AddMember("instType", rj::Value("ANY").Move(), allocator);
          args.PushBack(arg, allocator);
        }
      }
      document.AddMember("args", args, allocator);
      rj::StringBuffer stringBufferSubscribe;
      rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
      document.Accept(writerSubscribe);
      std::string sendString = stringBufferSubscribe.GetString();
      ErrorCode ec;
      this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
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
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& eventStr,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    auto fieldSet = subscription.getFieldSet();
    auto instrumentSet = subscription.getInstrumentSet();
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
          messageList.emplace_back(std::move(message));
        }
      } else {
        const rj::Value& arg = document["arg"];
        const rj::Value& data = document["data"];
        std::string channel = std::string(arg["channel"].GetString());
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        std::string instrument = arg["instId"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          if (channel == "orders" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
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
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fillFee"].GetString()));
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fillFeeCcy"].GetString()));
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            }
          } else if (channel == "orders" && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
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
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
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
  std::string apiPassphraseName;
  std::string apiXSimulatedTradingName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_H_
