#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_bitget_base.h"
namespace ccapi {
class MarketDataServiceBitgetFutures : public MarketDataServiceBitgetBase {
 public:
  MarketDataServiceBitgetFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                 std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceBitgetBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITGET_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/mix/v1/stream";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->getRecentTradesTarget = "/api/mix/v1/market/fills";
    this->getInstrumentTarget = "/api/mix/v1/market/contracts";
    this->getInstrumentsTarget = "/api/mix/v1/market/contracts";
    this->isDerivatives = true;
  }
  virtual ~MarketDataServiceBitgetFutures() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_FUTURES_H_
