#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_KUCOIN)
#include "ccapi_cpp/service/ccapi_market_data_service_kucoin_base.h"
namespace ccapi {
class MarketDataServiceKucoin : public MarketDataServiceKucoinBase {
 public:
  MarketDataServiceKucoin(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceKucoinBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KUCOIN;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->getRecentTradesTarget = "/api/v1/market/histories";
    this->getInstrumentTarget = "/api/v1/symbols";
    this->getInstrumentsTarget = "/api/v1/symbols";
    this->channelMarketTicker = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_TICKER;
    this->channelMarketLevel2Depth5 = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2DEPTH5;
    this->channelMarketLevel2Depth50 = CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2DEPTH50;
    this->tickerSubject = "trade.ticker";
    this->tickerBestBidPriceKey = "bestBid";
    this->tickerBestAskPriceKey = "bestAsk";
    this->matchSubject = "trade.l3match";
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
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
