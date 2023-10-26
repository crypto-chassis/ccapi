#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_KUCOIN)
#include "ccapi_cpp/service/ccapi_market_data_service_kucoin_base.h"
namespace ccapi {
class MarketDataServiceKucoin : public MarketDataServiceKucoinBase {
 public:
  MarketDataServiceKucoin(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          ServiceContext* serviceContextPtr)
      : MarketDataServiceKucoinBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KUCOIN;
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
    this->apiKeyName = CCAPI_KUCOIN_API_KEY;
    this->apiSecretName = CCAPI_KUCOIN_API_SECRET;
    this->apiPassphraseName = CCAPI_KUCOIN_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->getRecentTradesTarget = "/api/v1/market/histories";
    this->getHistoricalTradesTarget = "/api/v1/market/histories";
    this->getRecentCandlesticksTarget = "/api/v1/market/candles";
    this->getHistoricalCandlesticksTarget = "/api/v1/market/candles";
    this->getMarketDepthTarget = "/api/v1/market/orderbook/level2";
    this->getInstrumentTarget = "/api/v1/symbols";
    this->getInstrumentsTarget = "/api/v1/symbols";
    this->channelMarketTicker = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_TICKER;
    this->channelMarketLevel2Depth5 = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2DEPTH5;
    this->channelMarketLevel2Depth50 = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2DEPTH50;
    this->channelMarketLevel2 = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2;
    this->channelMarketKlines = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_KLINES;
    this->tickerSubject = "trade.ticker";
    this->tickerBestBidPriceKey = "bestBid";
    this->tickerBestAskPriceKey = "bestAsk";
    this->matchSubject = "trade.l3match";
    this->klineSubject = "trade.candles.update";
    this->level2Subject = "level2";
    this->recentTradesTimeKey = "time";
  }
  virtual ~MarketDataServiceKucoin() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void extractInstrumentInfo(Element& element, const rj::Value& x) override {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseCurrency"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteCurrency"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["priceIncrement"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["baseIncrement"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["baseMinSize"].GetString());
    element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, x["quoteMinSize"].GetString());
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
