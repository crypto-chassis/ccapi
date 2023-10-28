#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/service/ccapi_market_data_service_binance_base.h"
namespace ccapi {
class MarketDataServiceBinance : public MarketDataServiceBinanceBase {
 public:
  MarketDataServiceBinance(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                           ServiceContext* serviceContextPtr)
      : MarketDataServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE;
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
    this->apiKeyName = CCAPI_BINANCE_API_KEY;
    this->setupCredential({this->apiKeyName});
    this->getRecentTradesTarget = "/api/v3/trades";
    this->getHistoricalTradesTarget = "/api/v3/historicalTrades";
    this->getRecentAggTradesTarget = "/api/v3/aggTrades";
    this->getHistoricalAggTradesTarget = "/api/v3/aggTrades";
    this->getRecentCandlesticksTarget = "/api/v3/klines";
    this->getHistoricalCandlesticksTarget = "/api/v3/klines";
    this->getMarketDepthTarget = "/api/v3/depth";
    this->getInstrumentTarget = "/api/v3/exchangeInfo";
    this->getInstrumentsTarget = "/api/v3/exchangeInfo";
  }
  virtual ~MarketDataServiceBinance() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_H_
