#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GEMINI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GEMINI_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceGemini : public ExecutionManagementService {
 public:
  ExecutionManagementServiceGemini(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_GEMINI;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->apiKeyName = CCAPI_GEMINI_API_KEY;
    this->apiSecretName = CCAPI_GEMINI_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/v1/order/new";
    this->cancelOrderTarget = "/v1/order/cancel";
    this->getOrderTarget = "/v1/order/status";
    this->getOpenOrdersTarget = "/v1/orders";
    this->cancelOpenOrdersTarget = "/v1/order/cancel/session";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceGemini() {}

 protected:
  void signRequest(http::request<http::string_body>& req, rj::Document& document, rj::Document::AllocatorType& allocator,
                   const std::map<std::string, std::string>& param, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    document.AddMember("request", rj::Value(req.target().to_string().c_str(), allocator).Move(), allocator);
    int64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    document.AddMember("nonce", rj::Value(nonce).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    auto body = stringBuffer.GetString();
    this->signRequest(req, body, credential);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto base64Payload = UtilAlgorithm::base64Encode(body);
    req.set("X-GEMINI-PAYLOAD", base64Payload);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = UtilString::toLower(Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, base64Payload, true));
    req.set("X-GEMINI-SIGNATURE", signature);
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
      }
      if (key == "order_id") {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(static_cast<int64_t>(std::stoll(value))).Move(), allocator);
      } else {
        document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
      }
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("Content-Length", "0");
    req.set("Content-Type", "text/plain");
    req.set("X-GEMINI-APIKEY", apiKey);
    req.set("Cache-Control", "no-cache");
    switch (request.getOperation()) {
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
                              {CCAPI_EM_ORDER_QUANTITY, "amount"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                          });
        this->appendSymbolId(document, allocator, symbolId);
        if (param.find("type") == param.end()) {
          document.AddMember("type", rj::Value("exchange limit").Move(), allocator);
        }
        this->signRequest(req, document, allocator, param, now, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "order_id"},
                          });
        this->signRequest(req, document, allocator, param, now, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "order_id"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, param, now, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->getOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, {},
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, {}, now, credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->cancelOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, {},
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->signRequest(req, document, allocator, {}, now, credential);
      } break;
      default:
        this->convertReqCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("original_amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executed_amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    std::vector<Element> elementList;
    if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document["details"]["cancelledOrders"].GetArray()) {
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, std::to_string(x.GetInt64()));
        elementList.emplace_back(std::move(element));
      }
    } else if (document.IsObject()) {
      elementList.emplace_back(this->extractOrderInfo(document, extractionFieldNameMap));
    } else {
      for (const auto& x : document.GetArray()) {
        elementList.emplace_back(this->extractOrderInfo(x, extractionFieldNameMap));
      }
    }
    return elementList;
  }
  std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    std::vector<Element> elementList;
    return elementList;
  }
  Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    Element element = ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("executed_amount");
      auto it2 = x.FindMember("avg_execution_price");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
      }
    }
    {
      auto it = x.FindMember("is_live");
      if (it != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_STATUS, it->value.GetBool() ? "is_live" : "is_not_live");
      }
    }
    return element;
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
  using ExecutionManagementService::convertRequest;
  using ExecutionManagementService::convertTextMessageToMessageRest;
  FRIEND_TEST(ExecutionManagementServiceGeminiTest, signRequest);
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GEMINI_H_
