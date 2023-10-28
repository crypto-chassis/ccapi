#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service_kucoin_base.h"
namespace ccapi {
class MarketDataServiceKucoinFutures : public MarketDataServiceKucoinBase {
 public:
  MarketDataServiceKucoinFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 ServiceContext* serviceContextPtr)
      : MarketDataServiceKucoinBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    // try {
    //   this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    // } catch (const std::exception& e) {
    //   CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    // }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->getRecentTradesTarget = "/api/v1/trade/history";
    this->getHistoricalTradesTarget = "/api/v1/trade/history";
    this->getRecentCandlesticksTarget = "/api/v1/kline/query";
    this->getHistoricalCandlesticksTarget = "/api/v1/kline/query";
    this->getMarketDepthTarget = "/api/v1/level2/depth";
    this->getInstrumentTarget = "/api/v1/contracts/active";
    this->getInstrumentsTarget = "/api/v1/contracts/active";
    this->isDerivatives = true;
    this->channelMarketTicker = CCAPI_WEBSOCKET_KUCOIN_FUTURES_CHANNEL_MARKET_TICKER;
    this->channelMarketLevel2Depth5 = CCAPI_WEBSOCKET_KUCOIN_FUTURES_CHANNEL_MARKET_LEVEL2DEPTH5;
    this->channelMarketLevel2Depth50 = CCAPI_WEBSOCKET_KUCOIN_FUTURES_CHANNEL_MARKET_LEVEL2DEPTH50;
    this->tickerSubject = "tickerV2";
    this->tickerBestBidPriceKey = "bestBidPrice";
    this->tickerBestAskPriceKey = "bestAskPrice";
    this->matchSubject = "match";
    this->level2Subject = "level2";
    this->recentTradesTimeKey = "ts";
  }
  virtual ~MarketDataServiceKucoinFutures() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void extractInstrumentInfo(Element& element, const rj::Value& x) override {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_UNDERLYING_SYMBOL, x["indexSymbol"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["tickSize"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSize"].GetString());
    element.insert(CCAPI_CONTRACT_MULTIPLIER, x["multiplier"].GetString());
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_FUTURES_H_
