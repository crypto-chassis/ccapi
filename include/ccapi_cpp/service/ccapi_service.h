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
#ifndef CCAPI_WEBSOCKET_WRITE_BUFFER_SIZE
#define CCAPI_WEBSOCKET_WRITE_BUFFER_SIZE 1 << 20
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
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#include "websocketpp/config/boost_config.hpp"
#include "websocketpp/client.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/config/asio_client.hpp"
#else
#include "boost/beast/websocket.hpp"
#endif
// clang-format on

#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) &&                                                                                                      \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) &&                                                                                             \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_BITMART))
#include <iomanip>
#include <sstream>

#include "ccapi_cpp/websocketpp_decompress_workaround.h"
#endif
#else
#include "ccapi_cpp/ccapi_inflate_stream.h"
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
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
namespace wspp = websocketpp;
#endif
namespace rj = rapidjson;
namespace ccapi {
/**
 * Defines a service which provides access to exchange API and normalizes them. This is a base class that implements generic functionalities for dealing with
 * exchange REST and Websocket APIs. The Session object is responsible for routing requests and subscriptions to the desired concrete service.
 */
class Service : public std::enable_shared_from_this<Service> {
 public:
  typedef std::shared_ptr<ServiceContext> ServiceContextPtr;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  typedef wspp::lib::error_code ErrorCode;
#else
  typedef boost::system::error_code ErrorCode;  // a.k.a. beast::error_code
#endif
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
        resolverWs(*serviceContextPtr->ioContextPtr),
        httpConnectionPool(sessionOptions.httpConnectionPoolMaxSize) {
    this->enableCheckPingPongWebsocketProtocolLevel = this->sessionOptions.enableCheckPingPongWebsocketProtocolLevel;
    this->enableCheckPingPongWebsocketApplicationLevel = this->sessionOptions.enableCheckPingPongWebsocketApplicationLevel;
    // this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = sessionOptions.pingWebsocketProtocolLevelIntervalMilliSeconds;
    // this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = sessionOptions.pongWebsocketProtocolLevelTimeoutMilliSeconds;
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
  void purgeHttpConnectionPool() { this->httpConnectionPool.purge(); }
  void stop() {
    for (const auto& x : this->sendRequestDelayTimerByCorrelationIdMap) {
      x.second->cancel();
    }
    sendRequestDelayTimerByCorrelationIdMap.clear();
    this->shouldContinue = false;
    for (const auto& x : this->wsConnectionByIdMap) {
      ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      auto wsConnection = x.second;
      this->close(wsConnection, wsConnection.hdl, websocketpp::close::status::normal, "stop", ec);
#else
      auto wsConnectionPtr = x.second;
      this->close(wsConnectionPtr, beast::websocket::close_code::normal, beast::websocket::close_reason("stop"), ec);
#endif
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
      }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
#else
      this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnectionPtr->id] = false;
#endif
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
      TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(delayMilliSeconds)));
      timerPtr->async_wait([that = shared_from_this(), request, req, retry, eventQueuePtr](ErrorCode const& ec) mutable {
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
      this->sendRequestDelayTimerByCorrelationIdMap[request.getCorrelationId()] = timerPtr;
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
  // static std::string printableString(const char* s, size_t n) {
  //   std::string output(s, n);
  //   std::replace(output.begin(), output.end(), '\x01', '^');
  //   return output;
  // }
  // static std::string printableString(const std::string& s) {
  //   std::string output(s);
  //   std::replace(output.begin(), output.end(), '\x01', '^');
  //   return output;
  // }
  typedef ServiceContext::SslContextPtr SslContextPtr;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  typedef ServiceContext::TlsClient TlsClient;
#endif
  typedef std::shared_ptr<boost::asio::steady_timer> TimerPtr;
  void setHostRestFromUrlRest(std::string baseUrlRest) {
    auto hostPort = this->extractHostFromUrl(baseUrlRest);
    this->hostRest = hostPort.first;
    this->portRest = hostPort.second;
  }
  void setHostWsFromUrlWs(std::string baseUrlWs) {
    auto hostPort = this->extractHostFromUrl(baseUrlWs);
    this->hostWs = hostPort.first;
    this->portWs = hostPort.second;
  }
  std::pair<std::string, std::string> extractHostFromUrl(std::string baseUrl) {
    std::string host;
    std::string port;
    if (!baseUrl.empty()) {
      auto splitted1 = UtilString::split(baseUrl, "://");
      auto splitted2 = UtilString::split(UtilString::split(splitted1.at(1), "/").at(0), ":");
      host = splitted2.at(0);
      if (splitted2.size() == 2) {
        port = splitted2.at(1);
      } else {
        if (splitted1.at(0) == "https" || splitted1.at(0) == "wss") {
          port = CCAPI_HTTPS_PORT_DEFAULT;
        } else {
          port = CCAPI_HTTP_PORT_DEFAULT;
        }
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
      streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr,
                                                                           this->hostRest);
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
      streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, host);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(host, port, streamPtr));
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    std::shared_ptr<tcp::resolver> newResolverPtr(new tcp::resolver(*this->serviceContextPtr->ioContextPtr));
    newResolverPtr->async_resolve(host, port,
                                  beast::bind_front_handler(&Service::onResolve, shared_from_this(), httpConnectionPtr, newResolverPtr, req, errorHandler,
                                                            responseHandler, timeoutMilliSeconds));
  }
  void onResolve(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<tcp::resolver> newResolverPtr, http::request<http::string_body> req,
                 std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                 long timeoutMilliSeconds, beast::error_code ec, tcp::resolver::results_type tcpNewResolverResults) {
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    this->startConnect(httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliSeconds, tcpNewResolverResults);
  }
  void startConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                    std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                    long timeoutMilliSeconds, tcp::resolver::results_type tcpNewResolverResults) {
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    if (timeoutMilliSeconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(timeoutMilliSeconds));
    }
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
        tcpNewResolverResults, beast::bind_front_handler(&Service::onConnect, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
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
    CCAPI_LOGGER_TRACE("before ssl async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&Service::onSslHandshake, shared_from_this(), httpConnectionPtr, req, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after ssl async_handshake");
  }
  void onSslHandshake(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                      std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                      beast::error_code ec) {
    CCAPI_LOGGER_TRACE("ssl async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    CCAPI_LOGGER_TRACE("ssl handshaked");
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
  template <class T>
  std::shared_ptr<T> createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr, const std::string& host) {
    std::shared_ptr<T> streamPtr(new T(*iocPtr, *ctxPtr));
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
      beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }
    return streamPtr;
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
  template <>
  std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> createStream(std::shared_ptr<net::io_context> iocPtr,
                                                                                               std::shared_ptr<net::ssl::context> ctxPtr,
                                                                                               const std::string& host) {
    CCAPI_LOGGER_TRACE("host = " + host)
    std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> streamPtr(
        new beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>(*iocPtr, *ctxPtr));
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(streamPtr->next_layer().native_handle(), host.c_str())) {
      beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }
    return streamPtr;
  }
#endif
  // std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr,
  //                                                                    const std::string& host) {
  //   std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(new beast::ssl_stream<beast::tcp_stream>(*iocPtr, *ctxPtr));
  //   // Set SNI Hostname (many hosts need this to handshake successfully)
  //   if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
  //     beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
  //     CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
  //     throw ec;
  //   }
  //   return streamPtr;
  // }
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
    CCAPI_LOGGER_TRACE("before ssl async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&Service::onSslHandshake_2, shared_from_this(), httpConnectionPtr, request, req, retry, eventQueuePtr));
    CCAPI_LOGGER_TRACE("after ssl async_handshake");
  }
  void onSslHandshake_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                        Queue<Event>* eventQueuePtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("ssl async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "ssl handshake", {request.getCorrelationId()}, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("ssl handshaked");
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
      this->httpConnectionPool.purge();
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
    TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(5000)));
    timerPtr->async_wait([that = shared_from_this()](ErrorCode const& ec) {
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
    this->httpConnectionPoolPurgeTimer = timerPtr;
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
          Url url(resPtr->base()
                      .at("Location")
#if BOOST_VERSION < 108100
                      // Boost Beast 1.81 uses boost::core::string_view which doesn't contain to_string() method
                      .to_string()
#endif
          );
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
  virtual bool doesHttpBodyContainError(const std::string& body) { return false; }
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
            streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr,
                                                                                 this->hostRest);
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
              streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr,
                                                                                   this->serviceContextPtr->sslContextPtr, this->hostRest);
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
  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId, const std::string symbolIdCalled) {
    rjValue.AddMember(rj::Value(symbolIdCalled.c_str(), allocator).Move(), rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId, const std::string symbolIdCalled) {
    if (!symbolId.empty()) {
      queryString += symbolIdCalled;
      queryString += "=";
      queryString += Url::urlEncode(symbolId);
      queryString += "&";
    }
  }
  void setupCredential(std::vector<std::string> nameList) {
    for (const auto& x : nameList) {
      if (this->sessionConfigs.getCredential().find(x) != this->sessionConfigs.getCredential().end()) {
        this->credentialDefault.insert(std::make_pair(x, this->sessionConfigs.getCredential().at(x)));
      } else if (!UtilSystem::getEnvAsString(x).empty()) {
        this->credentialDefault.insert(std::make_pair(x, UtilSystem::getEnvAsString(x)));
      }
    }
  }
  http::verb convertHttpMethodStringToMethod(const std::string& methodString) {
    std::string methodStringUpper = UtilString::toUpper(methodString);
    return http::string_to_verb(methodStringUpper);
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
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
    element.insert(CCAPI_CONNECTION_URL, wsConnection.url);
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
    TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(seconds * 1000)));
    timerPtr->async_wait([thisWsConnection, that = shared_from_this(), urlBase](ErrorCode const& ec) {
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
    this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] = timerPtr;
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
    element.insert(CCAPI_CONNECTION_URL, wsConnection.url);
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
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) &&                                                                                                      \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) &&                                                                                             \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_BITMART))
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
      TimerPtr timerPtr(
          new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pingIntervalMilliSeconds - pongTimeoutMilliSeconds)));
      timerPtr->async_wait([wsConnection, that = shared_from_this(), hdl, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
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
              TimerPtr timerPtr(new boost::asio::steady_timer(*that->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pongTimeoutMilliSeconds)));
              timerPtr->async_wait([wsConnection, that, hdl, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
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
                          std::chrono::duration_cast<std::chrono::milliseconds>(now - that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).at(method))
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
              that->pongTimeOutTimerByMethodByConnectionIdMap[wsConnection.id][method] = timerPtr;
            }
          }
        }
      });
      this->pingTimerByMethodByConnectionIdMap[wsConnection.id][method] = timerPtr;
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) {}

