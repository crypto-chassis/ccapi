#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#include "ccapi_cpp/ccapi_logger.h"
#ifndef RAPIDJSON_HAS_CXX11_NOEXCEPT
#define RAPIDJSON_HAS_CXX11_NOEXCEPT 0
#endif
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x)                                           \
  if (!(x)) {                                                         \
    throw std::runtime_error("rapidjson internal assertion failure"); \
  }
#endif
#ifndef RAPIDJSON_PARSE_ERROR_NORETURN
#define RAPIDJSON_PARSE_ERROR_NORETURN(parseErrorCode, offset) throw std::runtime_error(#parseErrorCode)
#endif
#include <regex>

#include "boost/asio/strand.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/version.hpp"
#include "ccapi_cpp/ccapi_decimal.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_market_data_message.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
// clang-format off
#include "websocketpp/config/boost_config.hpp"
#include "websocketpp/client.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/config/asio_client.hpp"
// clang-format on
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) && (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || \
                                                  defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) && (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP))
#include <iomanip>
#include <sstream>

#include "ccapi_cpp/websocketpp_decompress_workaround.h"
#endif
#include "ccapi_cpp/ccapi_fix_connection.h"
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
/**
 * Defines a service which provides access to exchange API and normalizes them. This is a base class that implements generic functionalities for dealing with
 * exchange REST and Websocket APIs. The Session object is responsible for routing requests and subscriptions to the desired concrete service.
 */
