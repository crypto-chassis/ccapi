#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_USDT_SWAP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_USDT_SWAP_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_base.h"
namespace ccapi {
class ExecutionManagementServiceHuobiUsdtSwap : public ExecutionManagementServiceHuobiBase {
 public:
  ExecutionManagementServiceHuobiUsdtSwap(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceHuobiBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostFromUrl(this->baseUrlRest);
    this->apiKeyName = CCAPI_HUOBI_USDT_SWAP_API_KEY;
    this->apiSecretName = CCAPI_HUOBI_USDT_SWAP_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = CCAPI_HUOBI_USDT_SWAP_CREATE_ORDER_TARGET;
    this->cancelOrderTarget = CCAPI_HUOBI_USDT_SWAP_CANCEL_ORDER_TARGET;
    this->getOrderTarget = CCAPI_HUOBI_USDT_SWAP_GET_ORDER_TARGET;
    this->getOpenOrdersTarget = CCAPI_HUOBI_USDT_SWAP_GET_OPEN_ORDERS_TARGET;
    this->isDerivatives = true;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceHuobiUsdtSwap() {}

 private:
  bool doesHttpBodyContainError(const Request& request, const std::string& body) override { return body.find("err_code") != std::string::npos; }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(document, allocator, symbolId, "contract_code");
  }
  void appendSymbolId(std::map<std::string, std::string>& queryParamMap, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(queryParamMap, symbolId, "contract_code");
  }
  void convertReqDetail(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                        const std::map<std::string, std::string>& credential, std::map<std::string, std::string>& queryParamMap) override {
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {{CCAPI_EM_ORDER_SIDE, "direction"},
                           {CCAPI_EM_ORDER_QUANTITY, "volume"},
                           {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                           {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"}});
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->createOrderTarget, queryParamMap, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param, {{CCAPI_EM_ORDER_ID, "order_id"}, {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"}});
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->cancelOrderTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param, {{CCAPI_EM_ORDER_ID, "order_id"}, {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"}});
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->getOrderTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->getOpenOrdersTarget, queryParamMap, credential);
      } break;
      default:
        this->convertReqCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("direction", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("volume", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("trade_volume", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("contract_code", JsonDataType::STRING)}};
    std::vector<Element> elementList;
    const rj::Value& data = document["data"];
    if (operation == Request::Operation::CREATE_ORDER) {
      elementList.emplace_back(ExecutionManagementServiceHuobiUsdtSwap::extractOrderInfo(data, extractionFieldNameMap));
    } else if (operation == Request::Operation::CANCEL_ORDER) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, std::string(data["successes"].GetString()));
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_ORDER) {
      elementList.emplace_back(ExecutionManagementServiceHuobiUsdtSwap::extractOrderInfo(data[0], extractionFieldNameMap));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : data["orders"].GetArray()) {
        elementList.emplace_back(ExecutionManagementServiceHuobiUsdtSwap::extractOrderInfo(x, extractionFieldNameMap));
      }
    }
    return elementList;
  }
  std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    std::vector<Element> elementList;
    return elementList;
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
#endif
  std::vector<Message> convertTextMessageToMessageRest(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    const std::string& quotedTextMessage = std::regex_replace(textMessage, std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*)"), "$1\"$2\"");
    CCAPI_LOGGER_DEBUG("quotedTextMessage = " + quotedTextMessage);
    return ExecutionManagementService::convertTextMessageToMessageRest(request, quotedTextMessage, timeReceived);
  }
  Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    Element element = ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("trade_volume");
      auto it2 = x.FindMember("trade_avg_price");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * (it2->value.IsNull() ? 0 : std::stod(it2->value.GetString()))));
      }
    }
    return element;
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
  using ExecutionManagementService::convertRequest;
  FRIEND_TEST(ExecutionManagementServiceHuobiUsdtSwapTest, signRequest);
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_USDT_SWAP_H_
