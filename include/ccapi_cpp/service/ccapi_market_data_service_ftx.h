#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_market_data_service_ftx_base.h"
namespace ccapi {
class MarketDataServiceFtx : public MarketDataServiceFtxBase {
 public:
  MarketDataServiceFtx(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                       std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceFtxBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
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
  }
  virtual ~MarketDataServiceFtx() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