class Service : public std::enable_shared_from_this<Service> {
 public:
  typedef wspp::lib::shared_ptr<ServiceContext> ServiceContextPtr;
  typedef wspp::lib::error_code ErrorCode;
  enum class PingPongMethod {
    WEBSOCKET_PROTOCOL_LEVEL,
    WEBSOCKET_APPLICATION_LEVEL,
    FIX_PROTOCOL_LEVEL,
  };
  static std::string pingPongMethodToString(PingPongMethod pingPongMethod) {
    std::string output;
    switch (pingPongMethod) {
      case PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL:
        output = "WEBSOCKET_PROTOCOL_LEVEL";
        break;
      case PingPongMethod::WEBSOCKET_APPLICATION_LEVEL:
        output = "WEBSOCKET_APPLICATION_LEVEL";
        break;
      case PingPongMethod::FIX_PROTOCOL_LEVEL:
        output = "FIX_PROTOCOL_LEVEL";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  Service(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
          ServiceContextPtr serviceContextPtr)
      : eventHandler(eventHandler),
        sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs),
        serviceContextPtr(serviceContextPtr),
        resolver(*serviceContextPtr->ioContextPtr),
        httpConnectionPool(sessionOptions.httpConnectionPoolMaxSize) {
    this->enableCheckPingPongWebsocketProtocolLevel = this->sessionOptions.enableCheckPingPongWebsocketProtocolLevel;
    this->enableCheckPingPongWebsocketApplicationLevel = this->sessionOptions.enableCheckPingPongWebsocketApplicationLevel;
    this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = sessionOptions.pingWebsocketProtocolLevelIntervalMilliSeconds;
    this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = sessionOptions.pongWebsocketProtocolLevelTimeoutMilliSeconds;
    this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] = sessionOptions.pingWebsocketApplicationLevelIntervalMilliSeconds;
    this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] = sessionOptions.pongWebsocketApplicationLevelTimeoutMilliSeconds;
    this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::FIX_PROTOCOL_LEVEL] = sessionOptions.heartbeatFixIntervalMilliSeconds;
    this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::FIX_PROTOCOL_LEVEL] = sessionOptions.heartbeatFixTimeoutMilliSeconds;
  }
  virtual ~Service() {
    for (const auto& x : this->pingTimerByMethodByConnectionIdMap) {
      for (const auto& y : x.second) {
        y.second->cancel();
      }
    }
    for (const auto& x : this->pongTimeOutTimerByMethodByConnectionIdMap) {
      for (const auto& y : x.second) {
        y.second->cancel();
      }
    }
    for (const auto& x : this->connectRetryOnFailTimerByConnectionIdMap) {
      x.second->cancel();
    }
    if (this->httpConnectionPoolPurgeTimer) {
      this->httpConnectionPoolPurgeTimer->cancel();
    }
  }
  // void setEventHandler(const std::function<void(Event& event)>& eventHandler) { this->eventHandler = eventHandler; }
  void stop() {
    for (const auto& x : this->sendRequestDelayTimerByCorrelationIdMap) {
      x.second->cancel();
    }
    sendRequestDelayTimerByCorrelationIdMap.clear();
    this->shouldContinue = false;
    for (const auto& x : this->wsConnectionByIdMap) {
      auto wsConnection = x.second;
      ErrorCode ec;
      this->close(wsConnection, wsConnection.hdl, websocketpp::close::status::normal, "stop", ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
      }
      this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
    }
  }
  virtual void convertRequestForRestCustom(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                                           const std::map<std::string, std::string>& credential) {
    auto errorMessage = "REST unimplemented operation " + Request::operationToString(request.getOperation()) + " for exchange " + request.getExchange();
    throw std::runtime_error(errorMessage);
  }
  virtual void subscribe(std::vector<Subscription>& subscriptionList) {}
  virtual void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                                     const std::map<std::string, std::string>& credential) {}
  virtual void processSuccessfulTextMessageRest(int statusCode, const Request& request, const std::string& textMessage, const TimePoint& timeReceived,
                                                Queue<Event>* eventQueuePtr) {}
  std::shared_ptr<std::future<void>> sendRequest(Request& request, const bool useFuture, const TimePoint& now, long delayMilliSeconds,
                                                 Queue<Event>* eventQueuePtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("request = " + toString(request));
    CCAPI_LOGGER_DEBUG("useFuture = " + toString(useFuture));
    TimePoint then;
    if (delayMilliSeconds > 0) {
      then = now + std::chrono::milliseconds(delayMilliSeconds);
    } else {
      then = now;
    }
    http::request<http::string_body> req;
    try {
      req = this->convertRequest(request, then);
    } catch (const std::runtime_error& e) {
      CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, e, {request.getCorrelationId()}, eventQueuePtr);
      std::promise<void>* promisePtrRaw = nullptr;
      if (useFuture) {
        promisePtrRaw = new std::promise<void>();
      }
      std::shared_ptr<std::promise<void>> promisePtr(promisePtrRaw);
      std::shared_ptr<std::future<void>> futurePtr(nullptr);
      if (useFuture) {
        futurePtr = std::make_shared<std::future<void>>(std::move(promisePtr->get_future()));
        promisePtr->set_value();
      }
      return futurePtr;
    }
    std::promise<void>* promisePtrRaw = nullptr;
    if (useFuture) {
      promisePtrRaw = new std::promise<void>();
    }
    std::shared_ptr<std::promise<void>> promisePtr(promisePtrRaw);
    HttpRetry retry(0, 0, "", promisePtr);
    if (delayMilliSeconds > 0) {
      this->sendRequestDelayTimerByCorrelationIdMap[request.getCorrelationId()] = this->serviceContextPtr->tlsClientPtr->set_timer(
          delayMilliSeconds, [that = shared_from_this(), request, req, retry, eventQueuePtr](ErrorCode const& ec) mutable {
            if (ec) {
              CCAPI_LOGGER_ERROR("request = " + toString(request) + ", sendRequest timer error: " + ec.message());
              that->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, ec, "timer", {request.getCorrelationId()}, eventQueuePtr);
            } else {
              auto thatReq = req;
              auto now = UtilTime::now();
              request.setTimeSent(now);
              that->tryRequest(request, thatReq, retry, eventQueuePtr);
            }
            that->sendRequestDelayTimerByCorrelationIdMap.erase(request.getCorrelationId());
          });
    } else {
      request.setTimeSent(now);
      this->tryRequest(request, req, retry, eventQueuePtr);
    }
    std::shared_ptr<std::future<void>> futurePtr(nullptr);
    if (useFuture) {
      futurePtr = std::make_shared<std::future<void>>(std::move(promisePtr->get_future()));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return futurePtr;
  }
  virtual void sendRequestByWebsocket(Request& request, const TimePoint& now) {}
  virtual void sendRequestByFix(Request& request, const TimePoint& now) {}
  virtual void subscribeByFix(Subscription& subscription) {}
  void onError(const Event::Type eventType, const Message::Type messageType, const std::string& errorMessage,
               const std::vector<std::string> correlationIdList = {}, Queue<Event>* eventQueuePtr = nullptr) {
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    CCAPI_LOGGER_ERROR("correlationIdList = " + toString(correlationIdList));
    Event event;
    event.setType(eventType);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setType(messageType);
    message.setCorrelationIdList(correlationIdList);
    Element element;
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event, eventQueuePtr);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const ErrorCode& ec, const std::string& what,
               const std::vector<std::string> correlationIdList = {}, Queue<Event>* eventQueuePtr = nullptr) {
    this->onError(eventType, messageType, what + ": " + ec.message() + ", category: " + ec.category().name(), correlationIdList, eventQueuePtr);
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::exception& e, const std::vector<std::string> correlationIdList = {},
               Queue<Event>* eventQueuePtr = nullptr) {
    this->onError(eventType, messageType, e.what(), correlationIdList, eventQueuePtr);
  }
  void onResponseError(const Request& request, int statusCode, const std::string& errorMessage, Queue<Event>* eventQueuePtr) {
    std::string statusCodeStr = std::to_string(statusCode);
    CCAPI_LOGGER_ERROR("request = " + toString(request) + ", statusCode = " + statusCodeStr + ", errorMessage = " + errorMessage);
    Event event;
    event.setType(Event::Type::RESPONSE);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setType(Message::Type::RESPONSE_ERROR);
    message.setCorrelationIdList({request.getCorrelationId()});
    Element element;
    element.insert(CCAPI_HTTP_STATUS_CODE, statusCodeStr);
    element.insert(CCAPI_ERROR_MESSAGE, UtilString::trim(errorMessage));
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event, eventQueuePtr);
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  typedef ServiceContext::SslContextPtr SslContextPtr;
  typedef ServiceContext::TlsClient TlsClient;
  typedef wspp::lib::shared_ptr<wspp::lib::asio::steady_timer> TimerPtr;
  void setHostRestFromUrlRest(std::string baseUrlRest) {
    auto hostPort = this->extractHostFromUrl(baseUrlRest);
    this->hostRest = hostPort.first;
    this->portRest = hostPort.second;
  }
  std::pair<std::string, std::string> extractHostFromUrl(std::string baseUrl) {
    std::string host;
    std::string port;
    auto splitted1 = UtilString::split(baseUrl, "://");
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
  void sendRequest(const http::request<http::string_body>& req, std::function<void(const beast::error_code&)> errorHandler,
                   std::function<void(const http::response<http::string_body>&)> responseHandler, long timeoutMilliSeconds) {
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
#endif
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(nullptr);
    try {
      streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->hostRest);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(this->hostRest, this->portRest, streamPtr));
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    this->startConnect(httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliSeconds, this->tcpResolverResultsRest);
  }
  void sendRequest(const std::string& host, const std::string& port, const http::request<http::string_body>& req,
                   std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                   long timeoutMilliSeconds) {
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
#endif
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(nullptr);
    try {
      streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, host);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(host, port, streamPtr));
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    tcp::resolver newResolver(*this->serviceContextPtr->ioContextPtr);
    newResolver.async_resolve(
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
    this->startConnect(httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliSeconds, tcpResolverResultsRest);
  }
  void startConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                    std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                    long timeoutMilliSeconds, tcp::resolver::results_type tcpResolverResults) {
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    if (timeoutMilliSeconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(timeoutMilliSeconds));
    }
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
        tcpResolverResults, beast::bind_front_handler(&Service::onConnect, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_connect");
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
    // #ifdef CCAPI_DISABLE_NAGLE_ALGORITHM
    beast::get_lowest_layer(stream).socket().set_option(tcp::no_delay(true));
    // #endif
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
    std::shared_ptr<http::request<http::string_body>> reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr,
                      beast::bind_front_handler(&Service::onWrite, shared_from_this(), httpConnectionPtr, reqPtr, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body>> reqPtr,
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
    std::shared_ptr<http::response<http::string_body>> resPtr(new http::response<http::string_body>());
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    http::async_read(
        stream, *bufferPtr, *resPtr,
        beast::bind_front_handler(&Service::onRead, shared_from_this(), httpConnectionPtr, reqPtr, errorHandler, responseHandler, bufferPtr, resPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body>> reqPtr,
              std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
              std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body>> resPtr, beast::error_code ec,
              std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    {
      std::ostringstream oss;
      oss << *reqPtr;
      CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
    }
    {
      std::ostringstream oss;
      oss << *resPtr;
      CCAPI_LOGGER_DEBUG("res = \n" + oss.str());
    }
#endif
    responseHandler(*resPtr);
  }
  std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr,
                                                                     const std::string& host) {
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(new beast::ssl_stream<beast::tcp_stream>(*iocPtr, *ctxPtr));
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
      beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }
    return streamPtr;
  }
  void performRequest(std::shared_ptr<HttpConnection> httpConnectionPtr, const Request& request, http::request<http::string_body>& req, const HttpRetry& retry,
                      Queue<Event>* eventQueuePtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_DEBUG("this->sessionOptions.httpRequestTimeoutMilliSeconds = " + toString(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    if (this->sessionOptions.httpRequestTimeoutMilliSeconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    }
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
        this->tcpResolverResultsRest,
        beast::bind_front_handler(&Service::onConnect_2, shared_from_this(), httpConnectionPtr, request, req, retry, eventQueuePtr));
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onConnect_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                   Queue<Event>* eventQueuePtr, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "connect", {request.getCorrelationId()}, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("connected");
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    // #ifdef CCAPI_DISABLE_NAGLE_ALGORITHM
    beast::get_lowest_layer(stream).socket().set_option(tcp::no_delay(true));
    // #endif
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&Service::onHandshake_2, shared_from_this(), httpConnectionPtr, request, req, retry, eventQueuePtr));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                     Queue<Event>* eventQueuePtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "handshake", {request.getCorrelationId()}, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    this->startWrite_2(httpConnectionPtr, request, req, retry, eventQueuePtr);
  }
  void startWrite_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                    Queue<Event>* eventQueuePtr) {
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    if (this->sessionOptions.httpRequestTimeoutMilliSeconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    }
    std::shared_ptr<http::request<http::string_body>> reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr,
                      beast::bind_front_handler(&Service::onWrite_2, shared_from_this(), httpConnectionPtr, request, reqPtr, retry, eventQueuePtr));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body>> reqPtr, HttpRetry retry,
                 Queue<Event>* eventQueuePtr, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_write callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "write", {request.getCorrelationId()}, eventQueuePtr);
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("written");
    std::shared_ptr<beast::flat_buffer> bufferPtr(new beast::flat_buffer());
    std::shared_ptr<http::response<http::string_body>> resPtr(new http::response<http::string_body>());
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    http::async_read(
        stream, *bufferPtr, *resPtr,
        beast::bind_front_handler(&Service::onRead_2, shared_from_this(), httpConnectionPtr, request, reqPtr, retry, bufferPtr, resPtr, eventQueuePtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void setHttpConnectionPoolPurgeTimer() {
    this->httpConnectionPoolPurgeTimer = this->serviceContextPtr->tlsClientPtr->set_timer(5000, [that = shared_from_this()](ErrorCode const& ec) {
      auto now = UtilTime::now();
      if (ec) {
        that->onError(Event::Type::SESSION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
      } else {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - that->lastHttpConnectionPoolPushBackTp).count() >
            that->sessionOptions.httpConnectionPoolIdleTimeoutMilliSeconds) {
          that->httpConnectionPool.purge();
        }
        that->setHttpConnectionPoolPurgeTimer();
      }
    });
  }
  void onRead_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body>> reqPtr, HttpRetry retry,
                std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body>> resPtr, Queue<Event>* eventQueuePtr,
                beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    auto now = UtilTime::now();
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "read", {request.getCorrelationId()}, eventQueuePtr);
      this->httpConnectionPool.purge();
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry, eventQueuePtr);
      return;
    }
    if (!this->sessionOptions.enableOneHttpConnectionPerRequest) {
      try {
        if (std::chrono::duration_cast<std::chrono::seconds>(this->lastHttpConnectionPoolPushBackTp.time_since_epoch()).count() == 0 &&
            this->sessionOptions.httpConnectionPoolIdleTimeoutMilliSeconds > 0) {
          this->setHttpConnectionPoolPurgeTimer();
        }
        this->httpConnectionPool.pushBack(std::move(httpConnectionPtr));
        this->lastHttpConnectionPoolPushBackTp = now;
        CCAPI_LOGGER_TRACE("pushed back httpConnectionPtr " + toString(*httpConnectionPtr) + " to pool");
      } catch (const std::runtime_error& e) {
        if (e.what() != this->httpConnectionPool.EXCEPTION_QUEUE_FULL) {
          CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
        }
      }
    }
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    {
      std::ostringstream oss;
      oss << *reqPtr;
      CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
    }
    {
      std::ostringstream oss;
      oss << *resPtr;
      CCAPI_LOGGER_DEBUG("res = \n" + oss.str());
    }
