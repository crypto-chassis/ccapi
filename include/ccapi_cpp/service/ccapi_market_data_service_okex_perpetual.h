#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_PERPETUAL_SWAP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_PERPETUAL_SWAP_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX_PERPETUAL_SWAP
#include "ccapi_cpp/service/ccapi_market_data_service_okex_base.h"
namespace ccapi {
class MarketDataServiceOkexPerpetualSwap CCAPI_FINAL : public MarketDataServiceOkexBase {
 public:
  MarketDataServiceOkexPerpetualSwap(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataServiceOkexBase(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_OKEX_PERPETUAL_SWAP;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
    this->channelTrade = CCAPI_WEBSOCKET_OKEX_PERPETUAL_SWAP_CHANNEL_TRADE;
    this->channelPublicDepth5 = CCAPI_WEBSOCKET_OKEX_PERPETUAL_SWAP_CHANNEL_PUBLIC_DEPTH5;
    this->channelPublicDepth400 = CCAPI_WEBSOCKET_OKEX_PERPETUAL_SWAP_CHANNEL_PUBLIC_DEPTH400;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_PERPETUAL_SWAP_H_
