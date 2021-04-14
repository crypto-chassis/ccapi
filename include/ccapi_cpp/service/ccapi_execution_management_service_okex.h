#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include <regex>
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceOkex CCAPI_FINAL : public ExecutionManagementService {
 public:
  ExecutionManagementServiceOkex(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_OKEX;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostFromUrl(this->baseUrlRest);
    this->apiKeyName = CCAPI_OKEX_API_KEY;
    this->apiSecretName = CCAPI_OKEX_API_SECRET;
    this->apiPassphraseName = CCAPI_OKEX_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->createOrderTarget = "/api/v5/trade/order";
    this->cancelOrderTarget = "/api/v5/trade/cancel-order";
    this->getOrderTarget = "/api/v5/trade/order";
    this->getOpenOrdersTarget = "/api/v5/trade/orders-pending";
    this->orderStatusOpenSet = {"live", "partially_filled"};
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

 private:
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"0\"")); }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("OK-ACCESS-TIMESTAMP").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("OK-ACCESS-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
      } else if (key == "ordType") {
        value = UtilString::toLower(value);
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
    document.AddMember("instId", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "instId=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now,
                  const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("OK-ACCESS-KEY", apiKey);
    std::string timeStr = UtilTime::getISOTimestamp(now);
    req.set("OK-ACCESS-TIMESTAMP", timeStr.substr(0, timeStr.length() - 7) + "Z");
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName, {});
    req.set("OK-ACCESS-PASSPHRASE", apiPassphrase);
    switch (operation) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {{CCAPI_EM_ORDER_TYPE, "ordType"},
                           {CCAPI_EM_ORDER_SIDE, "side"},
                           {CCAPI_EM_ORDER_QUANTITY, "sz"},
                           {CCAPI_EM_ORDER_LIMIT_PRICE, "px"},
                           {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"},
                           {CCAPI_SYMBOL_ID, "instId"}});
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.target(this->cancelOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param, {{CCAPI_EM_ORDER_ID, "ordId"}, {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"}, {CCAPI_SYMBOL_ID, "instId"}});
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param, {{CCAPI_EM_ORDER_ID, "ordId"}, {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"}, {CCAPI_SYMBOL_ID, "instId"}});
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {{CCAPI_EM_ORDER_TYPE, "ordType"}, {CCAPI_EM_ORDER_ID, "ordId"}, {CCAPI_EM_CLIENT_ORDER_ID, "clOrdId"}, {CCAPI_SYMBOL_ID, "instId"}});
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(this->getOpenOrdersTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("ordId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clOrdId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("sz", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("px", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instId", JsonDataType::STRING)}};
    std::vector<Element> elementList;
    const rj::Value& data = document["data"];
    if (data.IsObject()) {
      elementList.emplace_back(this->extractOrderInfo(data, extractionFieldNameMap));
    } else {
      for (const auto& x : data.GetArray()) {
        elementList.emplace_back(this->extractOrderInfo(x, extractionFieldNameMap));
      }
    }
    return elementList;
  }
  std::string apiPassphraseName;
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 protected:
#endif
  Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    CCAPI_LOGGER_TRACE("");
    Element element = ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("accFillSz");
      auto it2 = x.FindMember("avgPx");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * (it2->value.IsNull() ? 0 : std::stod(it2->value.GetString()))));
      }
    }
    CCAPI_LOGGER_TRACE("");
    return element;
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
  using ExecutionManagementService::convertRequest;
  using ExecutionManagementService::convertTextMessageToMessage;
  FRIEND_TEST(ExecutionManagementServiceOkexTest, signRequest);
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_H_
