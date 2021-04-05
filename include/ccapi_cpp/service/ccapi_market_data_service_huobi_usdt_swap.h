#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_USDT_SWAP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_USDT_SWAP_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#include "ccapi_cpp/service/ccapi_market_data_service_huobi_base.h"
namespace ccapi {
class MarketDataServiceHuobiUsdtSwap CCAPI_FINAL : public MarketDataServiceHuobiBase {
 public:
  MarketDataServiceHuobiUsdtSwap(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceHuobiBase(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
    this->isDerivatives = true;
    this->getRecentTradesTarget = CCAPI_HUOBI_USDT_SWAP_GET_RECENT_TRADES_TARGET;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_USDT_SWAP_H_
