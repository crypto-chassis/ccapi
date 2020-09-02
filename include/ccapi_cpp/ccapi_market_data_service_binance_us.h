#ifndef INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BINANCE_US_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BINANCE_US_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_BINANCE_US
#include "ccapi_cpp/ccapi_market_data_service_binance_base.h"
namespace ccapi {
class MarketDataServiceBinanceUs final : public MarketDataServiceBinanceBase {
 public:
  MarketDataServiceBinanceUs(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContext& serviceContext): MarketDataServiceBinanceBase(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContext) {
    this->name = CCAPI_EXCHANGE_NAME_BINANCE_US;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BINANCE_US_H_
