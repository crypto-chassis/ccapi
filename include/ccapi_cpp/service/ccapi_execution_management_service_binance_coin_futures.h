#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_COIN_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_COIN_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_derivatives_base.h"

namespace ccapi {

class ExecutionManagementServiceBinanceCoinFutures : public ExecutionManagementServiceBinanceDerivativesBase {
 public:
  ExecutionManagementServiceBinanceCoinFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions,
                                               SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceDerivativesBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlWsOrderEntry = sessionConfigs.getUrlWebsocketOrderEntryBase().at(this->exchangeName) + CCAPI_BINANCE_COIN_FUTURES_WS_ORDER_ENTRY_PATH;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    // this->setHostWsFromUrlWsOrderEntry(this->baseUrlWsOrderEntry);
    this->apiKeyName = CCAPI_BINANCE_COIN_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_BINANCE_COIN_FUTURES_API_SECRET;
    this->websocketOrderEntryApiKeyName = CCAPI_BINANCE_COIN_FUTURES_WEBSOCKET_ORDER_ENTRY_API_KEY;
    this->websocketOrderEntryApiPrivateKeyPathName = CCAPI_BINANCE_COIN_FUTURES_WEBSOCKET_ORDER_ENTRY_API_PRIVATE_KEY_PATH;
    this->websocketOrderEntryApiPrivateKeyPasswordName = CCAPI_BINANCE_COIN_FUTURES_WEBSOCKET_ORDER_ENTRY_API_PRIVATE_KEY_PASSWORD;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->websocketOrderEntryApiKeyName, this->websocketOrderEntryApiPrivateKeyPathName,
                           this->websocketOrderEntryApiPrivateKeyPasswordName});
    this->websocketOrderEntryHost = CCAPI_BINANCE_COIN_FUTURES_HOST_WS_ORDER_ENTRY;
    this->createOrderTarget = CCAPI_BINANCE_COIN_FUTURES_CREATE_ORDER_PATH;
    this->cancelOrderTarget = "/dapi/v1/order";
    this->getOrderTarget = "/dapi/v1/order";
    this->getOpenOrdersTarget = "/dapi/v1/openOrders";
    this->cancelOpenOrdersTarget = "/dapi/v1/allOpenOrders";
    this->isDerivatives = true;
    this->listenKeyTarget = CCAPI_BINANCE_COIN_FUTURES_LISTEN_KEY_PATH;
    this->getAccountBalancesTarget = "/dapi/v1/account";
    this->getAccountPositionsTarget = "/dapi/v1/positionRisk";
  }

  virtual ~ExecutionManagementServiceBinanceCoinFutures() {}
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_COIN_FUTURES_H_
