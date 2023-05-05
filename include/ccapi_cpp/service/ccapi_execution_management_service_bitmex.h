#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITMEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITMEX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBitmex : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBitmex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITMEX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/realtime";
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
    this->apiKeyName = CCAPI_BITMEX_API_KEY;
    this->apiSecretName = CCAPI_BITMEX_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/api/v1";
    this->createOrderTarget = prefix + "/order";
    this->cancelOrderTarget = prefix + "/order";
    this->getOrderTarget = prefix + "/order";
    this->getOpenOrdersTarget = prefix + "/order";
    this->cancelOpenOrdersTarget = prefix + "/order/all";
    this->getAccountBalancesTarget = prefix + "/user/margin";
    this->getAccountPositionsTarget = prefix + "/position";
  }
  virtual ~ExecutionManagementServiceBitmex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = methodString;
    std::string target = path;
    if (!queryString.empty()) {
      target += "?" + queryString;
    }
    preSignedText += target;
    preSignedText += req.base().at("api-expires").to_string();
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "api-signature:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += req.base().at("api-expires").to_string();
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("api-signature", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "Buy") ? "Buy" : "Sell";
      }
      document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
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
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    req.set("api-expires", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                              (now + std::chrono::seconds(CCAPI_BITMEX_API_RECEIVE_WINDOW_SECONDS)).time_since_epoch())
                                              .count()));
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("api-key", apiKey);
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
                              {CCAPI_EM_ORDER_QUANTITY, "orderQty"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "clOrdID"},
                          });
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderID"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "clOrdID"},
                          });
        if (!queryString.empty()) {
          queryString.pop_back();
        }
        req.target(this->cancelOrderTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        if (!param.empty()) {
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          this->appendParam(document, allocator, param,
                            {
                                {CCAPI_EM_ORDER_ID, "orderID"},
                                {CCAPI_EM_CLIENT_ORDER_ID, "clOrdID"},
                            });
          queryString += "filter=";
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          queryString += Url::urlEncode(stringBuffer.GetString());
          queryString += "&";
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (!queryString.empty()) {
          queryString.pop_back();
        }
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString("filter=");
        queryString += Url::urlEncode("{\"open\": true}");
        queryString += "&";
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (!queryString.empty()) {
          queryString.pop_back();
        }
        req.target(this->getOrderTarget + "?" + queryString);
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
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto target = this->getAccountBalancesTarget;
        auto asset = param.find(CCAPI_EM_ASSET) != param.end() ? param.at(CCAPI_EM_ASSET) : "";
        target += "?currency=" + asset;
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto target = this->getAccountPositionsTarget;
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderID", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdID", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("orderQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("ordStatus", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    if (document.IsObject()) {
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
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        element.insert(CCAPI_EM_ACCOUNT_ID, document["account"].GetString());
        element.insert(CCAPI_EM_ASSET, document["currency"].GetString());
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, document["availableMargin"].GetString());
        elementList.emplace_back(std::move(element));
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, x["account"].GetString());
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["currentQty"].GetString());
          element.insert(CCAPI_EM_POSITION_COST, x["openingCost"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgEntryPrice"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
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
      auto it1 = x.FindMember("cumQty");
      auto it2 = x.FindMember("avgPx");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * (it2->value.IsNull() ? 0 : std::stod(it2->value.GetString()))));
      }
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("authKeyExpires").Move(), allocator);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto expires =
        std::chrono::duration_cast<std::chrono::seconds>((now + std::chrono::seconds(CCAPI_BITMEX_API_RECEIVE_WINDOW_SECONDS)).time_since_epoch()).count();
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = "GET";
    preSignedText += "/realtime";
    preSignedText += std::to_string(expires);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    rj::Value args(rj::kArrayType);
    args.PushBack(rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    args.PushBack(rj::Value(expires).Move(), allocator);
    args.PushBack(rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
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
                    const rj::Document& document, const TimePoint& timeReceived){
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
  if (document.FindMember("error") != document.MemberEnd()) {
    auto it = document.FindMember("request");
    if (it != document.MemberEnd()) {
      if (std::string(it->value["op"].GetString()) == "authKeyExpires") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        message.setType(Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    }
  } else {
    if (document.FindMember("success") != document.MemberEnd()) {
      auto it = document.FindMember("request");
      if (it != document.MemberEnd()) {
        if (std::string(it->value["op"].GetString()) == "authKeyExpires") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          message.setType(Message::Type::SUBSCRIPTION_STARTED);
          Element element;
          element.insert(CCAPI_INFO_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("op", rj::Value("subscribe").Move(), allocator);
          rj::Value args(rj::kArrayType);
          if (instrumentSet.empty()) {
            for (const auto& field : fieldSet) {
              if (field == CCAPI_EM_ORDER_UPDATE) {
                args.PushBack(rj::Value("order").Move(), allocator);
              } else if (field == CCAPI_EM_PRIVATE_TRADE) {
                args.PushBack(rj::Value("execution").Move(), allocator);
              }
            }
          } else {
            for (const auto& instrument : instrumentSet) {
              for (const auto& field : fieldSet) {
                std::string arg;
                if (field == CCAPI_EM_ORDER_UPDATE) {
                  arg = std::string("order:") + instrument;
                } else if (field == CCAPI_EM_PRIVATE_TRADE) {
                  arg = std::string("execution:") + instrument;
                }
                args.PushBack(rj::Value(arg.c_str(), allocator).Move(), allocator);
              }
            }
          }
          document.AddMember("args", args, allocator);
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
          this->send(hdl, sendString, wspp::frame::opcode::text, ec);
#else
            this->send(wsConnectionPtr, sendString, ec);
#endif
          if (ec) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
          }
        }
      }
    } else {
      auto it = document.FindMember("table");
      if (it != document.MemberEnd()) {
        std::string table = it->value.GetString();
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        auto it_2 = document.FindMember("action");
        if (it_2 != document.MemberEnd()) {
          std::string action = it_2->value.GetString();
          if (action == "insert") {
            for (const auto& x : document["data"].GetArray()) {
              std::string instrument = std::string(x["symbol"].GetString());
              if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                message.setTime(UtilTime::parse(std::string(x["timestamp"].GetString())));
                if (table == "execution" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
                  std::string execType = x["execType"].GetString();
                  if (execType == "Trade") {
                    message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                    std::vector<Element> elementList;
                    Element element;
                    element.insert(CCAPI_TRADE_ID, std::string(x["trdMatchID"].GetString()));
                    element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["lastPx"].GetString()));
                    element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["lastQty"].GetString()));
                    element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "Buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                    element.insert(CCAPI_IS_MAKER, std::string(x["lastLiquidityInd"].GetString()) == "RemovedLiquidity" ? "0" : "1");
                    element.insert(CCAPI_EM_ORDER_ID, std::string(x["orderID"].GetString()));
                    element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(x["clOrdID"].GetString()));
                    element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                    element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["commission"].GetString()));
                    elementList.emplace_back(std::move(element));
                    message.setElementList(elementList);
                    messageList.emplace_back(std::move(message));
                  }
                } else if (table == "order" && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
                  message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
                  const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                      {CCAPI_EM_ORDER_ID, std::make_pair("orderID", JsonDataType::STRING)},
                      {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdID", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_QUANTITY, std::make_pair("orderQty", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("leavesQty", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumQty", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_STATUS, std::make_pair("ordStatus", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
                  };
                  Element info;
                  this->extractOrderInfo(info, x, extractionFieldNameMap);
                  auto it = x.FindMember("avgPx");
                  if (it != x.MemberEnd() && !it->value.IsNull()) {
                    info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                                std::to_string(std::stod(it->value.GetString()) * std::stod(x["cumQty"].GetString())));
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
      }
    }
  }
  event.setMessageList(messageList);
  return event;
}
};  // namespace ccapi
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITMEX_H_