#else

  void close(std::shared_ptr<WsConnection> wsConnectionPtr, beast::websocket::close_code const code, beast::websocket::close_reason reason, ErrorCode& ec) {
    WsConnection& wsConnection = *wsConnectionPtr;
    if (wsConnection.status == WsConnection::Status::CLOSING) {
      CCAPI_LOGGER_WARN("websocket connection is already in the state of closing");
      return;
    }
    wsConnection.status = WsConnection::Status::CLOSING;
    wsConnection.remoteCloseCode = code;
    wsConnection.remoteCloseReason = reason;
    wsConnectionPtr->streamPtr->async_close(code, beast::bind_front_handler(&Service::onClose, shared_from_this(), wsConnectionPtr));
  }
  virtual void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) { this->connect(wsConnectionPtr); }
  virtual void connect(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = *wsConnectionPtr;
    wsConnection.status = WsConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("connection initialization on id " + wsConnection.id);
    std::string url = wsConnection.url;
    CCAPI_LOGGER_DEBUG("url = " + url);
    this->startResolveWs(wsConnectionPtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void startResolveWs(std::shared_ptr<WsConnection> wsConnectionPtr) {
    std::shared_ptr<tcp::resolver> newResolverPtr(new tcp::resolver(*this->serviceContextPtr->ioContextPtr));
    newResolverPtr->async_resolve(wsConnectionPtr->host, wsConnectionPtr->port,
                                  beast::bind_front_handler(&Service::onResolveWs, shared_from_this(), wsConnectionPtr, newResolverPtr));
  }
  void onResolveWs(std::shared_ptr<WsConnection> wsConnectionPtr, std::shared_ptr<tcp::resolver> newResolverPtr, beast::error_code ec,
                   tcp::resolver::results_type tcpNewResolverResultsWs) {
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "dns resolve", wsConnectionPtr->correlationIdList);
      return;
    }
    this->startConnectWs(wsConnectionPtr, this->sessionOptions.websocketConnectTimeoutMilliSeconds, tcpNewResolverResultsWs);
  }
  void startConnectWs(std::shared_ptr<WsConnection> wsConnectionPtr, long timeoutMilliSeconds, tcp::resolver::results_type tcpResolverResults) {
    beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>& stream = *wsConnectionPtr->streamPtr;
    if (timeoutMilliSeconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(timeoutMilliSeconds));
    }
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(tcpResolverResults, beast::bind_front_handler(&Service::onConnectWs, shared_from_this(), wsConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_connect");
  }
  void onConnectWs(std::shared_ptr<WsConnection> wsConnectionPtr, beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(wsConnectionPtr);
      return;
    }
    CCAPI_LOGGER_TRACE("connected");
    CCAPI_LOGGER_TRACE("ep.port() = " + std::to_string(ep.port()));
    wsConnectionPtr->hostHttpHeaderValue = this->hostWs + ':' + std::to_string(ep.port());
    beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>& stream = *wsConnectionPtr->streamPtr;
    beast::get_lowest_layer(stream).socket().set_option(tcp::no_delay(true));
    CCAPI_LOGGER_TRACE("before ssl async_handshake");
    stream.next_layer().async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Service::onSslHandshakeWs, shared_from_this(), wsConnectionPtr));
    CCAPI_LOGGER_TRACE("after ssl async_handshake");
  }
  void onSslHandshakeWs(std::shared_ptr<WsConnection> wsConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("ssl async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "ssl handshake", wsConnectionPtr->correlationIdList);
      return;
    }
    CCAPI_LOGGER_TRACE("ssl handshaked");
    beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>& stream = *wsConnectionPtr->streamPtr;
    beast::get_lowest_layer(stream).expires_never();
    beast::websocket::stream_base::timeout opt{std::chrono::milliseconds(this->sessionOptions.websocketConnectTimeoutMilliSeconds),
                                               std::chrono::milliseconds(this->sessionOptions.pongWebsocketProtocolLevelTimeoutMilliSeconds), true};

    stream.set_option(opt);
    stream.set_option(beast::websocket::stream_base::decorator([wsConnectionPtr](beast::websocket::request_type& req) {
      req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING));
      for (const auto& kv : wsConnectionPtr->headers) {
        req.set(kv.first, kv.second);
      }
    }));
    CCAPI_LOGGER_TRACE("before ws async_handshake");
    stream.async_handshake(wsConnectionPtr->hostHttpHeaderValue, wsConnectionPtr->path,
                           beast::bind_front_handler(&Service::onWsHandshakeWs, shared_from_this(), wsConnectionPtr));
    CCAPI_LOGGER_TRACE("after ws async_handshake");
  }
  void onWsHandshakeWs(std::shared_ptr<WsConnection> wsConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("ws async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "ws handshake", wsConnectionPtr->correlationIdList);
      return;
    }
    CCAPI_LOGGER_TRACE("ws handshaked");
    this->onOpen(wsConnectionPtr);
    this->wsConnectionByIdMap.insert(std::make_pair(wsConnectionPtr->id, wsConnectionPtr));
    CCAPI_LOGGER_TRACE("about to start read");
    this->startReadWs(wsConnectionPtr);
    auto& stream = *wsConnectionPtr->streamPtr;
    stream.control_callback(beast::bind_front_handler(&Service::onControlCallback, shared_from_this(), wsConnectionPtr));
  }
  void startReadWs(std::shared_ptr<WsConnection> wsConnectionPtr) {
    auto& stream = *wsConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    auto& connectionId = wsConnectionPtr->id;
    auto& readMessageBuffer = this->readMessageBufferByConnectionIdMap[connectionId];
    stream.async_read(readMessageBuffer, beast::bind_front_handler(&Service::onReadWs, shared_from_this(), wsConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onReadWs(std::shared_ptr<WsConnection> wsConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("n = " + toString(n));
    auto now = UtilTime::now();
    if (ec) {
      if (ec == beast::error::timeout) {
        CCAPI_LOGGER_TRACE("timeout, connection closed");
      } else {
        CCAPI_LOGGER_TRACE("fail");
        Event event;
        event.setType(Event::Type::SESSION_STATUS);
        Message message;
        message.setTimeReceived(now);
        message.setType(Message::Type::SESSION_CONNECTION_DOWN);
        message.setCorrelationIdList(wsConnectionPtr->correlationIdList);
        Element element(true);
        auto& connectionId = wsConnectionPtr->id;
        element.insert(CCAPI_CONNECTION_ID, connectionId);
        message.setElementList({element});
        event.setMessageList({message});
        this->eventHandler(event, nullptr);
        this->onFail(wsConnectionPtr);
      }
      return;
    }
    if (wsConnectionPtr->status != WsConnection::Status::OPEN) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }
    auto& connectionId = wsConnectionPtr->id;
    auto& readMessageBuffer = this->readMessageBufferByConnectionIdMap[connectionId];
    this->onMessage(wsConnectionPtr, (const char*)readMessageBuffer.data().data(), readMessageBuffer.size());
    readMessageBuffer.consume(readMessageBuffer.size());
    this->startReadWs(wsConnectionPtr);
    this->onPongByMethod(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnectionPtr, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    WsConnection& wsConnection = *wsConnectionPtr;
    wsConnection.status = WsConnection::Status::OPEN;
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " established");
    auto urlBase = UtilString::split(wsConnection.url, "?").at(0);
    this->connectNumRetryOnFailByConnectionUrlMap[urlBase] = 0;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    std::vector<std::string> correlationIdList = wsConnection.correlationIdList;
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnection.id);
    element.insert(CCAPI_CONNECTION_URL, wsConnection.url);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    if (this->enableCheckPingPongWebsocketProtocolLevel) {
      this->setPingPongTimer(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnectionPtr,
                             [wsConnectionPtr, that = shared_from_this()](ErrorCode& ec) { that->ping(wsConnectionPtr, "", ec); });
    }
    if (this->enableCheckPingPongWebsocketApplicationLevel) {
      this->setPingPongTimer(PingPongMethod::WEBSOCKET_APPLICATION_LEVEL, wsConnectionPtr,
                             [wsConnectionPtr, that = shared_from_this()](ErrorCode& ec) { that->pingOnApplicationLevel(wsConnectionPtr, ec); });
    }
  }
  void writeMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const char* data, size_t dataSize) {
    if (wsConnectionPtr->status != WsConnection::Status::OPEN) {
      CCAPI_LOGGER_WARN("should write no more messages");
      return;
    }
    auto& connectionId = wsConnectionPtr->id;
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    auto& writeMessageBufferBoundary = this->writeMessageBufferBoundaryByConnectionIdMap[connectionId];
    size_t n = writeMessageBufferWrittenLength;
    memcpy(writeMessageBuffer.data() + n, data, dataSize);
    writeMessageBufferBoundary.push_back(dataSize);
    n += dataSize;
    CCAPI_LOGGER_DEBUG("about to send " + std::string(data, dataSize));
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    if (writeMessageBufferWrittenLength == 0) {
      CCAPI_LOGGER_TRACE("about to start write");
      this->startWriteWs(wsConnectionPtr, writeMessageBuffer.data(), writeMessageBufferBoundary.front());
    }
    writeMessageBufferWrittenLength = n;
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    CCAPI_LOGGER_TRACE("writeMessageBufferBoundary = " + toString(writeMessageBufferBoundary));
  }
  void startWriteWs(std::shared_ptr<WsConnection> wsConnectionPtr, const char* data, size_t numBytesToWrite) {
    auto& stream = *wsConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_write");
    CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
    stream.binary(false);
    stream.async_write(boost::asio::buffer(data, numBytesToWrite), beast::bind_front_handler(&Service::onWriteWs, shared_from_this(), wsConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWriteWs(std::shared_ptr<WsConnection> wsConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto& connectionId = wsConnectionPtr->id;
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    auto& writeMessageBufferBoundary = this->writeMessageBufferBoundaryByConnectionIdMap[connectionId];
    writeMessageBufferWrittenLength -= writeMessageBufferBoundary.front();
    writeMessageBufferBoundary.erase(writeMessageBufferBoundary.begin());
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    CCAPI_LOGGER_TRACE("writeMessageBufferBoundary = " + toString(writeMessageBufferBoundary));
    if (writeMessageBufferWrittenLength > 0) {
      std::memmove(writeMessageBuffer.data(), writeMessageBuffer.data() + n, writeMessageBufferWrittenLength);
      CCAPI_LOGGER_TRACE("about to start write");
      this->startWriteWs(wsConnectionPtr, writeMessageBuffer.data(), writeMessageBufferBoundary.front());
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onFail_(std::shared_ptr<WsConnection> wsConnectionPtr) {
    WsConnection& wsConnection = *wsConnectionPtr;
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
    TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(seconds * 1000)));
    timerPtr->async_wait([wsConnectionPtr, that = shared_from_this(), urlBase](ErrorCode const& ec) {
      WsConnection& thisWsConnection = *wsConnectionPtr;
      if (that->wsConnectionByIdMap.find(thisWsConnection.id) == that->wsConnectionByIdMap.end()) {
        if (ec) {
          CCAPI_LOGGER_ERROR("wsConnection = " + toString(thisWsConnection) + ", connect retry on fail timer error: " + ec.message());
          that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
        } else {
          CCAPI_LOGGER_INFO("about to retry");
          try {
            auto thatWsConnectionPtr = that->createWsConnectionPtr(wsConnectionPtr);
            that->prepareConnect(thatWsConnectionPtr);
            that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "create stream", wsConnectionPtr->correlationIdList);
            return;
          }
        }
      }
    });
    this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] = timerPtr;
  }
  std::shared_ptr<WsConnection> createWsConnectionPtr(std::shared_ptr<WsConnection> wsConnectionPtr) {
    std::shared_ptr<WsConnection> thatWsConnectionPtr = wsConnectionPtr;
    std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> streamPtr(nullptr);
    try {
      streamPtr = this->createStream<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(this->serviceContextPtr->ioContextPtr,
                                                                                                     this->serviceContextPtr->sslContextPtr, this->hostWs);
    } catch (const beast::error_code& ec) {
      throw ec;
    }
    thatWsConnectionPtr->streamPtr = streamPtr;
    return thatWsConnectionPtr;
  }
  virtual void onFail(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->clearStates(wsConnectionPtr);
    this->onFail_(wsConnectionPtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void clearStates(std::shared_ptr<WsConnection> wsConnectionPtr) {
    WsConnection& wsConnection = *wsConnectionPtr;
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
    this->readMessageBufferByConnectionIdMap.erase(wsConnection.id);
    this->writeMessageBufferByConnectionIdMap.erase(wsConnection.id);
    this->writeMessageBufferWrittenLengthByConnectionIdMap.erase(wsConnection.id);
  }
  virtual void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    WsConnection& wsConnection = *wsConnectionPtr;
    wsConnection.status = WsConnection::Status::CLOSED;
    CCAPI_LOGGER_INFO("connection " + toString(wsConnection) + " is closed");
    std::stringstream s;
    s << "close code: " << wsConnectionPtr->remoteCloseCode << " (" << std::to_string(wsConnectionPtr->remoteCloseCode)
      << "), close reason: " << wsConnectionPtr->remoteCloseReason.reason;
    std::string reason = s.str();
    CCAPI_LOGGER_INFO("reason is " + reason);
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_DOWN);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnection.id);
    element.insert(CCAPI_CONNECTION_URL, wsConnection.url);
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
    this->clearStates(wsConnectionPtr);
    auto thisWsConnectionPtr = this->createWsConnectionPtr(wsConnectionPtr);
    this->wsConnectionByIdMap.erase(wsConnectionPtr->id);
    if (this->shouldContinue.load()) {
      this->prepareConnect(thisWsConnectionPtr);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const char* data, size_t dataSize) {
    auto now = UtilTime::now();
    WsConnection& wsConnection = *wsConnectionPtr;
    CCAPI_LOGGER_DEBUG("received a message from connection " + toString(wsConnection));
    if (wsConnection.status != WsConnection::Status::OPEN && !this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id]) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }
    auto& stream = *wsConnectionPtr->streamPtr;
    if (stream.got_text()) {
      boost::beast::string_view textMessage(data, dataSize);
      CCAPI_LOGGER_DEBUG(std::string("received a text message: ") + std::string(textMessage));
      try {
        this->onTextMessage(wsConnectionPtr, textMessage, now);
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR(std::string("textMessage = ") + std::string(textMessage));
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, e);
      }
    } else if (stream.got_binary()) {
      CCAPI_LOGGER_DEBUG(std::string("received a binary message: ") + UtilAlgorithm::stringToHex(std::string(data, dataSize)));
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) &&                                                                                                      \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) &&                                                                                             \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_BITMART))
      if (this->needDecompressWebsocketMessage) {
        std::string decompressed;
        boost::beast::string_view payload(data, dataSize);
        try {
          ErrorCode ec = this->inflater.decompress(reinterpret_cast<const uint8_t*>(&payload[0]), payload.size(), decompressed);
          if (ec) {
            CCAPI_LOGGER_FATAL(ec.message());
          }
          CCAPI_LOGGER_DEBUG("decompressed = " + decompressed);
          this->onTextMessage(wsConnectionPtr, decompressed, now);
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
  void onControlCallback(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::websocket::frame_type kind, boost::beast::string_view payload) {
    if (kind == boost::beast::websocket::frame_type::ping) {
      this->onPing(wsConnectionPtr, payload);
    } else if (kind == boost::beast::websocket::frame_type::pong) {
      this->onPong(wsConnectionPtr, payload);
    } else if (kind == boost::beast::websocket::frame_type::close) {
      this->onClose(wsConnectionPtr, ErrorCode());
    }
  }
  void onPong(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload) {
    auto now = UtilTime::now();
    this->onPongByMethod(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnectionPtr, now);
  }
  void onPongByMethod(PingPongMethod method, std::shared_ptr<WsConnection> wsConnectionPtr, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE(pingPongMethodToString(method) + ": received a pong from " + toString(*wsConnectionPtr));
    this->lastPongTpByMethodByConnectionIdMap[wsConnectionPtr->id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onPing(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("received a ping from " + toString(*wsConnectionPtr));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void send(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload, ErrorCode& ec) {
    this->writeMessage(wsConnectionPtr, payload.data(), payload.length());
  }
  void ping(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload, ErrorCode& ec) {
    if (!this->wsConnectionPendingPingingByIdMap[wsConnectionPtr->id]) {
      auto& stream = *wsConnectionPtr->streamPtr;
      stream.async_ping("", [that = this, wsConnectionPtr](ErrorCode const& ec) { that->wsConnectionPendingPingingByIdMap[wsConnectionPtr->id] = false; });
      this->wsConnectionPendingPingingByIdMap[wsConnectionPtr->id] = true;
    }
  }
  virtual void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) {}
  void setPingPongTimer(PingPongMethod method, std::shared_ptr<WsConnection> wsConnectionPtr, std::function<void(ErrorCode&)> pingMethod) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliSeconds = this->pingIntervalMilliSecondsByMethodMap[method];
    auto pongTimeoutMilliSeconds = this->pongTimeoutMilliSecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliSeconds = " + toString(pingIntervalMilliSeconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliSeconds = " + toString(pongTimeoutMilliSeconds));
    if (pingIntervalMilliSeconds <= pongTimeoutMilliSeconds) {
      return;
    }
    WsConnection& wsConnection = *wsConnectionPtr;
    if (wsConnection.status == WsConnection::Status::OPEN) {
      if (this->pingTimerByMethodByConnectionIdMap.find(wsConnection.id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).find(method) != this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(wsConnection.id).at(method)->cancel();
      }
      TimerPtr timerPtr(
          new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pingIntervalMilliSeconds - pongTimeoutMilliSeconds)));
      timerPtr->async_wait([wsConnectionPtr, that = shared_from_this(), pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
        WsConnection& wsConnection = *wsConnectionPtr;
        if (that->wsConnectionByIdMap.find(wsConnection.id) != that->wsConnectionByIdMap.end()) {
          if (ec) {
            CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", ping timer error: " + ec.message());
            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
          } else {
            if (that->wsConnectionByIdMap.at(wsConnection.id)->status == WsConnection::Status::OPEN) {
              ErrorCode ec;
              pingMethod(ec);
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
              TimerPtr timerPtr(new boost::asio::steady_timer(*that->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pongTimeoutMilliSeconds)));
              timerPtr->async_wait([wsConnectionPtr, that, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
                WsConnection& wsConnection = *wsConnectionPtr;
                if (that->wsConnectionByIdMap.find(wsConnection.id) != that->wsConnectionByIdMap.end()) {
                  if (ec) {
                    CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", pong time out timer error: " + ec.message());
                    that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                  } else {
                    if (that->wsConnectionByIdMap.at(wsConnection.id)->status == WsConnection::Status::OPEN) {
                      auto now = UtilTime::now();
                      if (that->lastPongTpByMethodByConnectionIdMap.find(wsConnection.id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                          that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).find(method) !=
                              that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).end() &&
                          std::chrono::duration_cast<std::chrono::milliseconds>(now - that->lastPongTpByMethodByConnectionIdMap.at(wsConnection.id).at(method))
                                  .count() >= pongTimeoutMilliSeconds) {
                        auto thisWsConnectionPtr = wsConnectionPtr;
                        ErrorCode ec;
                        that->close(thisWsConnectionPtr, beast::websocket::close_code::normal,
                                    beast::websocket::close_reason(beast::websocket::close_code::normal, "pong timeout"), ec);
                        if (ec) {
                          that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
                        }
                        that->shouldProcessRemainingMessageOnClosingByConnectionIdMap[thisWsConnectionPtr->id] = true;
                      } else {
                        auto thisWsConnectionPtr = wsConnectionPtr;
                        that->setPingPongTimer(method, thisWsConnectionPtr, pingMethod);
                      }
                    }
                  }
                }
              });
              that->pongTimeOutTimerByMethodByConnectionIdMap[wsConnection.id][method] = timerPtr;
            }
          }
        }
      });
      this->pingTimerByMethodByConnectionIdMap[wsConnection.id][method] = timerPtr;
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessage, const TimePoint& timeReceived) {}
#endif
  std::string apiKeyName;
  std::string apiSecretName;
  std::string exchangeName;
  std::string baseUrlWs;
  std::string baseUrlRest;
  std::function<void(Event& event, Queue<Event>* eventQueue)> eventHandler;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  ServiceContextPtr serviceContextPtr;
  tcp::resolver resolver, resolverWs;
  std::string hostRest;
  std::string portRest;
  std::string hostWs;
  std::string portWs;
  tcp::resolver::results_type tcpResolverResultsRest, tcpResolverResultsWs;
  Queue<std::shared_ptr<HttpConnection>> httpConnectionPool;
  TimePoint lastHttpConnectionPoolPushBackTp{std::chrono::seconds{0}};
  TimerPtr httpConnectionPoolPurgeTimer;
  std::map<std::string, std::string> credentialDefault;
  std::map<std::string, TimerPtr> sendRequestDelayTimerByCorrelationIdMap;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  std::map<std::string, WsConnection> wsConnectionByIdMap;
#else
  std::map<std::string, std::shared_ptr<WsConnection>> wsConnectionByIdMap;  // TODO(cryptochassis): for consistency, to be renamed to wsConnectionPtrByIdMap
  std::map<std::string, beast::flat_buffer> readMessageBufferByConnectionIdMap;
  std::map<std::string, std::array<char, CCAPI_WEBSOCKET_WRITE_BUFFER_SIZE>> writeMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> writeMessageBufferWrittenLengthByConnectionIdMap;
  std::map<std::string, std::vector<size_t>> writeMessageBufferBoundaryByConnectionIdMap;
#endif
  std::map<std::string, bool> wsConnectionPendingPingingByIdMap;
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
#if defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) &&                                                                                                      \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)) || \
    defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) &&                                                                                             \
        (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_BITMART))
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  struct monostate {};
  websocketpp::extensions_workaround::permessage_deflate::enabled<monostate> inflater;
#else
  InflateStream inflater;
#endif
#endif
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
