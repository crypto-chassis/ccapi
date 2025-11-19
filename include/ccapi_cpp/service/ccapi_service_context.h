#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_CONTEXT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_CONTEXT_H_

#include "boost/asio/ssl.hpp"
#include "ccapi_cpp/ccapi_logger.h"

namespace ccapi {

/**
 * Defines the service that the service depends on.
 */
class ServiceContext {
 public:
  typedef boost::asio::io_context IoContext;
  typedef boost::asio::io_context* IoContextPtr;
  typedef boost::asio::executor_work_guard<boost::asio::io_context::executor_type> ExecutorWorkGuard;
  typedef ExecutorWorkGuard* ExecutorWorkGuardPtr;
  typedef boost::asio::ssl::context SslContext;
  typedef SslContext* SslContextPtr;

  ServiceContext() {
    this->ioContextPtr = new boost::asio::io_context();
    this->useInternalIoContextPtr = true;
    this->executorWorkGuardPtr = new ExecutorWorkGuard(this->ioContextPtr->get_executor());
    this->sslContextPtr = new SslContext(SslContext::tls_client);
    this->useInternalSslContextPtr = true;
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
    this->useInternalSslContextPtr = true;
    this->sslContextPtr->set_verify_mode(boost::asio::ssl::verify_none);
  }

  ServiceContext(SslContextPtr sslContextPtr) {
    this->ioContextPtr = new boost::asio::io_context();
    this->useInternalIoContextPtr = true;
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
    if (this->useInternalIoContextPtr) {
      delete this->ioContextPtr;
    }
    if (this->useInternalSslContextPtr) {
      delete this->sslContextPtr;
    }
  }

  void start() {
    if (this->useInternalIoContextPtr) {
      std::thread thread([this]() {
        CCAPI_LOGGER_INFO("about to start asio io_context run loop");
        this->ioContextPtr->run();
        CCAPI_LOGGER_INFO("just exited asio io_context run loop");
      });
      this->thread = std::move(thread);
    }
  }

  void stop() {
    this->executorWorkGuardPtr->reset();
    if (this->useInternalIoContextPtr) {
      this->ioContextPtr->stop();
      this->thread.join();
    }
  }

  IoContextPtr ioContextPtr{nullptr};
  bool useInternalIoContextPtr{};
  ExecutorWorkGuardPtr executorWorkGuardPtr{nullptr};
  SslContextPtr sslContextPtr{nullptr};
  bool useInternalSslContextPtr{};
  std::thread thread;
};

} /* namespace ccapi */

#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_CONTEXT_H_
