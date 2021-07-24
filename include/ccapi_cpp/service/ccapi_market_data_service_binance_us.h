#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_US_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_US_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
#include "ccapi_cpp/service/ccapi_market_data_service_binance_base.h"
namespace ccapi {
class MarketDataServiceBinanceUs : public MarketDataServiceBinanceBase {
 public:
  MarketDataServiceBinanceUs(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceBinanceBase(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_US;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/stream";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_BINANCE_US_API_KEY;
    this->setupCredential({this->apiKeyName});
    this->getRecentTradesTarget = "/api/v3/trades";
    this->getRecentAggTradesTarget = "/api/v3/aggTrades";
    this->getInstrumentTarget="/api/v3/exchangeInfo";
  }
  virtual ~MarketDataServiceBinanceUs() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_US_H_
