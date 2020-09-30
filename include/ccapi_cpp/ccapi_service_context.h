#ifndef INCLUDE_CCAPI_CPP_CCAPI_SERVICE_CONTEXT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SERVICE_CONTEXT_H_
#include "ccapi_cpp/ccapi_logger.h"
namespace wspp = websocketpp;
namespace ccapi {

class ServiceContext final {
 public:
  typedef wspp::lib::asio::io_service IoContext;
  struct CustomClientConfig : public wspp::config::asio_tls_client {
    #ifdef WEBSOCKETPP_ENABLE_SINGLE_THREADING
      typedef wspp::config::asio_tls_client base;
      static bool const enable_multithreading = false;
      struct transport_config : public base::transport_config {
        static bool const enable_multithreading = false;
      };
    #endif
    static const wspp::log::level alog_level = wspp::log::alevel::none;
    static const wspp::log::level elog_level = wspp::log::elevel::none;
  };
  typedef wspp::client<CustomClientConfig> TlsClient;
  typedef wspp::lib::error_code ErrorCode;
  typedef wspp::lib::shared_ptr<wspp::lib::asio::ssl::context> SslContext;
  ServiceContext() {}
  ServiceContext(const ServiceContext&) = delete;
  ServiceContext& operator=(const ServiceContext&) = delete;
  void initialize() {
    this->tlsClient.set_access_channels(websocketpp::log::alevel::none);
    this->tlsClient.set_error_channels(websocketpp::log::elevel::none);
    ErrorCode ec;
    CCAPI_LOGGER_DEBUG("asio initialization start");
    this->tlsClient.init_asio(&this->ioContext, ec);
    if (ec) {
      CCAPI_LOGGER_FATAL("asio initialization error: " + ec.message());
    }
    CCAPI_LOGGER_DEBUG("asio initialization end");
    this->tlsClient.start_perpetual();
    this->sslContext = std::make_shared<wspp::lib::asio::ssl::context>(wspp::lib::asio::ssl::context::sslv23);
    this->sslContext->set_options(
        wspp::lib::asio::ssl::context::default_workarounds | wspp::lib::asio::ssl::context::no_sslv2 | wspp::lib::asio::ssl::context::no_sslv3
            | wspp::lib::asio::ssl::context::single_dh_use);
    this->sslContext->set_verify_mode(wspp::lib::asio::ssl::verify_none);
    // TODO(cryptochassis): verify ssl certificate to strengthen security
    // https://github.com/boostorg/asio/blob/develop/example/cpp03/ssl/client.cpp
  }
  void run() {
    CCAPI_LOGGER_INFO("about to start client asio io_service run loop");
    this->tlsClient.run();
    CCAPI_LOGGER_INFO("just exited client asio io_service run loop");
  }
  IoContext ioContext;
  TlsClient tlsClient;
  SslContext sslContext;
};

} /* namespace ccapi */

#endif  // INCLUDE_CCAPI_CPP_CCAPI_SERVICE_CONTEXT_H_
