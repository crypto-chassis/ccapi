#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KRAKEN_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "openssl/evp.h"
namespace ccapi {
class ExecutionManagementServiceKraken : public ExecutionManagementService {
 public:
  ExecutionManagementServiceKraken(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                     ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_KRAKEN;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
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
    this->getWebSocketsTokenTarget = prefix + "GetWebSocketsToken";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceKraken() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override { return body.find(R"("error":[])") == std::string::npos; }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential, const std::string& nonce) {
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
  std::string generateNonce(const TimePoint& now){
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
      for (const auto& x: document["result"]["txid"].GetArray()){
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, x.GetString());
        elementList.emplace_back(std::move(element));
      }
    }else if (operation == Request::Operation::GET_ORDER || operation == Request::Operation::GET_OPEN_ORDERS) {
      const rj::Value& orders = operation == Request::Operation::GET_ORDER? document["result"]:document["result"]["open"];
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
        const rj::Value& result = document["result"];
        for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
          Element element;
          element.insert(CCAPI_EM_ASSET, itr->name.GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, itr->value.GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        const rj::Value& result = document["result"];
        for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
          Element element;
          element.insert(CCAPI_EM_SYMBOL, itr->value["pair"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, Decimal(itr->value["vol"].GetString()).subtract(Decimal(itr->value["vol_closed"].GetString())).toString());
          element.insert(CCAPI_EM_POSITION_COST, itr->value["cost"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return elementList;
  }
  void prepareConnect(WsConnection& wsConnection) override {
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
    this->sendRequest(
        req,
        [wsConnection, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              std::string token = document["result"]["token"].GetString();
              thisWsConnection.url = url;
              that->connect(that->baseUrl);
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
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection,const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    auto fieldSet = subscription.getFieldSet();
    std::vector<std::string> sendStringList;
    for (const auto& field : fieldSet){
      std::string name;
      if (field == CCAPI_EM_ORDER_UPDATE) ) {
        channelId = "openOrders";
      } else if (field == CCAPI_EM_PRIVATE_TRADE){
        name = "ownTrades";
      }
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      document.AddMember("event", rj::Value("subscribe").Move(), allocator);
      rj::Value subscription(rj::kObjectType);
      subscription.AddMember("name", rj::Value(name.c_str(), allocator).Move(), allocator);
      subscription.AddMember("token", rj::Value(UtilAlgorithm::base64Encode(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("token")).c_str(), allocator).Move(), allocator);
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
      this->eventHandler(event);
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    std::string type = document["type"].GetString();
    if (this->websocketFullChannelTypeSet.find(type) != websocketFullChannelTypeSet.end()) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      auto fieldSet = subscription.getFieldSet();
      auto instrumentSet = subscription.getInstrumentSet();
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
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, document["price"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, document["size"].GetString());
            std::string takerSide = document["side"].GetString();
            if (document.FindMember("taker_user_id") != document.MemberEnd()) {
              element.insert(CCAPI_EM_ORDER_SIDE, takerSide == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, "0");
              element.insert(CCAPI_EM_ORDER_ID, document["taker_order_id"].GetString());
            } else if (document.FindMember("maker_user_id") != document.MemberEnd()) {
              element.insert(CCAPI_EM_ORDER_SIDE, takerSide == "sell" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, "1");
              element.insert(CCAPI_EM_ORDER_ID, document["maker_order_id"].GetString());
            }
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.push_back(std::move(message));
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
            auto info = this->extractOrderInfo(document, extractionFieldNameMap);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.push_back(std::move(message));
          }
        }
      }
    } else if (type == "subscriptions") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.push_back(std::move(message));
    } else if (type == "error") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.push_back(std::move(message));
    }
    event.setMessageList(messageList);
    return event;
  }
  static bool computeHash(const std::string& unhashed, std::string& hashed, bool returnHex = false)
  {
      bool success = false;
      EVP_MD_CTX* context = EVP_MD_CTX_new();
      if(context != NULL)
      {
          if(EVP_DigestInit_ex(context, EVP_sha256(), NULL))
          {
              if(EVP_DigestUpdate(context, unhashed.c_str(), unhashed.length()))
              {
                  unsigned char hash[EVP_MAX_MD_SIZE];
                  unsigned int lengthOfHash = 0;
                  if(EVP_DigestFinal_ex(context, hash, &lengthOfHash))
                  {
                    std::stringstream ss;
                    if (returnHex) {
                      for(unsigned int i = 0; i < lengthOfHash; ++i)
                      {
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
