#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_COINBASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_COINBASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceCoinbase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceCoinbase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                     ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_COINBASE;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
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
    this->apiKeyName = CCAPI_COINBASE_API_KEY;
    this->apiSecretName = CCAPI_COINBASE_API_SECRET;
    this->apiPassphraseName = CCAPI_COINBASE_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->createOrderTarget = "/orders";
    this->cancelOrderTarget = "/orders/<id>";
    this->getOrderTarget = "/orders/<id>";
    this->getOpenOrdersTarget = "/orders";
    this->cancelOpenOrdersTarget = "/orders";
    this->getAccountsTarget = "/accounts";
    this->getAccountBalancesTarget = "/accounts/<account-id>";
  }
  virtual ~ExecutionManagementServiceCoinbase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("CB-ACCESS-TIMESTAMP").to_string();
    preSignedText += methodString;
    std::string target = path;
    if (!queryString.empty()) {
      target += "?" + queryString;
    }
    preSignedText += target;
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "CB-ACCESS-SIGN:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("CB-ACCESS-TIMESTAMP").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    req.set("CB-ACCESS-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "client_oid"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("product_id", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("CB-ACCESS-KEY", apiKey);
    req.set("CB-ACCESS-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()));
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    req.set("CB-ACCESS-PASSPHRASE", apiPassphrase);
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
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        auto target = std::regex_replace(this->cancelOrderTarget, std::regex("<id>"), id);
        if (!symbolId.empty()) {
          target += "?product_id=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        auto target = std::regex_replace(this->getOrderTarget, std::regex("<id>"), id);
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        auto target = this->getOpenOrdersTarget;
        if (!symbolId.empty()) {
          target += "?product_id=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        auto target = this->cancelOpenOrdersTarget;
        if (!symbolId.empty()) {
          target += "?product_id=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, "", credential);
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
        this->substituteParam(target, {
                                          {"<account-id>", accountId},
                                      });
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
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_oid", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("executed_value", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("product_id", JsonDataType::STRING)}};
    if (operation == Request::Operation::CANCEL_ORDER) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, document.GetString());
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document.GetArray()) {
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, x.GetString());
        elementList.emplace_back(std::move(element));
      }
    } else {
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
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, x["id"].GetString());
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        element.insert(CCAPI_EM_ASSET, document["currency"].GetString());
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, document["available"].GetString());
        elementList.emplace_back(std::move(element));
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    auto timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    auto preSignedText = timestamp;
    preSignedText += "GET";
    preSignedText += "/users/self/verify";
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("type", rj::Value("subscribe").Move(), allocator);
    rj::Value channels(rj::kArrayType);
    std::string channelId;
    const auto& fieldSet = subscription.getFieldSet();
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      channelId = "full";
    }
    rj::Value channel(rj::kObjectType);
    rj::Value symbolIds(rj::kArrayType);
    const auto& instrumentSet = subscription.getInstrumentSet();
    for (const auto& instrument : instrumentSet) {
      const std::string& symbolId = instrument;
      symbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
    }
    channel.AddMember("name", rj::Value(channelId.c_str(), allocator).Move(), allocator);
    channel.AddMember("product_ids", symbolIds, allocator);
    channels.PushBack(channel, allocator);
    rj::Value heartbeatChannel(rj::kObjectType);
    heartbeatChannel.AddMember("name", rj::Value("heartbeat").Move(), allocator);
    rj::Value heartbeatSymbolIds(rj::kArrayType);
    for (const auto& instrument : instrumentSet) {
      const std::string& symbolId = instrument;
      heartbeatSymbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
    }
    heartbeatChannel.AddMember("product_ids", heartbeatSymbolIds, allocator);
    channels.PushBack(heartbeatChannel, allocator);
    document.AddMember("channels", channels, allocator);
    document.AddMember("signature", rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    document.AddMember("passphrase", rj::Value(apiPassphrase.c_str(), allocator).Move(), allocator);
    document.AddMember("timestamp", rj::Value(timestamp.c_str(), allocator).Move(), allocator);
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
    std::string type = document["type"].GetString();
    if (this->websocketFullChannelTypeSet.find(type) != websocketFullChannelTypeSet.end()) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      if (document.FindMember("user_id") != document.MemberEnd()) {
        std::string instrument = document["product_id"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          auto it = document.FindMember("time");
          if (it != document.MemberEnd()) {
            message.setTime(UtilTime::parse(std::string(it->value.GetString())));
          } else {
            auto it = document.FindMember("timestamp");
            message.setTime(UtilTime::makeTimePoint(UtilTime::divide(std::string(it->value.GetString()))));
          }
          if (type == "match" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_TRADE_ID, std::string(document["trade_id"].GetString()));
            std::string priceStr = document["price"].GetString();
            std::string sizeStr = document["size"].GetString();
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, priceStr);
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, sizeStr);
            std::string makerSide = document["side"].GetString();
            if (document.FindMember("taker_user_id") != document.MemberEnd()) {
              element.insert(CCAPI_EM_ORDER_SIDE, makerSide == "buy" ? CCAPI_EM_ORDER_SIDE_SELL : CCAPI_EM_ORDER_SIDE_BUY);
              element.insert(CCAPI_IS_MAKER, "0");
              element.insert(CCAPI_EM_ORDER_ID, document["taker_order_id"].GetString());
              element.insert(CCAPI_EM_ORDER_FEE_QUANTITY,
                             UtilString::printDoubleScientific(std::stod(priceStr) * std::stod(sizeStr) * std::stod(document["taker_fee_rate"].GetString())));
            } else if (document.FindMember("maker_user_id") != document.MemberEnd()) {
              element.insert(CCAPI_EM_ORDER_SIDE, makerSide == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, "1");
              element.insert(CCAPI_EM_ORDER_ID, document["maker_order_id"].GetString());
              element.insert(CCAPI_EM_ORDER_FEE_QUANTITY,
                             UtilString::printDoubleScientific(std::stod(priceStr) * std::stod(sizeStr) * std::stod(document["maker_fee_rate"].GetString())));
            }
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            std::map<std::string, std::pair<std::string, JsonDataType> > extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_oid", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("remaining_size", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_STATUS, std::make_pair("type", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("product_id", JsonDataType::STRING)},
            };
            if (type == "change") {
              extractionFieldNameMap.insert({CCAPI_EM_ORDER_QUANTITY, std::make_pair("new_size", JsonDataType::STRING)});
            } else {
              extractionFieldNameMap.insert({CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)});
            }
            Element info;
            this->extractOrderInfo(info, document, extractionFieldNameMap);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (type == "subscriptions") {
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
  std::string apiPassphraseName;
  std::set<std::string> websocketFullChannelTypeSet{"received", "open", "done", "match", "change", "activate"};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_COINBASE_H_
