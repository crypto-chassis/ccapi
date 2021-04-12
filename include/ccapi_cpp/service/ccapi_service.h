#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#include "ccapi_cpp/ccapi_logger.h"
#define RAPIDJSON_HAS_CXX11_NOEXCEPT 0
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x)                                           \
  if (!(x)) {                                                         \
    throw std::runtime_error("rapidjson internal assertion failure"); \
  }
#endif
// #ifndef RAPIDJSON_NOEXCEPT_ASSERT
// #define RAPIDJSON_NOEXCEPT_ASSERT(x)                            \
//   if (!(x)) {                                                   \
//     CCAPI_LOGGER_ERROR("rapidjson internal assertion failure"); \
//   }
// #endif
#ifndef RAPIDJSON_PARSE_ERROR_NORETURN
#define RAPIDJSON_PARSE_ERROR_NORETURN(parseErrorCode, offset) throw std::runtime_error(#parseErrorCode)
#endif
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_market_data_message.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "websocketpp/config/boost_config.hpp"

#include "websocketpp/client.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/config/asio_client.hpp"
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)
#include <iomanip>
#include <sstream>
#include "ccapi_cpp/websocketpp_decompress_workaround.h"
#endif
#include "ccapi_cpp/ccapi_http_connection.h"
#include "ccapi_cpp/ccapi_http_retry.h"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_subscription.h"
#include "ccapi_cpp/ccapi_url.h"
#include "ccapi_cpp/ccapi_ws_connection.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
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
  Service(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : eventHandler(eventHandler),
        sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs),
        serviceContextPtr(serviceContextPtr),
        resolver(*serviceContextPtr->ioContextPtr),
        httpConnectionPool(sessionOptions.httpConnectionPoolMaxSize) {}
  virtual ~Service() {}
  void setEventHandler(const std::function<void(Event& event)>& eventHandler) { this->eventHandler = eventHandler; }
  virtual void stop() {
    for (const auto& x : this->sendRequestDelayTimerByCorrelationIdMap) {
      x.second->cancel();
    }
    sendRequestDelayTimerByCorrelationIdMap.clear();
  }
  virtual void subscribe(const std::vector<Subscription>& subscriptionList) = 0;
  virtual void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now,
                          const std::string& symbolId, const std::map<std::string, std::string>& credential) = 0;
  virtual void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) = 0;
  std::shared_ptr<std::future<void> > sendRequest(const Request& request, const bool useFuture, const TimePoint& now, long delayMilliSeconds) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("request = " + toString(request));
    CCAPI_LOGGER_DEBUG("useFuture = " + toString(useFuture));
    TimePoint then;
    if (delayMilliSeconds > 0) {
      then = now + std::chrono::milliseconds(delayMilliSeconds);
    } else {
      then = now;
    }
    auto req = this->convertRequest(request, then);
    std::promise<void>* promisePtrRaw = nullptr;
    if (useFuture) {
      promisePtrRaw = new std::promise<void>();
    }
    std::shared_ptr<std::promise<void> > promisePtr(promisePtrRaw);
    HttpRetry retry(0, 0, "", promisePtr);
    if (delayMilliSeconds > 0) {
      this->sendRequestDelayTimerByCorrelationIdMap[request.getCorrelationId()] = this->serviceContextPtr->tlsClientPtr->set_timer(delayMilliSeconds, [that = shared_from_this(), request, req, retry](ErrorCode const& ec) {
        if (ec) {
          CCAPI_LOGGER_ERROR("request = " + toString(request) + ", sendRequest timer error: " + ec.message());
          that->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, ec, "timer", {request.getCorrelationId()});
        } else {
          auto thatReq = req;
          that->tryRequest(request, thatReq, retry);
        }
        that->sendRequestDelayTimerByCorrelationIdMap.erase(request.getCorrelationId());
      });
    } else {
      this->tryRequest(request, req, retry);
    }
    std::shared_ptr<std::future<void> > futurePtr(nullptr);
    if (useFuture) {
      futurePtr = std::make_shared<std::future<void> >(std::move(promisePtr->get_future()));
    }
    return futurePtr;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  // virtual std::shared_ptr<std::future<void> > sendRequestWithDelay(const Request& request, const bool useFuture, const TimePoint& then) {
  //   auto now = UtilTime::now();
  //   long delayMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(then - now).count()
  //   if (delayMilliSeconds > 0) {
  //     this->serviceContextPtr->tlsClientPtr->set_timer(
  //         delayMilliSeconds * 1000, [that = shared_from_this(), request, useFuture, then](ErrorCode const& ec) {
  //             if (ec) {
  //               CCAPI_LOGGER_ERROR("request = " + toString(request) + ", sendRequestWithDelay timer error: " + ec.message());
  //               that->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
  //             } else {
  //               that->sendRequest(request, useFuture, then);
  //             }
  //         });
  //   } else {
  //     this->sendRequest(request, useFuture, now);
  //   }
  // }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::string& errorMessage,
               const std::vector<std::string> correlationIdList = {}) {
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    CCAPI_LOGGER_ERROR("correlationIdList = " + toString(correlationIdList));
    Event event;
    event.setType(eventType);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(messageType);
    message.setCorrelationIdList(correlationIdList);
    Element element;
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const ErrorCode& ec, const std::string& what,
               const std::vector<std::string> correlationIdList = {}) {
    this->onError(eventType, messageType, what + ": " + ec.message() + ", category: " + ec.category().name(), correlationIdList);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::exception& e, const std::vector<std::string> correlationIdList = {}) {
    this->onError(eventType, messageType, e.what(), correlationIdList);
  }
  void onResponseError(const Request& request, int statusCode, const std::string& errorMessage) {
    std::string statusCodeStr = std::to_string(statusCode);
    CCAPI_LOGGER_ERROR("statusCode = " + statusCodeStr + ", errorMessage = " + errorMessage);
    Event event;
    event.setType(Event::Type::REQUEST_STATUS);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(Message::Type::RESPONSE_ERROR);
    message.setCorrelationIdList({request.getCorrelationId()});
    Element element;
    element.insert(CCAPI_HTTP_STATUS_CODE, statusCodeStr);
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
  }

 protected:
   typedef wspp::lib::shared_ptr<wspp::lib::asio::steady_timer> TimerPtr;
  void setHostFromUrl(std::string baseUrlRest) {
    auto hostPort = this->extractHostFromUrl(baseUrlRest);
    this->hostRest = hostPort.first;
    this->portRest = hostPort.second;
  }
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
  void sendRequest(const std::string& host, const std::string& port, const http::request<http::string_body>& req,
                   std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                   long timeoutMilliSeconds) {
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
#endif
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > streamPtr(nullptr);
    try {
      streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, host);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(host, port, streamPtr));
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    this->resolver.async_resolve(
        host, port,
        beast::bind_front_handler(&Service::onResolve, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliSeconds));
  }
  void onResolve(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                 std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                 long timeoutMilliSeconds, beast::error_code ec, tcp::resolver::results_type tcpResolverResults) {
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(timeoutMilliSeconds));
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
        tcpResolverResults, beast::bind_front_handler(&Service::onConnect, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                 std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                 beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("connected");
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&Service::onHandshake, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                   std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                   beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    std::shared_ptr<http::request<http::string_body> > reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr,
                      beast::bind_front_handler(&Service::onWrite, shared_from_this(), httpConnectionPtr, reqPtr, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body> > reqPtr,
               std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
               beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_write callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("written");
    std::shared_ptr<beast::flat_buffer> bufferPtr(new beast::flat_buffer());
    std::shared_ptr<http::response<http::string_body> > resPtr(new http::response<http::string_body>());
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    http::async_read(
        stream, *bufferPtr, *resPtr,
        beast::bind_front_handler(&Service::onRead, shared_from_this(), httpConnectionPtr, reqPtr, errorHandler, responseHandler, bufferPtr, resPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body> > reqPtr,
              std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
              std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body> > resPtr, beast::error_code ec,
              std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_shutdown");
    stream.async_shutdown(beast::bind_front_handler(&Service::onShutdown, shared_from_this(), httpConnectionPtr, errorHandler));
    CCAPI_LOGGER_TRACE("after async_shutdown");
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << *resPtr;
    CCAPI_LOGGER_DEBUG("res = \n" + oss.str());
#endif
    responseHandler(*resPtr);
  }
  void onShutdown(std::shared_ptr<HttpConnection> httpConnectionPtr, std::function<void(const beast::error_code&)> errorHandler, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_shutdown callback start");
    if (ec == net::error::eof) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
    }
    if (ec) {
      CCAPI_LOGGER_TRACE("shutdown fail: " + ec.message() + ", category: " + ec.category().name());
      return;
    }
    CCAPI_LOGGER_TRACE("shutdown");
    // If we get here then the connection is closed gracefully
  }
  std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr,
                                                                      const std::string& host) {
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > streamPtr(new beast::ssl_stream<beast::tcp_stream>(*iocPtr, *ctxPtr));
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
      beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }
    return streamPtr;
  }
  std::string convertInstrumentToRestSymbolId(std::string instrument) {
    std::string symbolId = instrument;
    CCAPI_LOGGER_DEBUG("instrument = "+instrument);
    if (!instrument.empty()) {
      if (this->sessionConfigs.getExchangeInstrumentSymbolMapRest().find(this->exchangeName) != this->sessionConfigs.getExchangeInstrumentSymbolMapRest().end() &&
          this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at(this->exchangeName).find(instrument) !=
              this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at(this->exchangeName).end()) {
        symbolId = this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at(this->exchangeName).at(instrument);
      } else if (this->sessionConfigs.getExchangeInstrumentSymbolMap().find(this->exchangeName) != this->sessionConfigs.getExchangeInstrumentSymbolMap().end() &&
                 this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->exchangeName).find(instrument) !=
                     this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->exchangeName).end()) {
        symbolId = this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->exchangeName).at(instrument);
      }
    }
    return symbolId;
  }
  std::string convertRestSymbolIdToInstrument(std::string symbolId) {
    std::string instrument = symbolId;
    if (!symbolId.empty()) {
      if (this->sessionConfigs.getExchangeSymbolInstrumentMapRest().find(this->exchangeName) != this->sessionConfigs.getExchangeSymbolInstrumentMapRest().end() &&
          this->sessionConfigs.getExchangeSymbolInstrumentMapRest().at(this->exchangeName).find(symbolId) !=
              this->sessionConfigs.getExchangeSymbolInstrumentMapRest().at(this->exchangeName).end()) {
        instrument = this->sessionConfigs.getExchangeSymbolInstrumentMapRest().at(this->exchangeName).at(symbolId);
      } else if (this->sessionConfigs.getExchangeSymbolInstrumentMap().find(this->exchangeName) != this->sessionConfigs.getExchangeSymbolInstrumentMap().end() &&
                 this->sessionConfigs.getExchangeSymbolInstrumentMap().at(this->exchangeName).find(symbolId) !=
                     this->sessionConfigs.getExchangeSymbolInstrumentMap().at(this->exchangeName).end()) {
        instrument = this->sessionConfigs.getExchangeSymbolInstrumentMap().at(this->exchangeName).at(symbolId);
      }
    }
    return instrument;
  }
  void performRequest(std::shared_ptr<HttpConnection> httpConnectionPtr, const Request& request, http::request<http::string_body>& req,
                      const HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    try {
      std::call_once(this->tcpResolverResultsFlag,
                     [that = shared_from_this()]() { that->tcpResolverResults = that->resolver.resolve(that->hostRest, that->portRest); });
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_DEBUG("this->sessionOptions.httpRequestTimeoutMilliSeconds = " + toString(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(this->tcpResolverResults,
                                                  beast::bind_front_handler(&Service::onConnect_2, shared_from_this(), httpConnectionPtr, request, req, retry));
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onConnect_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                   beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "connect", {request.getCorrelationId()});
      return;
    }
    CCAPI_LOGGER_TRACE("connected");
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&Service::onHandshake_2, shared_from_this(), httpConnectionPtr, request, req, retry));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                     beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "handshake", {request.getCorrelationId()});
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    std::shared_ptr<http::request<http::string_body> > reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr, beast::bind_front_handler(&Service::onWrite_2, shared_from_this(), httpConnectionPtr, request, reqPtr, retry));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body> > reqPtr, HttpRetry retry,
                 beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_write callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "write", {request.getCorrelationId()});
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry);
      return;
    }
    CCAPI_LOGGER_TRACE("written");
    std::shared_ptr<beast::flat_buffer> bufferPtr(new beast::flat_buffer());
    std::shared_ptr<http::response<http::string_body> > resPtr(new http::response<http::string_body>());
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    http::async_read(stream, *bufferPtr, *resPtr,
                     beast::bind_front_handler(&Service::onRead_2, shared_from_this(), httpConnectionPtr, request, reqPtr, retry, bufferPtr, resPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body> > reqPtr, HttpRetry retry,
                std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body> > resPtr, beast::error_code ec,
                std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    auto now = UtilTime::now();
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "read", {request.getCorrelationId()});
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry);
      return;
    }
    if (this->sessionOptions.enableOneHttpConnectionPerRequest) {
      beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
      CCAPI_LOGGER_TRACE("before async_shutdown");
      stream.async_shutdown(beast::bind_front_handler(&Service::onShutdown_2, shared_from_this(), httpConnectionPtr));
      CCAPI_LOGGER_TRACE("after async_shutdown");
    } else {
      try {
        this->httpConnectionPool.pushBack(std::move(httpConnectionPtr));
      } catch (const std::runtime_error& e) {
        if (e.what() != this->httpConnectionPool.EXCEPTION_QUEUE_FULL) {
          CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
        }
      }
    }
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << *resPtr;
    CCAPI_LOGGER_DEBUG("res = \n" + oss.str());
