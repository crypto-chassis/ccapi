#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_base.h"
namespace ccapi {
class ExecutionManagementServiceHuobi CCAPI_FINAL : public ExecutionManagementServiceHuobiBase {
 public:
  ExecutionManagementServiceHuobi(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceHuobiBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->name = CCAPI_EXCHANGE_NAME_HUOBI;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
    this->setHostFromUrl(this->baseUrlRest);
    this->apiKeyName = CCAPI_HUOBI_API_KEY;
    this->apiSecretName = CCAPI_HUOBI_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/v1/order/orders/place";
    this->cancelOrderTarget = "/v1/order/orders/{order-id}/submitcancel";
    this->cancelOrderByClientOrderIdTarget = "/v1/order/orders/submitCancelClientOrder";
    this->getOrderTarget = "/v1/order/orders/{order-id}";
    this->getOrderByClientOrderIdTarget = "/v1/order/orders/getClientOrder";
    this->getOpenOrdersTarget = "/v1/order/openOrders";
    this->orderStatusOpenSet = {"created", "submitted", "partial-filled", "canceling"};
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

 protected:
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(document, allocator, symbolId, "symbol");
  }
  void appendSymbolId(std::map<std::string, std::string>& queryParamMap, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(queryParamMap, symbolId, "symbol");
  }
  void convertReqDetail(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential, std::map<std::string, std::string>& queryParamMap) override {
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param, {
            {CCAPI_EM_ORDER_SIDE , "type"},
            {CCAPI_EM_ORDER_QUANTITY , "amount"},
            {CCAPI_EM_ORDER_LIMIT_PRICE , "price"},
            {CCAPI_EM_CLIENT_ORDER_ID , "client-order-id"},
            {CCAPI_EM_ACCOUNT_ID , "account-id"}
        });
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->createOrderTarget, queryParamMap, credential);
      }
      break;
      case Request::Operation::CANCEL_ORDER:
      {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = shouldUseOrderId ? param.at(CCAPI_EM_ORDER_ID)
            : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
            : "";
        auto target = shouldUseOrderId ? std::regex_replace(this->cancelOrderTarget, std::regex("\\{order\\-id\\}"), id) : this->cancelOrderByClientOrderIdTarget;
        if (!shouldUseOrderId) {
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          this->appendParam(document, allocator, param, {
              {CCAPI_EM_CLIENT_ORDER_ID , "client-order-id"}
          });
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          auto body = stringBuffer.GetString();
          req.body() = body;
          req.prepare_payload();
        }
        this->signRequest(req, target, queryParamMap, credential);
      }
      break;
      case Request::Operation::GET_ORDER:
      {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = shouldUseOrderId ? param.at(CCAPI_EM_ORDER_ID)
            : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
            : "";
        auto target = shouldUseOrderId ? std::regex_replace(this->getOrderTarget, std::regex("\\{order\\-id\\}"), id) : this->getOrderByClientOrderIdTarget;
        req.target(target);
        if (!shouldUseOrderId) {
          ExecutionManagementServiceHuobiBase::appendParam(queryParamMap, param, {
              {CCAPI_EM_ACCOUNT_ID , "account-id"}
          });
          queryParamMap.insert(std::make_pair("clientOrderId", Url::urlEncode(id)));
        }
        this->signRequest(req, target, queryParamMap, credential);
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        req.method(http::verb::get);
        ExecutionManagementServiceHuobiBase::appendParam(queryParamMap, {}, {
            {CCAPI_EM_ACCOUNT_ID , "account-id"}
        });
        if (!symbolId.empty()) {
          this->appendSymbolId(queryParamMap, symbolId);
        }
        this->signRequest(req, this->getOpenOrdersTarget, queryParamMap, credential);
      }
      break;
      default:
      CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<Element> extractOrderInfo(const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
      {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::INTEGER)},
      {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client-order-id", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_QUANTITY, std::make_pair("amount", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled-amount", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("filled-cash-amount", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}
    };
    std::vector<Element> elementList;
    const rj::Value& data = document["data"];
    if (operation == Request::Operation::CREATE_ORDER || operation == Request::Operation::CANCEL_ORDER) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, std::string(data.GetString()));
      elementList.emplace_back(element);
    } else if (data.IsObject()) {
      elementList.emplace_back(ExecutionManagementService::extractOrderInfo(data, extractionFieldNameMap));
    } else {
      for (const auto& x : data.GetArray()) {
        elementList.emplace_back(ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap));
      }
    }
    return elementList;
  }
  std::string cancelOrderByClientOrderIdTarget;
  std::string getOrderByClientOrderIdTarget;
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

 public:
  using ExecutionManagementService::convertRequest;
  using ExecutionManagementService::convertTextMessageToMessage;
  FRIEND_TEST(ExecutionManagementServiceHuobiTest, signRequest);
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_H_
