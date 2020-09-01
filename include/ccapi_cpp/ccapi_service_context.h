#ifndef INCLUDE_CCAPI_CPP_CCAPI_SERVICE_CONTEXT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SERVICE_CONTEXT_H_
#ifndef WEBSOCKETPP_USE_BOOST_ASIO
#define ASIO_STANDALONE
#endif
#include "ccapi_cpp/ccapi_logger.h"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/client.hpp"
namespace wspp = websocketpp;
namespace ccapi {

class ServiceContext final {
 public:
  typedef wspp::lib::asio::io_service IoContext;
  struct CustomClientConfig : public wspp::config::asio_tls_client {
    static const wspp::log::level alog_level = wspp::log::alevel::none;
    static const wspp::log::level elog_level = wspp::log::elevel::none;
  };
  typedef wspp::client<CustomClientConfig> TlsClient;
  typedef wspp::lib::error_code ErrorCode;
  ServiceContext() {}
  ServiceContext(const ServiceContext&) = delete;
  ServiceContext& operator=(const ServiceContext&) = delete;
  void initialize() {
    this->tlsClient.clear_access_channels(websocketpp::log::alevel::all);
    this->tlsClient.clear_error_channels(websocketpp::log::elevel::all);
    ErrorCode ec;
    CCAPI_LOGGER_DEBUG("asio initialization start");
    this->tlsClient.init_asio(&this->ioContext, ec);
    if (ec) {
      CCAPI_LOGGER_FATAL("asio initialization error: " + ec.message());
    }
    CCAPI_LOGGER_DEBUG("asio initialization end");
    this->tlsClient.start_perpetual();
  }
  void run() {
    CCAPI_LOGGER_INFO("about to start client asio io_service run loop");
    this->tlsClient.run();
    CCAPI_LOGGER_INFO("just exited client asio io_service run loop");
  }
  IoContext ioContext;
  TlsClient tlsClient;
};

} /* namespace ccapi */

#endif /* INCLUDE_CCAPI_CPP_CCAPI_SERVICE_CONTEXT_H_ */
