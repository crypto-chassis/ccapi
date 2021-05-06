#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
// used for ftx authentication
constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
std::string string_to_hex(unsigned char* data, std::size_t len) {
  std::string s(len * 2, ' ');
  for (std::size_t i = 0; i < len; ++i) {
    s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
    s[2 * i + 1] = hexmap[data[i] & 0x0F];
  }
  return s;
}

namespace ccapi {
class ExecutionManagementServiceFtx : public ExecutionManagementService {
 public:
  ExecutionManagementServiceFtx(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostFromUrl(this->baseUrlRest);
    this->apiKeyName = CCAPI_FTX_API_KEY;
    this->apiSecretName = CCAPI_FTX_API_SECRET;
    this->apiSubaccountName = CCAPI_FTX_API_SUBACCOUNT;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiSubaccountName});
    this->createOrderTarget = "/api/orders";
    this->cancelOrderTarget = "/api/orders";
    this->getOrderTarget = "/api/orders";
    this->getOpenOrdersTarget = "/api/orders";
    this->cancelOpenOrdersTarget = "/api/orders";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceFtx() {}

 protected:
  void signRequest(http::request<http::string_body>& req, const TimePoint& now, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

    std::string method(req.method_string());
    std::string path(req.target());
    std::string body_str(req.body());

    std::string data = ts + method + path;
    if (!body_str.empty()) {
      data += body_str;
    }
    std::string hmacced = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, data);
    std::string sign = string_to_hex((unsigned char*)hmacced.c_str(), 32);

