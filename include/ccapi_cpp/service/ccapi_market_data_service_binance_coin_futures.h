#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_COIN_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_COIN_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_binance_derivatives_base.h"
namespace ccapi {
class MarketDataServiceBinanceCoinFutures : public MarketDataServiceBinanceDerivativesBase {
 public:
  MarketDataServiceBinanceCoinFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      ServiceContext* serviceContextPtr)
      : MarketDataServiceBinanceDerivativesBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/stream";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    //     try {
    //       this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->apiKeyName = CCAPI_BINANCE_COIN_FUTURES_API_KEY;
    this->setupCredential({this->apiKeyName});
    this->getRecentTradesTarget = "/dapi/v1/trades";
    this->getHistoricalTradesTarget = "/dapi/v1/historicalTrades";
    this->getRecentAggTradesTarget = "/dapi/v1/aggTrades";
    this->getHistoricalAggTradesTarget = "/dapi/v1/aggTrades";
    this->getRecentCandlesticksTarget = "/dapi/v1/klines";
    this->getHistoricalCandlesticksTarget = "/dapi/v1/klines";
    this->getMarketDepthTarget = "/dapi/v1/depth";
    this->getInstrumentTarget = "/dapi/v1/exchangeInfo";
    this->getInstrumentsTarget = "/dapi/v1/exchangeInfo";
  }
  virtual ~MarketDataServiceBinanceCoinFutures() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_COIN_FUTURES_H_
