#ifndef TEST_TEST_UNIT_INCLUDE_CCAPI_CPP_CCAPI_TEST_MARKET_DATA_HELPER_H_
#define TEST_TEST_UNIT_INCLUDE_CCAPI_CPP_CCAPI_TEST_MARKET_DATA_HELPER_H_
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGeneric final : public MarketDataService {
public:
 MarketDataServiceGeneric(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                       std::shared_ptr<ServiceContext> serviceContextPtr)
     : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {

     }
     using MarketDataService::updateOrderBook;
   private:
     void subscribe(const std::vector<Subscription>& subscriptionList) override {}
     void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now,
                             const std::string& symbolId, const std::map<std::string, std::string>& credential) override {}
     void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {}
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage,
                                                                               const TimePoint& timeReceived) override{
                                                                                 std::vector<MarketDataMessage> x;
                                                                                 return x;
                                                                               }
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override{
    std::vector<std::string> x;
    return x;
  }
  std::vector<MarketDataMessage> processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage,
                                                            const TimePoint& timeReceived) override{
                                                              std::vector<MarketDataMessage> x;
                                                              return x;
                                                            }
};
} /* namespace ccapi */
#endif  // TEST_TEST_UNIT_INCLUDE_CCAPI_CPP_CCAPI_TEST_MARKET_DATA_HELPER_H_
