#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#ifndef CCAPI_BINANCE_CREATE_ORDER_PATH
#define CCAPI_BINANCE_CREATE_ORDER_PATH "/api/v3/order"
#endif
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"

namespace ccapi {

class ExecutionManagementServiceBinance : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinance(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                    ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlWsOrderEntry = sessionConfigs.getUrlWebsocketOrderEntryBase().at(this->exchangeName) + CCAPI_BINANCE_WS_ORDER_ENTRY_PATH;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    // this->setHostWsFromUrlWsOrderEntry(this->baseUrlWsOrderEntry);
    this->apiKeyName = CCAPI_BINANCE_API_KEY;
    this->apiSecretName = CCAPI_BINANCE_API_SECRET;
    this->websocketOrderEntryApiKeyName = CCAPI_BINANCE_WEBSOCKET_ORDER_ENTRY_API_KEY;
    this->websocketOrderEntryApiPrivateKeyPathName = CCAPI_BINANCE_WEBSOCKET_ORDER_ENTRY_API_PRIVATE_KEY_PATH;
    this->websocketOrderEntryApiPrivateKeyPasswordName = CCAPI_BINANCE_WEBSOCKET_ORDER_ENTRY_API_PRIVATE_KEY_PASSWORD;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->websocketOrderEntryApiKeyName, this->websocketOrderEntryApiPrivateKeyPathName,
                           this->websocketOrderEntryApiPrivateKeyPasswordName});
    this->websocketOrderEntryHost = CCAPI_BINANCE_HOST_WS_ORDER_ENTRY;
    this->createOrderTarget = CCAPI_BINANCE_CREATE_ORDER_PATH;
    this->cancelOrderTarget = "/api/v3/order";
    this->getOrderTarget = "/api/v3/order";
    this->getOpenOrdersTarget = "/api/v3/openOrders";
    this->cancelOpenOrdersTarget = "/api/v3/openOrders";
    this->listenKeyTarget = CCAPI_BINANCE_LISTEN_KEY_PATH;
    this->getAccountBalancesTarget = "/api/v3/account";
    this->createOrderMarginTarget = "/sapi/v1/margin/order";
    this->cancelOrderMarginTarget = "/sapi/v1/margin/order";
    this->getOrderMarginTarget = "/sapi/v1/margin/order";
    this->getOpenOrdersMarginTarget = "/sapi/v1/margin/openOrders";
    this->cancelOpenOrdersMarginTarget = "/sapi/v1/margin/openOrders";
    this->getAccountBalancesCrossMarginTarget = "/sapi/v1/margin/account";
    this->getAccountBalancesIsolatedMarginTarget = "/sapi/v1/margin/isolated/account";
    this->listenKeyCrossMarginTarget = CCAPI_BINANCE_LISTEN_KEY_CROSS_MARGIN_PATH;
    this->listenKeyIsolatedMarginTarget = CCAPI_BINANCE_LISTEN_KEY_ISOLATED_MARGIN_PATH;
  }

  virtual ~ExecutionManagementServiceBinance() {}
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_H_
