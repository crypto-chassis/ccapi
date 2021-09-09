#ifndef TEST_TEST_UNIT_INCLUDE_CCAPI_CPP_CCAPI_TEST_MARKET_DATA_HELPER_H_
#define TEST_TEST_UNIT_INCLUDE_CCAPI_CPP_CCAPI_TEST_MARKET_DATA_HELPER_H_
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGeneric final : public MarketDataService {
 public:
  MarketDataServiceGeneric(std::function<void(Event&, Queue<Event>*)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                           std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  using MarketDataService::updateOrderBook;
};
} /* namespace ccapi */
#endif  // TEST_TEST_UNIT_INCLUDE_CCAPI_CPP_CCAPI_TEST_MARKET_DATA_HELPER_H_
