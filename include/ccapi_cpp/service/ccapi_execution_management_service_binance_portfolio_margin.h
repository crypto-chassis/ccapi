#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_PORTFOLIO_MARGIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_PORTFOLIO_MARGIN_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_PORTFOLIO_MARGIN
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"

namespace ccapi {

class ExecutionManagementServiceBinancePortfolioMargin : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinancePortfolioMargin(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions,
                                               SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_PORTFOLIO_MARGIN;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->apiKeyName = CCAPI_BINANCE_PORTFOLIO_MARGIN_API_KEY;
    this->apiSecretName = CCAPI_BINANCE_PORTFOLIO_MARGIN_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = CCAPI_BINANCE_PORTFOLIO_MARGIN_CREATE_ORDER_PATH;
    this->cancelOrderTarget = "/papi/v1/um/order";
    this->getOrderTarget = "/papi/v1/um/order";
    this->getOpenOrdersTarget = "/papi/v1/um/openOrders";
    this->cancelOpenOrdersTarget = "/papi/v1/um/allOpenOrders";
    this->isDerivatives = true;
    this->getAccountBalancesTarget = "/papi/v1/balance";
    this->getAccountPositionsTarget = "/papi/v1/um/positionRisk";
  }

  virtual ~ExecutionManagementServiceBinancePortfolioMargin() {}
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_PORTFOLIO_MARGIN_H_