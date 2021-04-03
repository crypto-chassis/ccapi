#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_ERISX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_ERISX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/ccapi_jwt.h"
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceErisx CCAPI_FINAL : public ExecutionManagementService {
 public:
  ExecutionManagementServiceErisx(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                  SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->name = CCAPI_EXCHANGE_NAME_ERISX;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
    this->setHostFromUrl(this->baseUrlRest);
    this->apiKeyName = CCAPI_ERISX_API_KEY;
    this->apiSecretName = CCAPI_ERISX_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/rest-api";
    this->createOrderTarget = prefix + "/new-order-single";
    this->cancelOrderTarget = prefix + "/cancel-order-single";
    this->getOrderTarget = prefix + "/order/{partyID}/{orderID}";
    this->getOpenOrdersTarget = prefix + "/order-mass-status";
    this->cancelOpenOrdersTarget = prefix + "/cancel-all";
    this->orderStatusOpenSet = {"NEW", "PARTIAL FILLED"};
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

 private:
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override {
    return body.find("\"ordStatus\":\"REJECTED\"") != std::string::npos ||
           body.find("\"message\":\"Rejected with reason NO RESTING ORDERS\"") != std::string::npos;
  }
  void signRequest(http::request<http::string_body>& req, const TimePoint& now,
                   const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    rj::Document tokenPayloadDocument;
    tokenPayloadDocument.SetObject();
    rj::Document::AllocatorType& tokenPayloadAllocator = tokenPayloadDocument.GetAllocator();
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    tokenPayloadDocument.AddMember("sub", rj::Value(apiKey.c_str(), tokenPayloadAllocator).Move(), tokenPayloadAllocator);
    tokenPayloadDocument.AddMember("iat", rj::Value(UtilTime::getUnixTimestamp(now)).Move(), tokenPayloadAllocator);
    rj::StringBuffer tokenPayloadStringBuffer;
    rj::Writer<rj::StringBuffer> tokenPayloadWriter(tokenPayloadStringBuffer);
    tokenPayloadDocument.Accept(tokenPayloadWriter);
    auto token = Jwt::generate(Hmac::ShaVersion::SHA256, apiSecret, tokenPayloadStringBuffer.GetString());
    req.set("Authorization", "Bearer " + token);
  }
  void setBody(http::request<http::string_body>& req, rj::Document& document, rj::Document::AllocatorType& allocator,
               const std::map<std::string, std::string>& param, const TimePoint& now) {
    if (param.find("transactionTime") == param.end()) {
      document.AddMember("transactionTime", rj::Value(UtilTime::convertTimePointToFIXTime(now).c_str(), allocator).Move(),
                         allocator);
    }
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    req.body() = stringBuffer.GetString();
    req.prepare_payload();
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator,
                   const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> regularizationMap = {}) {
    for (const auto& kv : param) {
      auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "BUY" : "SELL";
      }
      document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void substituteParam(std::string& target, const std::map<std::string, std::string>& param,
                       const std::map<std::string, std::string> regularizationMap = {}) {
    for (const auto& kv : param) {
      auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      target = target.replace(target.find(key), key.length(), value);
    }
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation,
                  const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    switch (operation) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {{CCAPI_EM_ORDER_SIDE, "side"},
                           {CCAPI_EM_ORDER_QUANTITY, "orderQty"},
                           {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                           {CCAPI_EM_CLIENT_ORDER_ID, "clOrdID"},
                           {CCAPI_EM_PARTY_ID, "partyID"}});
        this->appendSymbolId(document, allocator, symbolId);
        if (param.find("ordType") == param.end()) {
          document.AddMember("ordType", rj::Value("LIMIT").Move(), allocator);
        }
        if (param.find("currency") == param.end()) {
          if (symbolId.find("/") != std::string::npos) {
            document.AddMember("currency", rj::Value(UtilString::split(symbolId, "/").at(0).c_str(), allocator).Move(),
                               allocator);
          }
        }
        this->setBody(req, document, allocator, param, now);
        this->signRequest(req, now, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {{CCAPI_EM_ORDER_ID, "orderID"},
                           {CCAPI_EM_ORIGINAL_CLIENT_ORDER_ID, "origClOrdID"},
                           {CCAPI_EM_CLIENT_ORDER_ID, "clOrdID"},
                           {CCAPI_EM_ORDER_SIDE, "side"},
                           {CCAPI_EM_PARTY_ID, "partyID"}});
        if (param.find("ordType") == param.end()) {
          document.AddMember("ordType", rj::Value("LIMIT").Move(), allocator);
        }
        this->appendSymbolId(document, allocator, symbolId);
        this->setBody(req, document, allocator, param, now);
        this->signRequest(req, now, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string target = this->getOrderTarget;
        this->substituteParam(target, param, {{CCAPI_EM_ORDER_ID, "{orderID}"}, {CCAPI_EM_PARTY_ID, "{partyID}"}});
        req.target(target);
        this->signRequest(req, now, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param, {{CCAPI_EM_PARTY_ID, "partyID"}});
        this->setBody(req, document, allocator, param, now);
        this->signRequest(req, now, credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param, {{CCAPI_EM_PARTY_ID, "partyID"}});
        this->setBody(req, document, allocator, param, now);
        this->signRequest(req, now, credential);
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation,
                                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderID", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdID", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("orderQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("ordStatus", JsonDataType::STRING)}};
    std::vector<Element> elementList;
    if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : document["orderStatuses"].GetArray()) {
        elementList.emplace_back(this->extractOrderInfo(x, extractionFieldNameMap));
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
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
#endif
  std::vector<Message> convertTextMessageToMessage(const Request& request, const std::string& textMessage,
                                                   const TimePoint& timeReceived) override {
    const std::string& quotedTextMessage =
        std::regex_replace(textMessage, std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*)"), "$1\"$2\"");
    return ExecutionManagementService::convertTextMessageToMessage(request, quotedTextMessage, timeReceived);
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 protected:
#endif
  Element extractOrderInfo(
      const rj::Value& x,
      const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    Element element = ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("cumQty");
      auto it2 = x.FindMember("avgPrice");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
      }
    }
    return element;
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
  using ExecutionManagementService::convertRequest;
  FRIEND_TEST(ExecutionManagementServiceErisxTest, signRequest);
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_ERISX_H_
