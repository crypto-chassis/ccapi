#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/service/ccapi_market_data_service_binance_base.h"
namespace ccapi {
class MarketDataServiceBinance CCAPI_FINAL : public MarketDataServiceBinanceBase {
 public:
  MarketDataServiceBinance(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions,
                           SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceBinanceBase(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_BINANCE;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