    req.set("FTX-SIGN", sign);
    req.set("FTX-TS", ts);
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;

      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
      }
      if (key == "price" || key == "size") {
        // `price` and `size` should be of double type on the document
        auto doubleVal = rj::Value();
        if (value != "NULL") {
          double number_val = atof(value.c_str());
          doubleVal = rj::Value(number_val);
        }
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), doubleVal, allocator);
      } else {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
      }
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("market", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("FTX-KEY", apiKey);

    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendSymbolId(document, allocator, symbolId);
        // order type handling
        this->appendParam(document, allocator, param,
                          {{CCAPI_EM_ORDER_SIDE, "side"},
                           {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                           {CCAPI_EM_ORDER_QUANTITY, "size"},
                           {CCAPI_EM_ORDER_TYPE, "type"},
                           {CCAPI_EM_CLIENT_ORDER_ID, "clientId"}});

        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, now, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID) : "";
        req.target(this->cancelOrderTarget + "/" + id);
        this->signRequest(req, now, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID) : "";
        req.target(this->getOrderTarget + "/" + id);
        this->signRequest(req, now, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        auto target = this->getOpenOrdersTarget;
        if (!symbolId.empty()) {
          target += "?market=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, now, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        auto target = this->cancelOpenOrdersTarget;
        if (!symbolId.empty()) {
          target += "?market=";
          target += symbolId;
        }
        req.target(target);
        this->signRequest(req, now, "", credential);
      } break;
      default:
        this->convertReqCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    rj::Document d = rj::Document();
    d.CopyFrom(document, d.GetAllocator());
    d.Accept(writer);

    std::cout << "GOT RESPONSE " << buffer.GetString() << std::endl;
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filledSize", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("market", JsonDataType::STRING)},
    };
    std::vector<Element> elementList;
    if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, document["result"].GetString());
      elementList.emplace_back(std::move(element));
    } else if (document["result"].IsArray()) {
      for (const auto& x : document["result"].GetArray()) {
        elementList.emplace_back(this->extractOrderInfo(x, extractionFieldNameMap));
      }
    } else if (document.IsObject()) {
      elementList.emplace_back(this->extractOrderInfo(document["result"], extractionFieldNameMap));
    }
    return elementList;
  }
  std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    std::vector<Element> elementList;
    return elementList;
  }

  std::vector<Element> extractExecutionInfoFromDocument(const rj::Document& document) {
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    rj::Document d = rj::Document();
    d.CopyFrom(document, d.GetAllocator());
    d.Accept(writer);

    std::cout << "GOT RESPONSE " << buffer.GetString() << std::endl;
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
        {CCAPI_TRADE_ID, std::make_pair("tradeId", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::make_pair("price", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filledSize", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("remainingSize", JsonDataType::DOUBLE)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("market", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_FEE_QUANTITY, std::make_pair("fee", JsonDataType::DOUBLE)}};
    std::vector<Element> elementList;
    if (document.IsObject()) {
      elementList.emplace_back(this->extractOrderInfo(document["data"], extractionFieldNameMap));
    }
    return elementList;
  }

  std::vector<Message> convertDocumentToMessage(const Subscription& subscription, const rj::Document& document, const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("textMessage = " + textMessage);
    Message message;
    std::vector<Message> messageList;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    auto channel = std::string(document["channel"].GetString());
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = " + type);
    CCAPI_LOGGER_TRACE("channel = " + channel);
    if (type == "update") {
      if (channel == "fills") {
        message.setTime(UtilTime::parse(std::string(document["data"]["time"].GetString()), "%FT%T%Ez"));
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
        message.setElementList(this->extractExecutionInfoFromDocument(document));
        messageList.push_back(std::move(message));
      } else if (channel == "orders") {
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
        message.setElementList(this->extractExecutionInfoFromDocument(document));
        messageList.push_back(std::move(message));
      }
    }

    return messageList;
  }

  std::vector<std::string> createSendStringListFromSubscription(const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("login").Move(), allocator);
    // create the args document
    rj::Document args;
    args.SetObject();
    rj::Document::AllocatorType& allocatorArgs = args.GetAllocator();

    // Get the signed values
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto subaccount = mapGetWithDefault(credential, this->apiSubaccountName);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    args.AddMember("key", rj::Value(apiKey.c_str(), allocatorArgs).Move(), allocatorArgs);
    std::string signData = ts + "websocket_login";
    std::string hmacced = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData);
    std::string sign = string_to_hex((unsigned char*)hmacced.c_str(), 32);
    args.AddMember("sign", rj::Value(sign.c_str(), allocatorArgs).Move(), allocatorArgs);
    rj::Value timeRj;
    timeRj.SetInt64(std::stol(ts));
    args.AddMember("time", rj::Value(timeRj, allocatorArgs).Move(), allocatorArgs);
    if (!subaccount.empty()) {
      args.AddMember("subaccount", rj::Value(subaccount.c_str(), allocatorArgs).Move(), allocatorArgs);
    }
    // Add the args object to the main document
    document.AddMember("args", rj::Value(args, allocator).Move(), allocator);
    // Turn the rapidjson document into a string
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    // First element should be the authentication string
    sendStringList.push_back(sendString);

    // Second element should be the channel to subscribe to
    std::string channelId;
    auto fieldSet = subscription.getFieldSet();
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
      channelId = "orders";
    } else if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      channelId = "fills";
    }

    rj::Document documentSubscribe;
    documentSubscribe.SetObject();
    rj::Document::AllocatorType& allocatorSubscribe = documentSubscribe.GetAllocator();
    documentSubscribe.AddMember("op", rj::Value("subscribe").Move(), allocatorSubscribe);
    documentSubscribe.AddMember("channel", rj::Value(channelId.c_str(), allocatorSubscribe).Move(), allocatorSubscribe);
    // Turn the rapidjson document into a string
    rj::StringBuffer stringBufferSubscribe;
    rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
    documentSubscribe.Accept(writerSubscribe);
    std::string sendStringSubscribe = stringBufferSubscribe.GetString();
    sendStringList.push_back(sendStringSubscribe);
    return sendStringList;
  }
  std::string apiSubaccountName;
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
  // TODO(cryptochassis): add more to ftx test.
 public:
  using ExecutionManagementService::convertRequest;
  using ExecutionManagementService::processSuccessfulTextMessage;
  FRIEND_TEST(ExecutionManagementServiceFTXTest, signRequest);
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
