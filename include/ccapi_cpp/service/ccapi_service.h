#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
// #ifndef RAPIDJSON_ASSERT
// #define RAPIDJSON_ASSERT(x) if (!(x)) { throw std::runtime_error("rapidjson internal assertion failure"); }
// #endif
#include "websocketpp/config/boost_config.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_market_data_message.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/client.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)
#include <sstream>
#include <iomanip>
#include "ccapi_cpp/websocketpp_decompress_workaround.h"
#endif
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_subscription.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_http_connection.h"
#include "ccapi_cpp/ccapi_ws_connection.h"
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
namespace wspp = websocketpp;
namespace rj = rapidjson;
namespace ccapi {
class Service : public std::enable_shared_from_this<Service> {
 public:
  typedef wspp::lib::shared_ptr<ServiceContext> ServiceContextPtr;
  typedef wspp::lib::error_code ErrorCode;
  Service(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      :
        eventHandler(eventHandler),
        sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs), serviceContextPtr(serviceContextPtr), resolver(*serviceContextPtr->ioContextPtr) {
  }
  virtual ~Service() {
  }
  void setEventHandler(const std::function<void(Event& event)>& eventHandler) {
    this->eventHandler = eventHandler;
  }
  virtual void stop() = 0;
  virtual void subscribe(const std::vector<Subscription>& subscriptionList) = 0;
  virtual std::shared_ptr<std::future<void> > sendRequest(const Request& request, const bool useFuture, const TimePoint& now) {
    return std::shared_ptr<std::future<void> >(nullptr);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::string& errorMessage) {
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    Event event;
    event.setType(eventType);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(messageType);
    Element element;
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const ErrorCode& ec, const std::string& what) {
    this->onError(eventType, messageType, what + ": " + ec.message() + ", category: " + ec.category().name());
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::exception& e) {
    this->onError(eventType, messageType, e.what());
  }
  void onResponseError(int statusCode, const std::string& errorMessage) {
    std::string statusCodeStr = std::to_string(statusCode);
    CCAPI_LOGGER_ERROR("statusCode = " + statusCodeStr + ", errorMessage = " + errorMessage);
    Event event;
    event.setType(Event::Type::REQUEST_STATUS);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(Message::Type::RESPONSE_ERROR);
    Element element;
    element.insert(CCAPI_HTTP_STATUS_CODE, statusCodeStr);
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
  }

 protected:
  std::pair<std::string, std::string> extractHostFromUrl(std::string baseUrlRest) {
    std::string host;
    std::string port;
    auto splitted1 = UtilString::split(baseUrlRest, "://");
    auto splitted2 = UtilString::split(UtilString::split(splitted1[1], "/")[0], ":");
    host = splitted2[0];
    if (splitted2.size() == 2) {
      port = splitted2[1];
    } else {
      if (splitted1[0] == "https") {
        port = CCAPI_HTTPS_PORT_DEFAULT;
      } else {
        port = CCAPI_HTTP_PORT_DEFAULT;
      }
    }
    return std::make_pair(host, port);
  }
  template <typename Derived>
  std::shared_ptr<Derived> shared_from_base() {
      return std::static_pointer_cast<Derived>(shared_from_this());
  }
  void sendRequest(const std::string& host, const std::string& port,
      const http::request<http::string_body>& req,
      std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
      long timeoutMilliSeconds) {
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n"+oss.str());
#endif
    std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(nullptr);
    try {
      streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, host);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(host, port, streamPtr));
    CCAPI_LOGGER_DEBUG("httpConnection = "+toString(*httpConnectionPtr));
    this->resolver.async_resolve(
        host,
        port,
        beast::bind_front_handler(&Service::onResolve, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliSeconds));
  }
  void onResolve(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req, std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                 long timeoutMilliSeconds, beast::error_code ec, tcp::resolver::results_type tcpResolverResults) {
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(timeoutMilliSeconds));
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
      tcpResolverResults,
      beast::bind_front_handler(&Service::onConnect, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req, std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                 beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("connected");
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(
      ssl::stream_base::client,
      beast::bind_front_handler(&Service::onHandshake, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req, std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    std::shared_ptr<http::request<http::string_body> > reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr, beast::bind_front_handler(&Service::onWrite, shared_from_this(), httpConnectionPtr, reqPtr, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body> > reqPtr, std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_write callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("written");
    std::shared_ptr<beast::flat_buffer> bufferPtr(new beast::flat_buffer());
    std::shared_ptr<http::response<http::string_body> > resPtr(new http::response < http::string_body >());
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    http::async_read(stream, *bufferPtr, *resPtr, beast::bind_front_handler(&Service::onRead, shared_from_this(), httpConnectionPtr, reqPtr, errorHandler, responseHandler, bufferPtr, resPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body> > reqPtr, std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler, std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body> > resPtr, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_shutdown");
    stream.async_shutdown(
      beast::bind_front_handler(
        &Service::onShutdown,
        shared_from_this(), httpConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_shutdown");
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << *resPtr;
    CCAPI_LOGGER_DEBUG("res = \n"+oss.str());
#endif
    responseHandler(*resPtr);
  }
  void onShutdown(std::shared_ptr<HttpConnection> httpConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_shutdown callback start");
    if (ec == net::error::eof) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
    }
    if (ec) {
      CCAPI_LOGGER_TRACE("fail: " + ec.message() + ", category: " + ec.category().name());
      return;
    }
    CCAPI_LOGGER_TRACE("shutdown");
    // If we get here then the connection is closed gracefully
  }
  std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr, const std::string& host) {
    std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(new beast::ssl_stream <beast::tcp_stream>(*iocPtr, *ctxPtr));
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
      beast::error_code ec {static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }
    return streamPtr;
  }
  std::string name;
  std::string baseUrl;
  std::string baseUrlRest;
  std::function<void(Event& event)> eventHandler;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  ServiceContextPtr serviceContextPtr;
  tcp::resolver resolver;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
