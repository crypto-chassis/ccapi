#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
#include "ccapi_cpp/service/ccapi_market_data_service_gateio_base.h"
namespace ccapi {
class MarketDataServiceGateio : public MarketDataServiceGateioBase {
 public:
  MarketDataServiceGateio(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceGateioBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GATEIO;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/v4/";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_GATEIO_API_KEY;
    this->setupCredential({this->apiKeyName});
    std::string prefix = "/api/v4";
    this->getRecentTradesTarget = prefix + "/spot/trades";
    this->getInstrumentTarget = prefix + "/spot/currency_pairs/{currency_pair}";
    this->getInstrumentsTarget = prefix + "/spot/currency_pairs";
  }
  virtual ~MarketDataServiceGateio() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_H_
