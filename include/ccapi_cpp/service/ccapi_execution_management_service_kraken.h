#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "openssl/evp.h"
namespace ccapi {
class ExecutionManagementServiceKraken : public ExecutionManagementService {
 public:
  ExecutionManagementServiceKraken(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_KRAKEN;
    this->baseUrl = CCAPI_KRAKEN_URL_WS_BASE_PRIVATE;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_KRAKEN_API_KEY;
    this->apiSecretName = CCAPI_KRAKEN_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/0/private";
    this->createOrderTarget = prefix + "/AddOrder";
    this->cancelOrderTarget = prefix + "/CancelOrder";
    this->getOrderTarget = prefix + "/QueryOrders";
    this->getOpenOrdersTarget = prefix + "/OpenOrders";
    this->cancelOpenOrdersTarget = prefix + "/CancelAll";
    this->getAccountBalancesTarget = prefix + "/Balance";
    this->getAccountPositionsTarget = prefix + "/OpenPositions";
    this->getWebSocketsTokenTarget = prefix + "/GetWebSocketsToken";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceKraken() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override { return body.find(R"("error":[])") == std::string::npos; }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential,
                   const std::string& nonce) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto noncePlusBody = nonce + body;
    std::string preSignedText = req.target().to_string();
    std::string noncePlusBodySha256;
    computeHash(noncePlusBody, noncePlusBodySha256);
    preSignedText += noncePlusBodySha256;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    req.set("API-Sign", signature);
    req.body() = body;
    req.prepare_payload();
  }
  std::string generateNonce(const TimePoint& now) {
    int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(nonce);
  }
  void appendParam(std::string& body, const std::map<std::string, std::string>& param, const std::string& nonce,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "type"},
                       {CCAPI_EM_ORDER_QUANTITY, "volume"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "userref"},
                       {CCAPI_EM_ORDER_TYPE, "ordertype"},
                       {CCAPI_EM_ORDER_LEVERAGE, "leverage"},
                       {CCAPI_EM_ORDER_ID, "txid"},
                   }) {
    body += "nonce=";
    body += nonce;
    body += "&";
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "type") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
      }
      body += Url::urlEncode(key);
      body += "=";
      body += Url::urlEncode(value);
      body += "&";
    }
  }
  void appendSymbolId(std::string& body, const std::string& symbolId) {
    body += "pair=";
    body += Url::urlEncode(symbolId);
    body += "&";
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/x-www-form-urlencoded; charset=utf-8");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("API-Key", apiKey);
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        if (param.find("type") == param.end()) {
          body += "ordertype=limit&";
        }
        this->appendSymbolId(body, symbolId);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOrderTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOpenOrdersTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOpenOrdersTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getAccountBalancesTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getAccountPositionsTarget);
        std::string body;
        std::string nonce = this->generateNonce(now);
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("userref", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("vol", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("vol_exec", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
    };
    std::vector<Element> elementList;
    if (operation == Request::Operation::CREATE_ORDER) {
      for (const auto& x : document["result"]["txid"].GetArray()) {
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, x.GetString());
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::GET_ORDER || operation == Request::Operation::GET_OPEN_ORDERS) {
      const rj::Value& orders = operation == Request::Operation::GET_ORDER ? document["result"] : document["result"]["open"];
      for (auto itr = orders.MemberBegin(); itr != orders.MemberEnd(); ++itr) {
        Element element;
        this->extractOrderInfo(element, itr->value, extractionFieldNameMap);
        const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionMoreFieldNameMap = {
            {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
            {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
            {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("pair", JsonDataType::STRING)}};
        this->extractOrderInfo(element, itr->value["descr"], extractionMoreFieldNameMap);
        auto it1 = itr->value.FindMember("vol_exec");
        auto it2 = itr->value.FindMember("price");
        if (it1 != itr->value.MemberEnd() && it2 != itr->value.MemberEnd()) {
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                         std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
        }
        element.insert(CCAPI_EM_ORDER_ID, itr->name.GetString());
        elementList.emplace_back(std::move(element));
      }
    }
    return elementList;
  }
  std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    std::vector<Element> elementList;
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        auto resultItr = document.FindMember("result");
        if (resultItr != document.MemberEnd()) {
          const rj::Value& result = resultItr->value;
          for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
            Element element;
            element.insert(CCAPI_EM_ASSET, itr->name.GetString());
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, itr->value.GetString());
            elementList.emplace_back(std::move(element));
          }
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        auto resultItr = document.FindMember("result");
        if (resultItr != document.MemberEnd()) {
          const rj::Value& result = resultItr->value;
          for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
            Element element;
            element.insert(CCAPI_EM_SYMBOL, itr->value["pair"].GetString());
            element.insert(CCAPI_EM_POSITION_QUANTITY,
                           Decimal(itr->value["vol"].GetString()).subtract(Decimal(itr->value["vol_closed"].GetString())).toString());
            element.insert(CCAPI_EM_POSITION_COST, itr->value["cost"].GetString());
            elementList.emplace_back(std::move(element));
          }
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return elementList;
  }
  void prepareConnect(WsConnection& wsConnection) override {
    auto now = UtilTime::now();
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    std::string target = this->getWebSocketsTokenTarget;
    req.target(target);
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("API-Key", apiKey);
    req.set(beast::http::field::content_type, "application/x-www-form-urlencoded; charset=utf-8");
    std::string body;
    std::string nonce = this->generateNonce(now);
    this->appendParam(body, {}, nonce);
    body.pop_back();
    this->signRequest(req, body, credential, nonce);
    this->sendRequest(
        req,
        [wsConnection, that = shared_from_base<ExecutionManagementServiceKraken>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<ExecutionManagementServiceKraken>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              std::string token = document["result"]["token"].GetString();
              thisWsConnection.url = that->baseUrl;
              that->connect(thisWsConnection);
              that->extraPropertyByConnectionIdMap[thisWsConnection.id].insert({
                  {"token", token},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(thisWsConnection);
        },
        this->sessionOptions.httpRequestTimeoutMilliSeconds);
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    auto fieldSet = subscription.getFieldSet();
    std::vector<std::string> sendStringList;
    for (const auto& field : fieldSet) {
      std::string name;
      if (field == CCAPI_EM_ORDER_UPDATE) {
        name = "openOrders";
      } else if (field == CCAPI_EM_PRIVATE_TRADE) {
        name = "ownTrades";
      }
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      document.AddMember("event", rj::Value("subscribe").Move(), allocator);
      rj::Value subscription(rj::kObjectType);
      subscription.AddMember("name", rj::Value(name.c_str(), allocator).Move(), allocator);
      subscription.AddMember("token", rj::Value(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("token").c_str(), allocator).Move(), allocator);
      document.AddMember("subscription", subscription, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      std::string sendString = stringBuffer.GetString();
      sendStringList.push_back(sendString);
    }
    return sendStringList;
  }
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage, const rj::Document& document,
                     const TimePoint& timeReceived) override {
    Event event = this->createEvent(subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    if (document.IsArray() && document.Size() == 3) {
      std::string channelName = document[1].GetString();
      if (channelName == "ownTrades" || channelName == "openOrders") {
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        auto fieldSet = subscription.getFieldSet();
        auto instrumentSet = subscription.getInstrumentSet();
        if (channelName == "ownTrades" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          for (const auto& x : document[0].GetArray()) {
            for (auto itr = x.MemberBegin(); itr != x.MemberEnd(); ++itr) {
              std::string instrument = itr->value["pair"].GetString();
              if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                Message message;
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                auto timePair = UtilTime::divide(std::string(itr->value["time"].GetString()));
                auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
                tp += std::chrono::nanoseconds(timePair.second);
                message.setTime(tp);
                message.setTimeReceived(timeReceived);
                message.setCorrelationIdList({subscription.getCorrelationId()});
                std::vector<Element> elementList;
                Element element;
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, itr->value["price"].GetString());
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, itr->value["vol"].GetString());
                element.insert(CCAPI_EM_ORDER_SIDE, std::string(itr->value["type"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                element.insert(CCAPI_EM_ORDER_ID, std::string(itr->value["ordertxid"].GetString()));
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(itr->value["fee"].GetString()));
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            }
          }
        }
        if (channelName == "openOrders" && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          Message message;
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          message.setTime(timeReceived);
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          std::vector<Element> elementList;
          const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("userref", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("vol", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("vol_exec", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
          };
          for (const auto& x : document[0].GetArray()) {
            for (auto itr = x.MemberBegin(); itr != x.MemberEnd(); ++itr) {
              const rj::Value& descr = itr->value["descr"];
              std::string instrument = descr["pair"].GetString();
              if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                Element element;
                this->extractOrderInfo(element, itr->value, extractionFieldNameMap);
                const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionMoreFieldNameMap = {
                    {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("pair", JsonDataType::STRING)}};
                this->extractOrderInfo(element, descr, extractionMoreFieldNameMap);
                auto it1 = itr->value.FindMember("vol_exec");
                auto it2 = itr->value.FindMember("avg_price");
                if (it1 != itr->value.MemberEnd() && it2 != itr->value.MemberEnd()) {
                  element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                                 std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
                }
                element.insert(CCAPI_EM_ORDER_ID, itr->name.GetString());
                elementList.emplace_back(std::move(element));
              }
            }
          }
          if (!elementList.empty()) {
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (document.IsObject() && document.HasMember("event")) {
      std::string eventPayload = std::string(document["event"].GetString());
      if (eventPayload == "heartbeat") {
        // CCAPI_LOGGER_DEBUG("heartbeat: " + toString(wsConnection));
      } else if (eventPayload == "subscriptionStatus") {
        std::string status = document["status"].GetString();
        if (status == "subscribed" || status == "error") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          message.setType(status == "subscribed" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(status == "subscribed" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.push_back(std::move(message));
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  static bool computeHash(const std::string& unhashed, std::string& hashed, bool returnHex = false) {
    bool success = false;
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context != NULL) {
      if (EVP_DigestInit_ex(context, EVP_sha256(), NULL)) {
        if (EVP_DigestUpdate(context, unhashed.c_str(), unhashed.length())) {
          unsigned char hash[EVP_MAX_MD_SIZE];
          unsigned int lengthOfHash = 0;
          if (EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
            std::stringstream ss;
            if (returnHex) {
              for (unsigned int i = 0; i < lengthOfHash; ++i) {
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
              }
            } else {
              for (unsigned int i = 0; i < lengthOfHash; ++i) {
                ss << (char)hash[i];
              }
            }
            hashed = ss.str();
            success = true;
          }
        }
      }
      EVP_MD_CTX_free(context);
    }
    return success;
  }
  std::string getWebSocketsTokenTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_H_
