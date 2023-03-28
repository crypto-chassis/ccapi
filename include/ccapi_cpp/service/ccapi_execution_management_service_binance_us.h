#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"
namespace ccapi {
class ExecutionManagementServiceBinanceUs : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinanceUs(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_US;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->apiKeyName = CCAPI_BINANCE_US_API_KEY;
    this->apiSecretName = CCAPI_BINANCE_US_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = CCAPI_BINANCE_US_CREATE_ORDER_PATH;
    this->cancelOrderTarget = "/api/v3/order";
    this->getOrderTarget = "/api/v3/order";
    this->getOpenOrdersTarget = "/api/v3/openOrders";
    this->cancelOpenOrdersTarget = "/api/v3/openOrders";
    this->listenKeyTarget = CCAPI_BINANCE_US_LISTEN_KEY_PATH;
    this->getAccountBalancesTarget = "/api/v3/account";
  }
  virtual ~ExecutionManagementServiceBinanceUs() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
