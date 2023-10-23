#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_FTX) || defined(CCAPI_ENABLE_EXCHANGE_FTX_US)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceFtxBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceFtxBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                    ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->createOrderTarget = "/api/orders";
    this->cancelOrderTarget = "/api/orders";
    this->getOrderTarget = "/api/orders";
    this->getOpenOrdersTarget = "/api/orders";
    this->cancelOpenOrdersTarget = "/api/orders";
    this->getAccountsTarget = "/api/subaccounts";
    this->getAccountBalancesTarget = "/api/wallet/balances";
  }
  virtual ~ExecutionManagementServiceFtxBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, R"({"op":"ping"})", ec); }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at(this->ftx + "-TS").to_string();
    preSignedText += methodString;
    std::string target = path;
    if (!queryString.empty()) {
      target += "?" + queryString;
    }
    preSignedText += target;
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += this->ftx + "-SIGN:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at(this->ftx + "-TS").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set(this->ftx + "-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_TYPE, "type"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientId"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      if (key == "type" && value == "market") {
        document.AddMember("price", rj::Value(rj::Type::kNullType), allocator);
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
  void appendParam_2(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                     const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (value != "null") {
        if (value == "true" || value == "false") {
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("market", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set(this->ftx + "-KEY", apiKey);
    req.set(this->ftx + "-TS", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    auto apiSubaccountName = mapGetWithDefault(credential, this->apiSubaccountName);
    if (!apiSubaccountName.empty()) {
      req.set(this->ftx + "-SUBACCOUNT", Url::urlEncode(apiSubaccountName));
    }
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, now, credential);
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
        this->appendParam(document, allocator, param);
        if (param.find("type") == param.end()) {
          document.AddMember("type", rj::Value("limit").Move(), allocator);
        }
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        bool shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        req.target(shouldUseOrderId ? this->cancelOrderTarget + "/" + id : this->cancelOrderTarget + "/by_client_id/" + id);
        std::string body;
        if (!shouldUseOrderId && !symbolId.empty()) {
          body = R"({"market":")";
          body += symbolId;
          body += R"("})";
        }
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        req.target(shouldUseOrderId ? this->getOrderTarget + "/" + id : this->getOrderTarget + "/by_client_id/" + id);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        auto target = this->getOpenOrdersTarget;
        if (!symbolId.empty()) {
          target += "?market=";
          target += Url::urlEncode(symbolId);
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        auto target = this->cancelOpenOrdersTarget;
        req.target(target);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam_2(document, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::get);
        req.target(this->getAccountsTarget);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto target = this->getAccountBalancesTarget;
        auto accountId = param.find(CCAPI_EM_ACCOUNT_ID) != param.end() ? param.at(CCAPI_EM_ACCOUNT_ID) : "";
        if (!accountId.empty()) {
          target = "/api/subaccounts/{nickname}/balances";
          this->substituteParam(target, {
                                            {"{nickname}", Url::urlEncode(accountId)},
                                        });
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filledSize", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("market", JsonDataType::STRING)},
    };
    const auto& result = document["result"];
    if (result.IsArray()) {
      for (const auto& x : result.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (result.IsObject()) {
      Element element;
      this->extractOrderInfo(element, result, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : document["result"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, x["nickname"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document["result"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["coin"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["total"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
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
      auto it1 = x.FindMember("filledSize");
      auto it2 = x.FindMember("avgFillPrice");
      if (it1 != x.MemberEnd() && !it1->value.IsNull() && it2 != x.MemberEnd() && !it2->value.IsNull()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       Decimal(UtilString::printDoubleScientific(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString()))).toString());
      }
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("login").Move(), allocator);
    rj::Document args;
    args.SetObject();
    rj::Document::AllocatorType& allocatorArgs = args.GetAllocator();
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto subaccount = mapGetWithDefault(credential, this->apiSubaccountName);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    args.AddMember("key", rj::Value(apiKey.c_str(), allocatorArgs).Move(), allocatorArgs);
    std::string signData = ts + "websocket_login";
    std::string sign = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData, true);
    args.AddMember("sign", rj::Value(sign.c_str(), allocatorArgs).Move(), allocatorArgs);
    rj::Value timeRj;
    timeRj.SetInt64(std::stoll(ts));
    args.AddMember("time", rj::Value(timeRj, allocatorArgs).Move(), allocatorArgs);
    if (!subaccount.empty()) {
      args.AddMember("subaccount", rj::Value(subaccount.c_str(), allocatorArgs).Move(), allocatorArgs);
    }
    document.AddMember("args", rj::Value(args, allocator).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    const auto& fieldSet = subscription.getFieldSet();
    for (const auto& field : subscription.getFieldSet()) {
      std::string channelId;
      if (field == CCAPI_EM_ORDER_UPDATE) {
        channelId = "orders";
      } else if (field == CCAPI_EM_PRIVATE_TRADE) {
        channelId = "fills";
      }
      rj::Document documentSubscribe;
      documentSubscribe.SetObject();
      rj::Document::AllocatorType& allocatorSubscribe = documentSubscribe.GetAllocator();
      documentSubscribe.AddMember("op", rj::Value("subscribe").Move(), allocatorSubscribe);
      documentSubscribe.AddMember("channel", rj::Value(channelId.c_str(), allocatorSubscribe).Move(), allocatorSubscribe);
      rj::StringBuffer stringBufferSubscribe;
      rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
      documentSubscribe.Accept(writerSubscribe);
      std::string sendStringSubscribe = stringBufferSubscribe.GetString();
      sendStringList.push_back(sendStringSubscribe);
    }
    return sendStringList;
  }
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    Event event = this->createEvent(subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    std::string type = document["type"].GetString();
    if (type == "update") {
      std::string channel = std::string(document["channel"].GetString());
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = document["data"];
      std::string instrument = data["market"].GetString();
      if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
        if (channel == "fills" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          message.setTime(UtilTime::parse(std::string(data["time"].GetString()), "%FT%T%Ez"));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, std::string(data["tradeId"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(data["price"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(data["size"].GetString()));
          element.insert(CCAPI_EM_ORDER_SIDE, std::string(data["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_IS_MAKER, std::string(data["liquidity"].GetString()) == "maker" ? "1" : "0");
          element.insert(CCAPI_EM_ORDER_ID, std::string(data["orderId"].GetString()));
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
          element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(data["fee"].GetString()));
          {
            auto it = data.FindMember("clientOrderId");
            if (it != data.MemberEnd() && !it->value.IsNull()) {
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(it->value.GetString()));
            }
          }
          elementList.emplace_back(std::move(element));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        } else if (channel == "orders" && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          message.setTime(timeReceived);
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          const std::map<std::string, std::pair<std::string, JsonDataType>>& extractionFieldNameMap = {
              {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientId", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("remainingSize", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filledSize", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("market", JsonDataType::STRING)},
          };
          Element info;
          this->extractOrderInfo(info, data, extractionFieldNameMap);
          {
            auto it1 = data.FindMember("filledSize");
            auto it2 = data.FindMember("avgFillPrice");
            if (it1 != data.MemberEnd() && !it1->value.IsNull() && it2 != data.MemberEnd() && !it2->value.IsNull()) {
              info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                          Decimal(UtilString::printDoubleScientific(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString()))).toString());
            }
          }
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    } else if (type == "subscribed") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else if (type == "error") {
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
  std::string apiSubaccountName;
  std::string ftx;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_BASE_H_