#endif
    int statusCode = resPtr->result_int();
    std::string body = resPtr->body();
    try {
      if (statusCode / 100 == 2) {
        if (this->doesHttpBodyContainError(request, body)) {
          this->onResponseError(request, 400, body);
        } else {
          this->processSuccessfulTextMessage(request, body, now);
        }
      } else if (statusCode / 100 == 3) {
        if (resPtr->base().find("Location") != resPtr->base().end()) {
          Url url(resPtr->base().at("Location").to_string());
          std::string host(url.host);
          if (!url.port.empty()) {
            host += ":";
            host += url.port;
          }
          auto now = UtilTime::now();
          auto req = this->convertRequest(request, now);
          req.set(http::field::host, host);
          req.target(url.target);
          retry.numRedirect += 1;
          this->tryRequest(request, req, retry);
          return;
        } else {
          this->onResponseError(request, statusCode, body);
        }
      } else if (statusCode / 100 == 4) {
        this->onResponseError(request, statusCode, body);
      } else if (statusCode / 100 == 5) {
        this->onResponseError(request, statusCode, body);
        retry.numRetry += 1;
        this->tryRequest(request, *reqPtr, retry);
        return;
      } else {
        this->onResponseError(request, statusCode, "unhandled response");
      }
    } catch (const std::exception& e) {
      CCAPI_LOGGER_ERROR(e.what());
      std::ostringstream oss;
      oss << *resPtr;
      CCAPI_LOGGER_ERROR("res = " + oss.str());
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, e, {request.getCorrelationId()});
    }
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    if (retry.promisePtr) {
      retry.promisePtr->set_value();
    }
  }
  virtual bool doesHttpBodyContainError(const Request& request, const std::string& body) { return false; }
  void onShutdown_2(std::shared_ptr<HttpConnection> httpConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_shutdown callback start");
    if (ec == net::error::eof) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
    }
    if (ec) {
      CCAPI_LOGGER_TRACE("shutdown fail: " + ec.message() + ", category: " + ec.category().name());
      // this->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
      return;
    }
    CCAPI_LOGGER_TRACE("shutdown");
    // If we get here then the connection is closed gracefully
  }
  void tryRequest(const Request& request, http::request<http::string_body>& req, const HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
#endif
    CCAPI_LOGGER_TRACE("retry = " + toString(retry));
    if (retry.numRetry <= this->sessionOptions.httpMaxNumRetry && retry.numRedirect <= this->sessionOptions.httpMaxNumRedirect) {
      try {
        if (this->sessionOptions.enableOneHttpConnectionPerRequest || this->httpConnectionPool.empty()) {
          std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > streamPtr(nullptr);
          try {
            streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->hostRest);
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream", {request.getCorrelationId()});
            return;
          }
          std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(this->hostRest, this->portRest, streamPtr));
          CCAPI_LOGGER_TRACE("about to perform request with new http connection");
          this->performRequest(httpConnectionPtr, request, req, retry);
        } else {
          std::shared_ptr<HttpConnection> httpConnectionPtr(nullptr);
          try {
            httpConnectionPtr = std::move(this->httpConnectionPool.popBack());
            this->onHandshake_2(httpConnectionPtr, request, req, retry, {});
          } catch (const std::runtime_error& e) {
            if (e.what() != this->httpConnectionPool.EXCEPTION_QUEUE_EMPTY) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
            std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > streamPtr(nullptr);
            try {
              streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->hostRest);
            } catch (const beast::error_code& ec) {
              CCAPI_LOGGER_TRACE("fail");
              this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream", {request.getCorrelationId()});
              return;
            }
            httpConnectionPtr = std::make_shared<HttpConnection>(this->hostRest, this->portRest, streamPtr);
            CCAPI_LOGGER_TRACE("about to perform request with existing http connection");
            this->performRequest(httpConnectionPtr, request, req, retry);
          }
        }
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, e, {request.getCorrelationId()});
      }
    } else {
      std::string errorMessage = this->sessionOptions.httpMaxNumRetry ? "max retry exceeded" : "max redirect exceeded";
      CCAPI_LOGGER_ERROR(errorMessage);
      CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, std::runtime_error(errorMessage), {request.getCorrelationId()});
      if (retry.promisePtr) {
        retry.promisePtr->set_value();
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  http::request<http::string_body> convertRequest(const Request& request, const TimePoint& now) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto credential = request.getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto instrument = request.getInstrument();
    auto symbolId = this->convertInstrumentToRestSymbolId(instrument);
    CCAPI_LOGGER_TRACE("symbolId = " + symbolId);
    auto operation = request.getOperation();
    http::request<http::string_body> req;
    if (this->exchangeName == CCAPI_EXCHANGE_NAME_OKEX) {
      req.set(http::field::host, this->hostRest);
    } else {
      req.set(http::field::host, this->hostRest + ":" + this->portRest);
    }
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    this->convertReq(req, request, operation, now, symbolId, credential);
    CCAPI_LOGGER_FUNCTION_EXIT;
    return req;
  }
  std::string exchangeName;
  std::string baseUrl;
  std::string baseUrlRest;
  std::function<void(Event& event)> eventHandler;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  ServiceContextPtr serviceContextPtr;
  tcp::resolver resolver;
  std::string hostRest;
  std::string portRest;
  tcp::resolver::results_type tcpResolverResults;
  std::once_flag tcpResolverResultsFlag;
  Queue<std::shared_ptr<HttpConnection> > httpConnectionPool;
  std::map<std::string, std::string> credentialDefault;
  std::map<std::string, TimerPtr> sendRequestDelayTimerByCorrelationIdMap;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
