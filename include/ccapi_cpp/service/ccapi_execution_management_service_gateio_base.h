#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_GATEIO) || defined(CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceGateioBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceGateioBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                       ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~ExecutionManagementServiceGateioBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl,
               "{\"time\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"channel\":\"" + (this->isDerivatives ? "futures" : "spot") + ".ping\"}",
               wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr,
               "{\"time\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"channel\":\"" + (this->isDerivatives ? "futures" : "spot") + ".ping\"}",
               ec);
  }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = methodString;
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    preSignedText += queryString;
    preSignedText += "\n";
    preSignedText += UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, body, true);
    preSignedText += "\n";
    preSignedText += req.base().at("TIMESTAMP").to_string();
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "SIGN:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::string& queryString, const std::string& body,
                   const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = std::string(req.method_string());
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    preSignedText += queryString;
    preSignedText += "\n";
    preSignedText += UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, body, true);
    preSignedText += "\n";
    preSignedText += req.base().at("TIMESTAMP").to_string();
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true);
    req.set("SIGN", signature);
    req.target(queryString.empty() ? path : path + "?" + queryString);
    req.body() = body;
    req.prepare_payload();
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
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += this->symbolName;
    queryString += "=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      if (value != "null") {
        if (value == "true" || value == "false") {
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
    }
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param) {
    this->appendParam(document, allocator, param,
                      {
                          {CCAPI_EM_ORDER_SIDE, "side"},
                          {CCAPI_EM_ORDER_QUANTITY, this->amountName},
                          {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                          {CCAPI_EM_CLIENT_ORDER_ID, "text"},
                          {CCAPI_EM_ORDER_TYPE, "type"},
                          {CCAPI_EM_ACCOUNT_TYPE, "account"},
                      });
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember(rj::Value(symbolName.c_str(), allocator).Move(), rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void substituteParamSettle(std::string& target, const std::map<std::string, std::string>& param, const std::string& symbolId) {
    this->substituteParam(target, param,
                          {
                              {"settle", "{settle}"},
                              {CCAPI_MARGIN_ASSET, "{settle}"},
                          });
    std::string settle;
    if (UtilString::endsWith(symbolId, "_USD")) {
      settle = "btc";
    } else if (UtilString::endsWith(symbolId, "_USDT")) {
      settle = "usdt";
    }
    this->substituteParam(target, {
                                      {"{settle}", settle},
                                  });
  }
  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set("Accept", "application/json");
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("KEY", apiKey);
    req.set("TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()));
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, now, credential);
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.set("X-Gate-Channel-Id", CCAPI_GATEIO_API_CHANNEL_ID);
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        std::string path = this->createOrderTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, "", body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        std::string path = this->cancelOrderTarget;
        this->substituteParam(path, {
                                        {"{order_id}", id},
                                    });
        param.erase(CCAPI_EM_ORDER_ID);
        param.erase(CCAPI_EM_CLIENT_ORDER_ID);
        param.erase("order_id");
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        std::string path = this->getOrderTarget;
        this->substituteParam(path, {
                                        {"{order_id}", id},
                                    });
        param.erase(CCAPI_EM_ORDER_ID);
        param.erase(CCAPI_EM_CLIENT_ORDER_ID);
        param.erase("order_id");
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        param["status"] = "open";
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        std::string path = this->getOpenOrdersTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        std::string path = this->getOpenOrdersTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ASSET, "currency"},
                          });
        if (!queryString.empty()) {
          queryString.pop_back();
        }
        std::string path = this->getAccountsTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          float total = std::stof(x["locked"].GetString()) + std::stof(x["available"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, std::to_string(total));
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("text", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair(this->amountName, JsonDataType::STRING)},
        {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("left", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_total", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(this->symbolName, JsonDataType::STRING)},
    };
    if (operation == Request::Operation::GET_OPEN_ORDERS || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else {
      Element element;
      this->extractOrderInfo(element, document, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    int time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    for (const auto& field : fieldSet) {
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      document.AddMember("time", rj::Value(time).Move(), allocator);
      std::string channel;
      if (field == CCAPI_EM_PRIVATE_TRADE) {
        channel = this->websocketChannelUserTrades;
      } else if (field == CCAPI_EM_ORDER_UPDATE) {
        channel = this->websocketChannelOrders;
      }
      document.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
      document.AddMember("event", rj::Value("subscribe").Move(), allocator);
      rj::Value payload(rj::kArrayType);
      if (instrumentSet.empty()) {
        payload.PushBack(rj::Value("!all").Move(), allocator);
      } else {
        for (const auto& instrument : instrumentSet) {
          payload.PushBack(rj::Value(instrument.c_str(), allocator).Move(), allocator);
        }
      }
      document.AddMember("payload", payload, allocator);
      rj::Value auth(rj::kObjectType);
      auth.AddMember("method", rj::Value("api_key").Move(), allocator);
      auth.AddMember("KEY", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
      std::string preSignedText = "channel=" + channel + "&event=subscribe&time=" + std::to_string(time);
      auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true);
      auth.AddMember("SIGN", rj::Value(signature.c_str(), allocator).Move(), allocator);
      document.AddMember("auth", auth, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      std::string sendString = stringBuffer.GetString();
      sendStringList.push_back(sendString);
    }
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
    std::string eventType = document["event"].GetString();
    if (eventType == "update") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      std::string channel = document["channel"].GetString();
      std::string field;
      if (channel == this->websocketChannelUserTrades) {
        field = CCAPI_EM_PRIVATE_TRADE;
      } else if (channel == this->websocketChannelOrders) {
        field = CCAPI_EM_ORDER_UPDATE;
      }
      if (fieldSet.find(field) != fieldSet.end()) {
        for (const auto& x : document["result"].GetArray()) {
          std::string instrument = x[this->symbolName.c_str()].GetString();
          if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
            if (field == CCAPI_EM_PRIVATE_TRADE) {
              Message message;
              message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["create_time_ms"].GetString())));
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
              std::vector<Element> elementList;
              Element element;
              element.insert(CCAPI_TRADE_ID, std::string(x["id"].GetString()));
              element.insert(CCAPI_EM_ORDER_ID, x["order_id"].GetString());
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["text"].GetString());
              element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["price"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x[this->amountName.c_str()].GetString());
              std::string takerSide = x["side"].GetString();
              element.insert(CCAPI_IS_MAKER, std::string(x["role"].GetString()) == "maker" ? "1" : "0");
              element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              elementList.emplace_back(std::move(element));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            } else if (field == CCAPI_EM_ORDER_UPDATE) {
              Message message;
              message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["update_time_ms"].GetString())));
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("text", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair(this->amountName, JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("left", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_total", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("event", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(this->symbolName, JsonDataType::STRING)},
              };
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
    } else if (eventType == "subscribe") {
      bool hasError = document.HasMember("error") && !document["error"].IsNull();
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(hasError ? Message::Type::SUBSCRIPTION_FAILURE : Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(hasError ? CCAPI_ERROR_MESSAGE : CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    }
    event.setMessageList(messageList);
    return event;
  }
  bool isDerivatives{};
  std::string symbolName;
  std::string websocketChannelUserTrades;
  std::string websocketChannelOrders;
  std::string amountName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_BASE_H_
