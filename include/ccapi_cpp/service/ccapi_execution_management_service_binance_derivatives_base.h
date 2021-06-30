#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_DERIVATIVES_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_DERIVATIVES_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"
namespace ccapi {
class ExecutionManagementServiceBinanceDerivativesBase : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinanceDerivativesBase(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                           ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    // this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_DERIVATIVES_BASE;
    // this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    // this->setHostRestFromUrlRest(this->baseUrlRest);
    // try {
    //   this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    // } catch (const std::exception& e) {
    //   CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    // }
    // this->apiKeyName = CCAPI_BINANCE_DERIVATIVES_BASE_API_KEY;
    // this->apiSecretName = CCAPI_BINANCE_DERIVATIVES_BASE_API_SECRET;
    // this->setupCredential({this->apiKeyName, this->apiSecretName});
    // this->createOrderTarget = CCAPI_BINANCE_DERIVATIVES_BASE_CREATE_ORDER_TARGET;
    // this->cancelOrderTarget = "/fapi/v1/order";
    // this->getOrderTarget = "/fapi/v1/order";
    // this->getOpenOrdersTarget = "/fapi/v1/openOrders";
    // this->cancelOpenOrdersTarget = "/fapi/v1/allOpenOrders";
    this->isDerivatives = true;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~ExecutionManagementServiceBinanceDerivativesBase() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_DERIVATIVES_BASE_H_
