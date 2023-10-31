#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_CONTEXT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_CONTEXT_H_
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#include "ccapi_cpp/ccapi_logger.h"
#include "websocketpp/client.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/config/boost_config.hpp"
namespace wspp = websocketpp;
namespace ccapi {
/**
 * Defines the service that the service depends on.
 */
class ServiceContext CCAPI_FINAL {
 public:
  typedef wspp::lib::asio::io_service IoContext;
  typedef wspp::lib::shared_ptr<wspp::lib::asio::io_service> IoContextPtr;
  struct CustomClientConfig : public wspp::config::asio_tls_client {
#ifdef CCAPI_USE_SINGLE_THREAD
    typedef wspp::config::asio_tls_client base;
    static bool const enable_multithreading = false;
    struct transport_config : public base::transport_config {
      static bool const enable_multithreading = false;
    };
#endif
#ifdef CCAPI_WEBSOCKET_READ_BUFFER_SIZE
    static const size_t connection_read_buffer_size = CCAPI_WEBSOCKET_READ_BUFFER_SIZE;
#endif
    static const wspp::log::level alog_level = wspp::log::alevel::none;
    static const wspp::log::level elog_level = wspp::log::elevel::none;
  };
  typedef wspp::client<CustomClientConfig> TlsClient;
  typedef wspp::lib::shared_ptr<wspp::client<CustomClientConfig> > TlsClientPtr;
  typedef wspp::lib::error_code ErrorCode;
  typedef wspp::lib::asio::ssl::context SslContext;
  typedef wspp::lib::shared_ptr<SslContext> SslContextPtr;
  ServiceContext() {
    this->tlsClientPtr->set_access_channels(websocketpp::log::alevel::none);
    this->tlsClientPtr->set_error_channels(websocketpp::log::elevel::none);
    ErrorCode ec;
    CCAPI_LOGGER_DEBUG("asio initialization start");
    this->tlsClientPtr->init_asio(this->ioContextPtr.get(), ec);
    if (ec) {
      CCAPI_LOGGER_FATAL("asio initialization error: " + ec.message());
    }
    CCAPI_LOGGER_DEBUG("asio initialization end");
    this->sslContextPtr->set_options(SslContext::default_workarounds | SslContext::no_sslv2 | SslContext::no_sslv3 | SslContext::single_dh_use);
    this->sslContextPtr->set_verify_mode(wspp::lib::asio::ssl::verify_none);
    // TODO(cryptochassis): verify ssl certificate to strengthen security
    // https://github.com/boostorg/asio/blob/develop/example/cpp03/ssl/client.cpp
  }
  ServiceContext(const ServiceContext&) = delete;
  ServiceContext& operator=(const ServiceContext&) = delete;
  void start() {
    CCAPI_LOGGER_INFO("about to start client asio io_service run loop");
    this->tlsClientPtr->start_perpetual();
    this->tlsClientPtr->run();
    CCAPI_LOGGER_INFO("just exited client asio io_service run loop");
  }
  void stop() {
    this->ioContextPtr->stop();
    this->tlsClientPtr->stop();
    this->tlsClientPtr->stop_perpetual();
  }
  IoContextPtr ioContextPtr{new IoContext()};
  TlsClientPtr tlsClientPtr{new TlsClient()};
  SslContextPtr sslContextPtr{new SslContext(SslContext::tls_client)};
};

} /* namespace ccapi */
#else
#include "ccapi_cpp/ccapi_logger.h"
namespace ccapi {
/**
 * Defines the service that the service depends on.
 */
class ServiceContext CCAPI_FINAL {
 public:
  typedef boost::asio::io_context IoContext;
  typedef boost::asio::io_context* IoContextPtr;
  typedef boost::asio::executor_work_guard<boost::asio::io_context::executor_type> ExecutorWorkGuard;
  typedef ExecutorWorkGuard* ExecutorWorkGuardPtr;
  typedef boost::asio::ssl::context SslContext;
  typedef SslContext* SslContextPtr;
  ServiceContext() {
    this->ioContextPtr = new boost::asio::io_context();
    this->executorWorkGuardPtr = new ExecutorWorkGuard(this->ioContextPtr->get_executor());
    this->sslContextPtr = new SslContext(SslContext::tls_client);
    // this->sslContextPtr->set_options(SslContext::default_workarounds | SslContext::no_sslv2 | SslContext::no_sslv3 | SslContext::single_dh_use);
    this->sslContextPtr->set_verify_mode(boost::asio::ssl::verify_none);
    // TODO(cryptochassis): verify ssl certificate to strengthen security
    // https://github.com/boostorg/asio/blob/develop/example/cpp03/ssl/client.cpp
  }
#ifndef SWIG
  ServiceContext(IoContextPtr ioContextPtr) {
    this->ioContextPtr = ioContextPtr;
    this->executorWorkGuardPtr = new ExecutorWorkGuard(this->ioContextPtr->get_executor());
    this->sslContextPtr = new SslContext(SslContext::tls_client);
    this->sslContextPtr->set_verify_mode(boost::asio::ssl::verify_none);
  }
  ServiceContext(SslContextPtr sslContextPtr) {
    this->ioContextPtr = new boost::asio::io_context();
    this->executorWorkGuardPtr = new ExecutorWorkGuard(this->ioContextPtr->get_executor());
    this->sslContextPtr = sslContextPtr;
    this->sslContextPtr->set_verify_mode(boost::asio::ssl::verify_none);
  }
  ServiceContext(IoContextPtr ioContextPtr, SslContextPtr sslContextPtr) {
    this->ioContextPtr = ioContextPtr;
    this->executorWorkGuardPtr = new ExecutorWorkGuard(this->ioContextPtr->get_executor());
    this->sslContextPtr = sslContextPtr;
    this->sslContextPtr->set_verify_mode(boost::asio::ssl::verify_none);
  }
#endif
  ServiceContext(const ServiceContext&) = delete;
  ServiceContext& operator=(const ServiceContext&) = delete;
  virtual ~ServiceContext() {
    delete this->executorWorkGuardPtr;
    delete this->ioContextPtr;
    delete this->sslContextPtr;
  }
  void start() {
    CCAPI_LOGGER_INFO("about to start client asio io_context run loop");
    this->ioContextPtr->run();
    CCAPI_LOGGER_INFO("just exited client asio io_context run loop");
  }
  void stop() {
    this->executorWorkGuardPtr->reset();
    this->ioContextPtr->stop();
  }
  IoContextPtr ioContextPtr{nullptr};
  ExecutorWorkGuardPtr executorWorkGuardPtr{nullptr};
  SslContextPtr sslContextPtr{nullptr};
  // IoContextPtr ioContextPtr{new IoContext()};
  // ExecutorWorkGuardPtr executorWorkGuardPtr{new ExecutorWorkGuard(ioContextPtr->get_executor())};
  // SslContextPtr sslContextPtr{new SslContext(SslContext::tls_client)};
};

} /* namespace ccapi */

#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_CONTEXT_H_
