#ifndef INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_BINANCE
#include "ccapi_cpp/ccapi_market_data_service_binance_base.h"
namespace ccapi {
class MarketDataServiceBinance final : public MarketDataServiceBinanceBase {
 public:
  MarketDataServiceBinance(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContext& serviceContext): MarketDataServiceBinanceBase(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContext) {
    this->name = CCAPI_EXCHANGE_NAME_BINANCE;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
