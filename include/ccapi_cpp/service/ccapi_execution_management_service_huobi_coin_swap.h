#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_COIN_SWAP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_COIN_SWAP_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_derivatives_base.h"
namespace ccapi {
class ExecutionManagementServiceHuobiCoinSwap : public ExecutionManagementServiceHuobiDerivativesBase {
 public:
  ExecutionManagementServiceHuobiCoinSwap(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceHuobiDerivativesBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/swap-notification";
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
    this->apiKeyName = CCAPI_HUOBI_COIN_SWAP_API_KEY;
    this->apiSecretName = CCAPI_HUOBI_COIN_SWAP_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = CCAPI_HUOBI_COIN_SWAP_CREATE_ORDER_PATH;
    this->cancelOrderTarget = CCAPI_HUOBI_COIN_SWAP_CANCEL_ORDER_PATH;
    this->getOrderTarget = CCAPI_HUOBI_COIN_SWAP_GET_ORDER_PATH;
    this->getOpenOrdersTarget = CCAPI_HUOBI_COIN_SWAP_GET_OPEN_ORDERS_PATH;
    this->getAccountBalancesTarget = CCAPI_HUOBI_COIN_SWAP_GET_ACCOUNT_BALANCES_PATH;
    this->getAccountPositionsTarget = CCAPI_HUOBI_COIN_SWAP_GET_ACCOUNT_POSITIONS_PATH;
    this->authenticationPath = "/swap-api/v1/swap_order";
    this->orderDataTopic = CCAPI_HUOBI_COIN_SWAP_SUBSCRIBE_ORDER_DATA_TOPIC;
    this->matchOrderDataTopic = CCAPI_HUOBI_COIN_SWAP_SUBSCRIBE_MATCH_ORDER_DATA_TOPIC;
  }
  virtual ~ExecutionManagementServiceHuobiCoinSwap() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_COIN_SWAP_H_