#endif
    int statusCode = resPtr->result_int();
    std::string body = resPtr->body();
    try {
      if (statusCode / 100 == 2) {
        this->processSuccessfulTextMessageRest(statusCode, request, body, now, eventQueuePtr);
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
          CCAPI_LOGGER_WARN("redirect from request " + request.toString() + " to url " + url.toString());
          this->tryRequest(request, req, retry, eventQueuePtr);
        }
        this->onResponseError(request, statusCode, body, eventQueuePtr);
        return;
      } else if (statusCode / 100 == 4) {
        this->onResponseError(request, statusCode, body, eventQueuePtr);
      } else if (statusCode / 100 == 5) {
        this->onResponseError(request, statusCode, body, eventQueuePtr);
        retry.numRetry += 1;
        this->tryRequest(request, *reqPtr, retry, eventQueuePtr);
        return;
      } else {
        this->onResponseError(request, statusCode, "unhandled response", eventQueuePtr);
      }
    } catch (const std::exception& e) {
      CCAPI_LOGGER_ERROR(e.what());
      {
        std::ostringstream oss;
        oss << *reqPtr;
        CCAPI_LOGGER_ERROR("req = \n" + oss.str());
      }
      {
        std::ostringstream oss;
        oss << *resPtr;
        CCAPI_LOGGER_ERROR("res = " + oss.str());
      }
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, e, {request.getCorrelationId()}, eventQueuePtr);
    }
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    if (retry.promisePtr) {
      retry.promisePtr->set_value();
    }
  }
  virtual bool doesHttpBodyContainError(const Request& request, const std::string& body) { return false; }
  void tryRequest(const Request& request, http::request<http::string_body>& req, const HttpRetry& retry, Queue<Event>* eventQueuePtr) {
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
          std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(nullptr);
          try {
            streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->hostRest);
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream", {request.getCorrelationId()}, eventQueuePtr);
            return;
          }
          std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(this->hostRest, this->portRest, streamPtr));
          CCAPI_LOGGER_WARN("about to perform request with new httpConnectionPtr " + toString(*httpConnectionPtr));
          this->performRequest(httpConnectionPtr, request, req, retry, eventQueuePtr);
        } else {
          std::shared_ptr<HttpConnection> httpConnectionPtr(nullptr);
          try {
            httpConnectionPtr = std::move(this->httpConnectionPool.popBack());
            CCAPI_LOGGER_TRACE("about to perform request with existing httpConnectionPtr " + toString(*httpConnectionPtr));
            this->startWrite_2(httpConnectionPtr, request, req, retry, eventQueuePtr);
          } catch (const std::runtime_error& e) {
            if (e.what() != this->httpConnectionPool.EXCEPTION_QUEUE_EMPTY) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
            std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(nullptr);
            try {
              streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->hostRest);
            } catch (const beast::error_code& ec) {
              CCAPI_LOGGER_TRACE("fail");
              this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream", {request.getCorrelationId()}, eventQueuePtr);
              return;
            }
            httpConnectionPtr = std::make_shared<HttpConnection>(this->hostRest, this->portRest, streamPtr);
            CCAPI_LOGGER_WARN("about to perform request with new httpConnectionPtr " + toString(*httpConnectionPtr));
            this->performRequest(httpConnectionPtr, request, req, retry, eventQueuePtr);
          }
        }
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, e, {request.getCorrelationId()}, eventQueuePtr);
      }
    } else {
      std::string errorMessage = retry.numRetry > this->sessionOptions.httpMaxNumRetry ? "max retry exceeded" : "max redirect exceeded";
      CCAPI_LOGGER_ERROR(errorMessage);
      CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, std::runtime_error(errorMessage), {request.getCorrelationId()}, eventQueuePtr);
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
    auto symbolId = instrument;
    CCAPI_LOGGER_TRACE("symbolId = " + symbolId);
    http::request<http::string_body> req;
    req.version(11);
    if (this->sessionOptions.enableOneHttpConnectionPerRequest) {
      req.keep_alive(false);
    } else {
      req.keep_alive(true);
    }
    req.set(http::field::host, this->hostRest);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    this->convertRequestForRest(req, request, now, symbolId, credential);
    CCAPI_LOGGER_FUNCTION_EXIT;
    return req;
  }
  SslContextPtr onTlsInit(wspp::connection_hdl hdl) { return this->serviceContextPtr->sslContextPtr; }
  WsConnection& getWsConnectionFromConnectionPtr(TlsClient::connection_ptr connectionPtr) {
    return this->wsConnectionByIdMap.at(this->connectionAddressToString(connectionPtr));
  }
  std::string connectionAddressToString(const TlsClient::connection_ptr con) {
    const void* address = static_cast<const void*>(con.get());
    std::stringstream ss;
    ss << address;
    return ss.str();
  }
  void close(WsConnection& wsConnection, wspp::connection_hdl hdl, wspp::close::status::value const code, std::string const& reason, ErrorCode& ec) {
    if (wsConnection.status == WsConnection::Status::CLOSING) {
      CCAPI_LOGGER_WARN("websocket connection is already in the state of closing");
      return;
    }
    wsConnection.status = WsConnection::Status::CLOSING;
    this->serviceContextPtr->tlsClientPtr->close(hdl, code, reason, ec);
  }
  void substituteParam(std::string& target, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      auto it = target.find(key);
      if (it != std::string::npos) {
        target = target.replace(it, key.length(), value);
      }
    }
  }
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    int i = 0;
    for (const auto& kv : param) {
      std::string key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += key;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
      ++i;
    }
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId, const std::string symbolIdCalled) {
    if (!symbolId.empty()) {
      queryString += symbolIdCalled;
      queryString += "=";
      queryString += Url::urlEncode(symbolId);
      queryString += "&";
    }
  }
  // virtual std::string convertNumberToStringInJson(const std::string& jsonString) {
  //   auto quotedTextMessage = std::regex_replace(jsonString, this->convertNumberToStringInJsonRegex,
  //   this->convertNumberToStringInJsonRewrite); return quotedTextMessage;
  // }
  void setupCredential(std::vector<std::string> nameList) {
    for (const auto& x : nameList) {
      if (this->sessionConfigs.getCredential().find(x) != this->sessionConfigs.getCredential().end()) {
        this->credentialDefault.insert(std::make_pair(x, this->sessionConfigs.getCredential().at(x)));
      } else if (!UtilSystem::getEnvAsString(x).empty()) {
        this->credentialDefault.insert(std::make_pair(x, UtilSystem::getEnvAsString(x)));
      }
    }
  }
  virtual void prepareConnect(WsConnection& wsConnection) { this->connect(wsConnection); }
  virtual void connect(WsConnection& wsConnection) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    wsConnection.status = WsConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("connection initialization on dummy id " + wsConnection.id);
    std::string url = wsConnection.url;
    CCAPI_LOGGER_DEBUG("url = " + url);
    this->serviceContextPtr->tlsClientPtr->set_tls_init_handler(std::bind(&Service::onTlsInit, shared_from_this(), std::placeholders::_1));
    CCAPI_LOGGER_DEBUG("endpoint tls init handler set");
    ErrorCode ec;
    TlsClient::connection_ptr con = this->serviceContextPtr->tlsClientPtr->get_connection(url, ec);
    for (const auto& kv : wsConnection.headers) {
      con->append_header(kv.first, kv.second);
    }
    wsConnection.id = this->connectionAddressToString(con);
    CCAPI_LOGGER_DEBUG("connection initialization on actual id " + wsConnection.id);
    if (ec) {
      CCAPI_LOGGER_FATAL("connection initialization error: " + ec.message());
    }
    this->wsConnectionByIdMap.insert(std::pair<std::string, WsConnection>(wsConnection.id, wsConnection));
    CCAPI_LOGGER_DEBUG("this->wsConnectionByIdMap = " + toString(this->wsConnectionByIdMap));
    con->set_open_handler(std::bind(&Service::onOpen, shared_from_this(), std::placeholders::_1));
    con->set_fail_handler(std::bind(&Service::onFail, shared_from_this(), std::placeholders::_1));
    con->set_close_handler(std::bind(&Service::onClose, shared_from_this(), std::placeholders::_1));
    con->set_message_handler(std::bind(&Service::onMessage, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    if (this->sessionOptions.enableCheckPingPongWebsocketProtocolLevel) {
      con->set_pong_handler(std::bind(&Service::onPong, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    }
    con->set_ping_handler(std::bind(&Service::onPing, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    this->serviceContextPtr->tlsClientPtr->connect(con);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onOpen(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
    wsConnection.hdl = hdl;
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " established");
    auto urlBase = UtilString::split(wsConnection.url, "?").at(0);
    this->connectNumRetryOnFailByConnectionUrlMap[urlBase] = 0;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    std::vector<std::string> correlationIdList;
    for (const auto& subscription : wsConnection.subscriptionList) {
      correlationIdList.push_back(subscription.getCorrelationId());
    }
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnection.id);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    if (this->enableCheckPingPongWebsocketProtocolLevel) {
      this->setPingPongTimer(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnection, hdl,
                             [that = shared_from_this()](wspp::connection_hdl hdl, ErrorCode& ec) { that->ping(hdl, "", ec); });
    }
    if (this->enableCheckPingPongWebsocketApplicationLevel) {
      this->setPingPongTimer(PingPongMethod::WEBSOCKET_APPLICATION_LEVEL, wsConnection, hdl,
                             [that = shared_from_this()](wspp::connection_hdl hdl, ErrorCode& ec) { that->pingOnApplicationLevel(hdl, ec); });
    }
  }
  virtual void onFail_(WsConnection& wsConnection) {
    wsConnection.status = WsConnection::Status::FAILED;
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "connection " + toString(wsConnection) + " has failed before opening");
    WsConnection thisWsConnection = wsConnection;
    this->wsConnectionByIdMap.erase(thisWsConnection.id);
    auto urlBase = UtilString::split(thisWsConnection.url, "?").at(0);
    long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[urlBase], 6)));
    CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(thisWsConnection.id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(thisWsConnection.id)->cancel();
    }
    this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] =
        this->serviceContextPtr->tlsClientPtr->set_timer(seconds * 1000, [thisWsConnection, that = shared_from_this(), urlBase](ErrorCode const& ec) {
          if (that->wsConnectionByIdMap.find(thisWsConnection.id) == that->wsConnectionByIdMap.end()) {
            if (ec) {
              CCAPI_LOGGER_ERROR("wsConnection = " + toString(thisWsConnection) + ", connect retry on fail timer error: " + ec.message());
              that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
            } else {
              CCAPI_LOGGER_INFO("about to retry");
              auto thatWsConnection = thisWsConnection;
              thatWsConnection.assignDummyId();
              that->prepareConnect(thatWsConnection);
              that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
            }
          }
        });
  }
  virtual void onFail(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->clearStates(wsConnection);
    this->onFail_(wsConnection);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void clearStates(WsConnection& wsConnection) {
    CCAPI_LOGGER_INFO("clear states for wsConnection " + toString(wsConnection));
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap.erase(wsConnection.id);
    this->lastPongTpByMethodByConnectionIdMap.erase(wsConnection.id);
    this->extraPropertyByConnectionIdMap.erase(wsConnection.id);
    if (this->pingTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pingTimerByMethodByConnectionIdMap.end()) {
      for (const auto& x : this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id)) {
        x.second->cancel();
      }
      this->pingTimerByMethodByConnectionIdMap.erase(wsConnection.id);
    }
    if (this->pongTimeOutTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pongTimeOutTimerByMethodByConnectionIdMap.end()) {
      for (const auto& x : this->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id)) {
        x.second->cancel();
      }
      this->pongTimeOutTimerByMethodByConnectionIdMap.erase(wsConnection.id);
    }
    // auto urlBase = UtilString::split(wsConnection.url, "?").at(0);
    // this->connectNumRetryOnFailByConnectionUrlMap.erase(urlBase);
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(wsConnection.id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      this->connectRetryOnFailTimerByConnectionIdMap.erase(wsConnection.id);
    }
  }
  virtual void onClose(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    TlsClient::connection_ptr con = this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(con);
    wsConnection.status = WsConnection::Status::CLOSED;
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " is closed");
    std::stringstream s;
    s << "close code: " << con->get_remote_close_code() << " (" << websocketpp::close::status::get_string(con->get_remote_close_code())
      << "), close reason: " << con->get_remote_close_reason();
    std::string reason = s.str();
    CCAPI_LOGGER_INFO("reason is " + reason);
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_DOWN);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnection.id);
    element.insert(CCAPI_REASON, reason);
    message.setElementList({element});
    std::vector<std::string> correlationIdList;
    for (const auto& subscription : wsConnection.subscriptionList) {
      correlationIdList.push_back(subscription.getCorrelationId());
    }
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " is closed");
    this->clearStates(wsConnection);
    WsConnection thisWsConnection = wsConnection;
    this->wsConnectionByIdMap.erase(wsConnection.id);
    if (this->shouldContinue.load()) {
      thisWsConnection.assignDummyId();
      this->prepareConnect(thisWsConnection);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onMessage(wspp::connection_hdl hdl, TlsClient::message_ptr msg) {
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_DEBUG("received a message from connection " + toString(wsConnection));
    if (wsConnection.status != WsConnection::Status::OPEN && !this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id]) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }
    auto opcode = msg->get_opcode();
    // CCAPI_LOGGER_DEBUG("opcode = " + toString(opcode));
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
      const std::string& textMessage = msg->get_payload();
      CCAPI_LOGGER_DEBUG("received a text message: " + textMessage);
      try {
        this->onTextMessage(hdl, textMessage, now);
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR("textMessage = " + textMessage);
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, e);
      }
    } else if (opcode == websocketpp::frame::opcode::binary) {
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) && (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || \
                                                  defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) && (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP))
      if (this->needDecompressWebsocketMessage) {
        std::string decompressed;
        const std::string& payload = msg->get_payload();
        try {
          ErrorCode ec = this->inflater.decompress(reinterpret_cast<const uint8_t*>(&payload[0]), payload.size(), decompressed);
          if (ec) {
            CCAPI_LOGGER_FATAL(ec.message());
          }
          CCAPI_LOGGER_DEBUG("decompressed = " + decompressed);
          this->onTextMessage(hdl, decompressed, now);
        } catch (const std::exception& e) {
          std::stringstream ss;
          ss << std::hex << std::setfill('0');
          for (int i = 0; i < payload.size(); ++i) {
            ss << std::setw(2) << static_cast<unsigned>(reinterpret_cast<const uint8_t*>(&payload[0])[i]);
          }
          CCAPI_LOGGER_ERROR("binaryMessage = " + ss.str());
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, e);
        }
        ErrorCode ec = this->inflater.inflate_reset();
        if (ec) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "decompress");
        }
      }
