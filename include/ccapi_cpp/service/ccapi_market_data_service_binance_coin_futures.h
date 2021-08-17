#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_COIN_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_COIN_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_binance_derivatives_base.h"
namespace ccapi {
class MarketDataServiceBinanceCoinFutures : public MarketDataServiceBinanceDerivativesBase {
 public:
  MarketDataServiceBinanceCoinFutures(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceBinanceDerivativesBase(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/stream";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_BINANCE_COIN_FUTURES_API_KEY;
    this->setupCredential({this->apiKeyName});
    this->getRecentTradesTarget = "/dapi/v1/trades";
    this->getRecentAggTradesTarget = "/dapi/v1/aggTrades";
    this->getInstrumentTarget = "/dapi/v1/exchangeInfo";
    this->getInstrumentsTarget = "/dapi/v1/exchangeInfo";
  }
  virtual ~MarketDataServiceBinanceCoinFutures() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_COIN_FUTURES_H_
