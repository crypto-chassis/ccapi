#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"
namespace ccapi {
class ExecutionManagementServiceBinanceFutures CCAPI_FINAL : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinanceFutures(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                           SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->name = CCAPI_EXCHANGE_NAME_BINANCE_FUTURES;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
    this->setHostFromUrl(this->baseUrlRest);
    this->apiKeyName = CCAPI_BINANCE_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_BINANCE_FUTURES_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = CCAPI_BINANCE_FUTURES_CREATE_ORDER_TARGET;
    this->cancelOrderTarget = "/fapi/v1/order";
    this->getOrderTarget = "/fapi/v1/order";
    this->getOpenOrdersTarget = "/fapi/v1/openOrders";
    this->cancelOpenOrdersTarget = "/fapi/v1/allOpenOrders";
    this->isFutures = true;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
 public:
  using ExecutionManagementService::convertTextMessageToMessage;
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_FUTURES_H_