#endif
    }
  }
  void onPong(wspp::connection_hdl hdl, std::string payload) {
    auto now = UtilTime::now();
    this->onPongByMethod(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, hdl, payload, now);
  }
  void onPongByMethod(PingPongMethod method, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_TRACE(pingPongMethodToString(method) + ": received a pong from " + toString(wsConnection));
    this->lastPongTpByMethodByConnectionIdMap[wsConnection.id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  bool onPing(wspp::connection_hdl hdl, std::string payload) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_TRACE("received a ping from " + toString(wsConnection));
    CCAPI_LOGGER_FUNCTION_EXIT;
    return true;
  }
  void send(wspp::connection_hdl hdl, std::string const& payload, wspp::frame::opcode::value op, ErrorCode& ec) {
    this->serviceContextPtr->tlsClientPtr->send(hdl, payload, op, ec);
  }
  void ping(wspp::connection_hdl hdl, std::string const& payload, ErrorCode& ec) { this->serviceContextPtr->tlsClientPtr->ping(hdl, payload, ec); }
  virtual void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) {}
  void setPingPongTimer(PingPongMethod method, WsConnection& wsConnection, wspp::connection_hdl hdl,
                        std::function<void(wspp::connection_hdl, ErrorCode&)> pingMethod) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliSeconds = this->pingIntervalMilliSecondsByMethodMap[method];
    auto pongTimeoutMilliSeconds = this->pongTimeoutMilliSecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliSeconds = " + toString(pingIntervalMilliSeconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliSeconds = " + toString(pongTimeoutMilliSeconds));
    if (pingIntervalMilliSeconds <= pongTimeoutMilliSeconds) {
      return;
    }
    if (wsConnection.status == WsConnection::Status::OPEN) {
      if (this->pingTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).find(method) != this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).at(method)->cancel();
      }
      this->pingTimerByMethodByConnectionIdMap[wsConnection.id][method] = this->serviceContextPtr->tlsClientPtr->set_timer(
          pingIntervalMilliSeconds - pongTimeoutMilliSeconds,
          [wsConnection, that = shared_from_this(), hdl, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
            if (that->wsConnectionByIdMap.find(wsConnection.id) != that->wsConnectionByIdMap.end()) {
              if (ec) {
                CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", ping timer error: " + ec.message());
                that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
              } else {
                if (that->wsConnectionByIdMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                  ErrorCode ec;
                  pingMethod(hdl, ec);
                  if (ec) {
                    that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping");
                  }
                  if (pongTimeoutMilliSeconds <= 0) {
                    return;
                  }
                  if (that->pongTimeOutTimerByMethodByConnectionIdMap.find(wsConnection.id) != that->pongTimeOutTimerByMethodByConnectionIdMap.end() &&
                      that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id).find(method) !=
                          that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id).end()) {
                    that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnection.id).at(method)->cancel();
                  }
                  that->pongTimeOutTimerByMethodByConnectionIdMap[wsConnection.id][method] = that->serviceContextPtr->tlsClientPtr->set_timer(
                      pongTimeoutMilliSeconds, [wsConnection, that, hdl, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
                        if (that->wsConnectionByIdMap.find(wsConnection.id) != that->wsConnectionByIdMap.end()) {
                          if (ec) {
                            CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", pong time out timer error: " + ec.message());
                            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                          } else {
                            if (that->wsConnectionByIdMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                              auto now = UtilTime::now();
                              if (that->lastPongTpByMethodByConnectionIdMap.find(wsConnection.id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                                  that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).find(method) !=
                                      that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).end() &&
                                  std::chrono::duration_cast<std::chrono::milliseconds>(
                                      now - that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).at(method))
                                          .count() >= pongTimeoutMilliSeconds) {
                                auto thisWsConnection = wsConnection;
                                ErrorCode ec;
                                that->close(thisWsConnection, hdl, websocketpp::close::status::normal, "pong timeout", ec);
                                if (ec) {
                                  that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
                                }
                                that->shouldProcessRemainingMessageOnClosingByConnectionIdMap[thisWsConnection.id] = true;
                              } else {
                                auto thisWsConnection = wsConnection;
                                that->setPingPongTimer(method, thisWsConnection, hdl, pingMethod);
                              }
                            }
                          }
                        }
                      });
                }
              }
            }
          });
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  http::verb convertHttpMethodStringToMethod(const std::string& methodString) {
    std::string methodStringUpper = UtilString::toUpper(methodString);
    return http::string_to_verb(methodStringUpper);
  }
  virtual void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) {}
  std::string apiKeyName;
  std::string apiSecretName;
  std::string exchangeName;
  std::string baseUrl;
  std::string baseUrlRest;
  std::function<void(Event& event, Queue<Event>* eventQueue)> eventHandler;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  ServiceContextPtr serviceContextPtr;
  tcp::resolver resolver;
  std::string hostRest;
  std::string portRest;
  tcp::resolver::results_type tcpResolverResultsRest;
  Queue<std::shared_ptr<HttpConnection>> httpConnectionPool;
  TimePoint lastHttpConnectionPoolPushBackTp{std::chrono::seconds{0}};
  TimerPtr httpConnectionPoolPurgeTimer;
  std::map<std::string, std::string> credentialDefault;
  std::map<std::string, TimerPtr> sendRequestDelayTimerByCorrelationIdMap;
  std::map<std::string, WsConnection> wsConnectionByIdMap;
  std::map<std::string, bool> shouldProcessRemainingMessageOnClosingByConnectionIdMap;
  std::map<std::string, int> connectNumRetryOnFailByConnectionUrlMap;
  std::map<std::string, TimerPtr> connectRetryOnFailTimerByConnectionIdMap;
  std::map<std::string, std::map<PingPongMethod, TimePoint>> lastPongTpByMethodByConnectionIdMap;
  std::map<std::string, std::map<PingPongMethod, TimerPtr>> pingTimerByMethodByConnectionIdMap;
  std::map<std::string, std::map<PingPongMethod, TimerPtr>> pongTimeOutTimerByMethodByConnectionIdMap;
  std::map<PingPongMethod, long> pingIntervalMilliSecondsByMethodMap;
  std::map<PingPongMethod, long> pongTimeoutMilliSecondsByMethodMap;
  std::atomic<bool> shouldContinue{true};
  std::map<std::string, std::map<std::string, std::string>> extraPropertyByConnectionIdMap;
  bool enableCheckPingPongWebsocketProtocolLevel{};
  bool enableCheckPingPongWebsocketApplicationLevel{};
  std::map<Request::Operation, Message::Type> requestOperationToMessageTypeMap;
  // std::regex convertNumberToStringInJsonRegex{"(\\[|,|\":)\\s?(-?\\d+\\.?\\d*)"};
  // std::string convertNumberToStringInJsonRewrite{"$1\"$2\""};
  bool needDecompressWebsocketMessage{};
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) && (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || \
                                                  defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) && (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP))
  struct monostate {};
  websocketpp::extensions_workaround::permessage_deflate::enabled<monostate> inflater;
#endif
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
