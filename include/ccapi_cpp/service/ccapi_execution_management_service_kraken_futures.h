#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "openssl/evp.h"
namespace ccapi {
class ExecutionManagementServiceKrakenFutures : public ExecutionManagementService {
 public:
  ExecutionManagementServiceKrakenFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/v1";
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
    this->apiKeyName = CCAPI_KRAKEN_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_KRAKEN_FUTURES_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix("/derivatives/api/v3");
    std::string prefix_2("/api/v3");
    this->createOrderTarget = prefix + "/sendorder";
    this->createOrderPath = prefix_2 + "/sendorder";
    this->cancelOrderTarget = prefix + "/cancelorder";
    this->cancelOrderPath = prefix_2 + "/cancelorder";
    this->getOpenOrdersTarget = prefix + "/openorders";
    this->getOpenOrdersPath = prefix_2 + "/openorders";
    this->cancelOpenOrdersTarget = prefix + "/cancelallorders";
    this->cancelOpenOrdersPath = prefix_2 + "/cancelallorders";
    this->getAccountsTarget = prefix + "/accounts";
    this->getAccountsPath = prefix_2 + "/accounts";
    this->getAccountPositionsTarget = prefix + "/openpositions";
    this->getAccountPositionsPath = prefix_2 + "/openpositions";
  }
  virtual ~ExecutionManagementServiceKrakenFutures() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("result":"success")") == std::string::npos; }
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = queryString;
    preSignedText += req.base().at("Nonce").to_string();
    ;
    preSignedText += path;
    std::string preSignedTextSha256 = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256, preSignedText);
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedTextSha256));
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "Authent:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::string& postData,
                   const std::map<std::string, std::string>& credential, const std::string& nonce) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = postData;
    preSignedText += nonce;
    preSignedText += path;
    std::string preSignedTextSha256 = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256, preSignedText);
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedTextSha256));
    req.set("Authent", signature);
  }
  std::string generateNonce(const TimePoint& now, int requestIndex) {
    int64_t nonce = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() + requestIndex;
    return std::to_string(nonce);
  }
  void appendParam(std::string& postData, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "limitPrice"},
                       {CCAPI_EM_ORDER_ID, "order_id"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "cliOrdId"},
                       {CCAPI_EM_ORDER_TYPE, "orderType"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      postData += Url::urlEncode(key);
      postData += "=";
      postData += Url::urlEncode(value);
      postData += "&";
    }
  }
  void appendSymbolId(std::string& postData, const std::string& symbolId) {
    postData += "symbol=";
    postData += Url::urlEncode(symbolId);
    postData += "&";
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("APIKey", apiKey);
    std::string nonce = this->generateNonce(now, request.getIndex());
    req.set("Nonce", nonce);
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string postData;
        this->appendParam(postData, param);
        if (param.find(CCAPI_EM_ORDER_TYPE) == param.end() && param.find("orderType") == param.end()) {
          postData += "orderType=lmt&";
        }
        this->appendSymbolId(postData, symbolId);
        postData.pop_back();
        req.target(this->createOrderTarget + "?" + postData);
        this->signRequest(req, this->createOrderPath, postData, credential, nonce);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string postData;
        this->appendParam(postData, param);
        postData.pop_back();
        req.target(this->cancelOrderTarget + "?" + postData);
        this->signRequest(req, this->cancelOrderPath, postData, credential, nonce);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        std::string postData;
        req.target(this->getOpenOrdersTarget);
        this->signRequest(req, this->getOpenOrdersPath, postData, credential, nonce);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string postData;
        if (!symbolId.empty()) {
          this->appendSymbolId(postData, symbolId);
          postData.pop_back();
        }
        if (!postData.empty()) {
          req.target(this->cancelOpenOrdersTarget + "?" + postData);
        } else {
          req.target(this->cancelOpenOrdersTarget);
        }
        this->signRequest(req, this->cancelOpenOrdersPath, postData, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::post);
        std::string postData;
        req.target(this->getAccountsTarget);
        this->signRequest(req, this->getAccountsPath, postData, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::post);
        std::string postData;
        req.target(this->getAccountPositionsTarget);
        this->signRequest(req, this->getAccountPositionsPath, postData, credential, nonce);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("cliOrdId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("quantity", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("limitPrice", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("unfilledSize", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    if (operation == Request::Operation::CREATE_ORDER || operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      const rj::Value& sendStatus = document[operation == Request::Operation::CREATE_ORDER ? "sendStatus" : "cancelStatus"];
      if (sendStatus.FindMember("orderEvents") != sendStatus.MemberEnd()) {
        for (const auto& x : sendStatus["orderEvents"].GetArray()) {
          Element element;
          this->extractOrderInfo(element, x["order"], extractionFieldNameMap);
          elementList.emplace_back(std::move(element));
        }
      } else {
        Element element;
        element.insert(CCAPI_EM_ORDER_STATUS, sendStatus["status"].GetString());
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : document["openOrders"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        if (element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY).empty()) {
          auto it = x.FindMember("filledSize");
          if (it != x.MemberEnd()) {
            element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, it->value.GetString());
          }
        }
        if (element.getValue(CCAPI_EM_ORDER_ID).empty()) {
          auto it = x.FindMember("order_id");
          if (it != x.MemberEnd()) {
            element.insert(CCAPI_EM_ORDER_ID, it->value.GetString());
          }
        }
        elementList.emplace_back(std::move(element));
      }
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        auto resultItr = document.FindMember("accounts");
        if (resultItr != document.MemberEnd()) {
          const rj::Value& result = resultItr->value;
          for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
            std::string id = itr->name.GetString();
            const rj::Value& result_2 = itr->value;
            std::string type = result_2["type"].GetString();
            for (auto itr_2 = result_2["balances"].MemberBegin(); itr_2 != result_2["balances"].MemberEnd(); ++itr_2) {
              Element element;
              element.insert(CCAPI_EM_ACCOUNT_ID, id);
              element.insert(CCAPI_EM_ACCOUNT_TYPE, type);
              element.insert(CCAPI_EM_ASSET, itr_2->name.GetString());
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, itr_2->value.GetString());
              elementList.emplace_back(std::move(element));
            }
          }
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["openPositions"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["side"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["price"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("event", rj::Value("challenge").Move(), allocator);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    document.AddMember("api_key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
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
    if (document.FindMember("event") == document.MemberEnd()) {
      if (document.FindMember("feed") != document.MemberEnd()) {
        std::string feed = document["feed"].GetString();
        if (feed == "fills" || feed == "open_orders") {
          std::string field;
          if (feed == "fills") {
            field = CCAPI_EM_PRIVATE_TRADE;
          } else if (feed == "open_orders") {
            field = CCAPI_EM_ORDER_UPDATE;
          }
          const auto& fieldSet = subscription.getFieldSet();
          if (fieldSet.find(field) != fieldSet.end()) {
            const auto& instrumentSet = subscription.getInstrumentSet();
            if (field == CCAPI_EM_PRIVATE_TRADE) {
              for (const auto& x : document["fills"].GetArray()) {
                std::string instrument = x["instrument"].GetString();
                if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                  Message message;
                  message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                  message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(x["time"].GetString())));
                  message.setTimeReceived(timeReceived);
                  message.setCorrelationIdList({subscription.getCorrelationId()});
                  std::vector<Element> elementList;
                  Element element;
                  element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["price"].GetString());
                  element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["qty"].GetString());
                  element.insert(CCAPI_EM_ORDER_SIDE, x["buy"].GetBool() ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                  element.insert(CCAPI_EM_ORDER_ID, std::string(x["order_id"].GetString()));
                  element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(x["cli_ord_id"].GetString()));
                  element.insert(CCAPI_IS_MAKER, std::string(x["fill_type"].GetString()) == "maker" ? "1" : "0");
                  element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                  element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["fee_paid"].GetString()));
                  element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(x["fee_currency"].GetString()));
                  elementList.emplace_back(std::move(element));
                  message.setElementList(elementList);
                  messageList.emplace_back(std::move(message));
                }
              }
            } else if (field == CCAPI_EM_ORDER_UPDATE) {
              const rj::Value& x = document["order"];
              std::string instrument = x["instrument"].GetString();
              if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                Message message;
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
                message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(x["last_update_time"].GetString())));
                message.setTimeReceived(timeReceived);
                message.setCorrelationIdList({subscription.getCorrelationId()});
                std::vector<Element> elementList;
                Element element;
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                element.insert(CCAPI_EM_ORDER_QUANTITY, x["qty"].GetString());
                element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, x["filled"].GetString());
                element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, x["limit_price"].GetString());
                element.insert(CCAPI_EM_ORDER_ID, x["order_id"].GetString());
                element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["direction"].GetString()) == "0" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                element.insert("is_cancel", document["is_cancel"].GetBool() ? "1" : "0");
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            }
          }
        }
      }
    } else {
      std::string eventType = document["event"].GetString();
      if (eventType == "challenge") {
        auto credential = subscription.getCredential();
        if (credential.empty()) {
          credential = this->credentialDefault;
        }
        auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
        auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
        std::string challengeToSign = document["message"].GetString();
        std::string challengeToSignSha256 = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256, challengeToSign);
        auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), challengeToSignSha256));
        std::vector<std::string> sendStringList;
        for (const auto& field : subscription.getFieldSet()) {
          {
            std::string feed;
            if (field == CCAPI_EM_PRIVATE_TRADE) {
              feed = "fills";
            } else if (field == CCAPI_EM_ORDER_UPDATE) {
              feed = "open_orders";
            }
            rj::Document document;
            document.SetObject();
            auto& allocator = document.GetAllocator();
            document.AddMember("event", rj::Value("subscribe").Move(), allocator);
            document.AddMember("feed", rj::Value(feed.c_str(), allocator).Move(), allocator);
            document.AddMember("api_key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
            document.AddMember("original_challenge", rj::Value(challengeToSign.c_str(), allocator).Move(), allocator);
            document.AddMember("signed_challenge", rj::Value(signature.c_str(), allocator).Move(), allocator);
            rj::StringBuffer stringBuffer;
            rj::Writer<rj::StringBuffer> writer(stringBuffer);
            document.Accept(writer);
            std::string sendString = stringBuffer.GetString();
            sendStringList.push_back(sendString);
          }
          {
            rj::Document document;
            document.SetObject();
            auto& allocator = document.GetAllocator();
            document.AddMember("event", rj::Value("subscribe").Move(), allocator);
            document.AddMember("feed", rj::Value("heartbeat").Move(), allocator);
            rj::StringBuffer stringBuffer;
            rj::Writer<rj::StringBuffer> writer(stringBuffer);
            document.Accept(writer);
            std::string sendString = stringBuffer.GetString();
            sendStringList.push_back(sendString);
          }
        }
        for (const auto& sendString : sendStringList) {
          ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          this->send(hdl, sendString, wspp::frame::opcode::text, ec);
#else
          this->send(wsConnectionPtr, sendString, ec);
#endif
          if (ec) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
          }
        }
      } else if (eventType == "subscribed" || eventType == "error") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        message.setType(eventType == "subscribed" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(eventType == "subscribed" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string createOrderPath;
  std::string cancelOrderPath;
  std::string getOpenOrdersPath;
  std::string cancelOpenOrdersPath;
  std::string getAccountsPath;
  std::string getAccountPositionsPath;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_FUTURES_H_
