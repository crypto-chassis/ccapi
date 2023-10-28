#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio_base.h"
namespace ccapi {
class ExecutionManagementServiceGateio : public ExecutionManagementServiceGateioBase {
 public:
  ExecutionManagementServiceGateio(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceGateioBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GATEIO;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/v4/";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    //     try {
    //       this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->apiKeyName = CCAPI_GATEIO_API_KEY;
    this->apiSecretName = CCAPI_GATEIO_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/api/v4";
    this->createOrderTarget = prefix + "/spot/orders";
    this->cancelOrderTarget = prefix + "/spot/orders/{order_id}";
    this->getOrderTarget = prefix + "/spot/orders/{order_id}";
    this->getOpenOrdersTarget = prefix + "/spot/orders";
    this->cancelOpenOrdersTarget = prefix + "/spot/orders";
    this->getAccountsTarget = prefix + "/spot/accounts";
    this->symbolName = "currency_pair";
    this->websocketChannelUserTrades = "spot.usertrades";
    this->websocketChannelOrders = "spot.orders";
    this->amountName = "amount";
  }
  virtual ~ExecutionManagementServiceGateio() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_H_
