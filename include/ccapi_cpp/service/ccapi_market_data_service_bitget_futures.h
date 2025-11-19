#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_bitget_base.h"

namespace ccapi {

class MarketDataServiceBitgetFutures : public MarketDataServiceBitgetBase {
 public:
  MarketDataServiceBitgetFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 ServiceContext* serviceContextPtr)
      : MarketDataServiceBitgetBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITGET_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v2/ws/public";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->getRecentTradesTarget = "/api/v2/mix/market/fills";
    this->getHistoricalTradesTarget = "/api/v2/mix/market/fills-history";
    this->getInstrumentTarget = "/api/v2/mix/market/contracts";
    this->getInstrumentsTarget = "/api/v2/mix/market/contracts";
    this->getRecentCandlesticksTarget = "/api/v2/mix/market/candles";
    this->getHistoricalCandlesticksTarget = "/api/v2/mix/market/candles";
    this->getBbosTarget = "/api/v2/mix/market/tickers";
    this->getBbosTarget = "/api/v2/mix/market/ticker";
    this->isDerivatives = true;
  }

  virtual ~MarketDataServiceBitgetFutures() {}
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_FUTURES_H_
