#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_MARGIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_MARGIN_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_MARGIN
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"
namespace ccapi {
class ExecutionManagementServiceBinanceMargin : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinanceMargin(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_MARGIN;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_BINANCE_MARGIN_API_KEY;
    this->apiSecretName = CCAPI_BINANCE_MARGIN_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/sapi/v1/margin/order";
    this->cancelOrderTarget = "/sapi/v1/margin/order";
    this->getOrderTarget = "/sapi/v1/margin/order";
    this->getOpenOrdersTarget = "/sapi/v1/margin/openOrders";
    this->cancelOpenOrdersTarget = "/sapi/v1/margin/openOrders";
    this->listenKeyTarget = CCAPI_BINANCE_LISTEN_KEY_PATH;
    this->getAccountBalancesTarget = "/sapi/v1/margin/account";
  }
  virtual ~ExecutionManagementServiceBinanceMargin() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_MARGIN_H_
