#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_BYBIT) || defined(CCAPI_ENABLE_EXCHANGE_BYBIT_DERIVATIVES)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceBybitBase : public MarketDataService {
 public:
  MarketDataServiceBybitBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~MarketDataServiceBybitBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("retCode":0)") == std::string::npos; }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, R"({"op":"ping"})", ec); }
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_BASE_H_
