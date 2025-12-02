#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_

#if (defined(CCAPI_ENABLE_SERVICE_MARKET_DATA) &&                                                                                                   \
     (defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP))) || \
    (defined(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT) &&                                                                                          \
     (defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_BITMART)))
#define CCAPI_REQUIRES_INFLATE_STREAM 1
#else
#define CCAPI_REQUIRES_INFLATE_STREAM 0
#endif

#ifndef CCAPI_HTTP_RESPONSE_PARSER_BODY_LIMIT
#define CCAPI_HTTP_RESPONSE_PARSER_BODY_LIMIT (8 * 1024 * 1024)
#endif

#ifndef CCAPI_JSON_PARSE_BUFFER_SIZE
#define CCAPI_JSON_PARSE_BUFFER_SIZE (8 * 1024 * 1024)
#endif

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
#define CCAPI_WEBSOCKET_WRITE_BUFFER_SIZE (1 << 20)
#endif

#include <regex>

#include "boost/asio/strand.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/version.hpp"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_market_data_message.h"
#include "ccapi_cpp/ccapi_util_private.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
// clang-format off

#include "boost/beast/websocket.hpp"

// clang-format on

#include "ccapi_cpp/ccapi_fix_connection.h"
#include "ccapi_cpp/ccapi_http_connection.h"
#include "ccapi_cpp/ccapi_http_retry.h"
#if CCAPI_REQUIRES_INFLATE_STREAM
#include "ccapi_cpp/ccapi_inflate_stream.h"
#endif
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
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace rj = rapidjson;

namespace ccapi {

/**
 * Defines a service which provides access to exchange API and normalizes them. This is a base class that implements generic functionalities for dealing with
 * exchange REST and Websocket APIs. The Session object is responsible for routing requests and subscriptions to the desired concrete service.
 */
class Service : public std::enable_shared_from_this<Service> {
 public:
  typedef ServiceContext* ServiceContextPtr;

  typedef boost::system::error_code ErrorCode;  // a.k.a. beast::error_code

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
        jsonDocumentAllocator(jsonParseBuffer.data(), jsonParseBuffer.size()) {
    this->enableCheckPingPongWebsocketProtocolLevel = this->sessionOptions.enableCheckPingPongWebsocketProtocolLevel;
    this->enableCheckPingPongWebsocketApplicationLevel = this->sessionOptions.enableCheckPingPongWebsocketApplicationLevel;
    // this->pingIntervalMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = sessionOptions.pingWebsocketProtocolLevelIntervalMilliseconds;
    // this->pongTimeoutMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = sessionOptions.pongWebsocketProtocolLevelTimeoutMilliseconds;
    this->pingIntervalMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] = sessionOptions.pingWebsocketApplicationLevelIntervalMilliseconds;
    this->pongTimeoutMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] = sessionOptions.pongWebsocketApplicationLevelTimeoutMilliseconds;
    this->pingIntervalMillisecondsByMethodMap[PingPongMethod::FIX_PROTOCOL_LEVEL] = sessionOptions.heartbeatFixIntervalMilliseconds;
    this->pongTimeoutMillisecondsByMethodMap[PingPongMethod::FIX_PROTOCOL_LEVEL] = sessionOptions.heartbeatFixTimeoutMilliseconds;
  }

  void startHttpConnectionPoolIfEnabled() {
    if (this->sessionOptions.enableHttpConnectionPoolMultiIP && !this->sessionOptions.httpConnectionPoolBindIPs.empty()) {
      CCAPI_LOGGER_INFO("Initializing HTTP connection pool with " + std::to_string(this->sessionOptions.httpConnectionPoolBindIPs.size()) + " IPs");
      this->initializeHttpConnectionPool();
    }
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
    // 清理HTTP连接池保活定时器
    this->stopAllHttpConnectionPoolKeepAliveTimers();
  }

  void purgeHttpConnectionPool() {
    this->httpConnectionPool.clear();
    this->stopAllHttpConnectionPoolKeepAliveTimers();
  }

  void purgeHttpConnectionPool(const std::string& localIpAddress) {
    this->httpConnectionPool.erase(localIpAddress);
    this->stopHttpConnectionPoolKeepAliveTimers(localIpAddress);
  }

  void purgeHttpConnectionPool(const std::string& localIpAddress, const std::string& baseUrl) {
    this->httpConnectionPool[localIpAddress].erase(baseUrl);
    this->stopHttpConnectionPoolKeepAliveTimer(localIpAddress, baseUrl);
  }

  void forceCloseWebsocketConnections() {
    for (const auto& x : this->wsConnectionPtrByIdMap) {
      ErrorCode ec;
      auto wsConnectionPtr = x.second;
      this->close(wsConnectionPtr, beast::websocket::close_code::normal, beast::websocket::close_reason("force close"), ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
      }
    }
  }

  void stop() {
    for (const auto& x : this->sendRequestDelayTimerByCorrelationIdMap) {
      x.second->cancel();
    }
    sendRequestDelayTimerByCorrelationIdMap.clear();
    this->shouldContinue = false;
    for (const auto& x : this->wsConnectionPtrByIdMap) {
      ErrorCode ec;
      auto wsConnectionPtr = x.second;
      this->close(wsConnectionPtr, beast::websocket::close_code::normal, beast::websocket::close_reason("stop"), ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
      }
      this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnectionPtr->id] = false;
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

  virtual void processSuccessfulTextMessageRest(int statusCode, const Request& request, boost::beast::string_view textMessageView,
                                                const TimePoint& timeReceived, Queue<Event>* eventQueuePtr) {}

  std::shared_ptr<std::future<void>> sendRequest(Request& request, const bool useFuture, const TimePoint& now, long delayMilliseconds,
                                                 Queue<Event>* eventQueuePtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("request = " + toString(request));
    CCAPI_LOGGER_DEBUG("useFuture = " + toString(useFuture));
    TimePoint then;
    if (delayMilliseconds > 0) {
      then = now + std::chrono::milliseconds(delayMilliseconds);
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
    if (delayMilliseconds > 0) {
      auto timerPtr = std::make_shared<net::steady_timer>(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(delayMilliseconds));
      timerPtr->async_wait([that = shared_from_this(), request, req, retry, eventQueuePtr](ErrorCode const& ec) mutable {
        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
            CCAPI_LOGGER_ERROR("request = " + toString(request) + ", sendRequest timer error: " + ec.message());
            that->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, ec, "timer", {request.getCorrelationId()}, eventQueuePtr);
          }
        } else {
          auto now = UtilTime::now();
          request.setTimeSent(now);
          that->tryRequest(request, req, retry, eventQueuePtr);
        }
        that->sendRequestDelayTimerByCorrelationIdMap.erase(request.getCorrelationId());
      });
      this->sendRequestDelayTimerByCorrelationIdMap[request.getCorrelationId()] = timerPtr;
    } else {
      request.setTimeSent(now);
      net::post(*this->serviceContextPtr->ioContextPtr,
                [that = shared_from_this(), request, req, retry, eventQueuePtr]() mutable { that->tryRequest(request, req, retry, eventQueuePtr); });
    }
    std::shared_ptr<std::future<void>> futurePtr(nullptr);
    if (useFuture) {
      futurePtr = std::make_shared<std::future<void>>(std::move(promisePtr->get_future()));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return futurePtr;
  }

  virtual void sendRequestByWebsocket(const std::string& websocketOrderEntrySubscriptionCorrelationId, Request& request, const TimePoint& now) {}

  virtual void sendRequestByFix(const std::string& fixOrderEntrySubscriptionCorrelationId, Request& request, const TimePoint& now) {}

  virtual void subscribe(Subscription& subscription) {}

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

  void onResponseError(const Request& request, int statusCode, boost::beast::string_view errorMessageView, Queue<Event>* eventQueuePtr) {
    std::string statusCodeStr = std::to_string(statusCode);
    CCAPI_LOGGER_ERROR("request = " + toString(request) + ", statusCode = " + statusCodeStr + ", errorMessage = " + std::string(errorMessageView));
    Event event;
    event.setType(Event::Type::RESPONSE);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setType(Message::Type::RESPONSE_ERROR);
    message.setCorrelationIdList({request.getCorrelationId()});
    Element element;
    element.insert(CCAPI_HTTP_STATUS_CODE, statusCodeStr);
    element.insert(CCAPI_ERROR_MESSAGE, UtilString::trim(std::string(errorMessageView)));
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event, eventQueuePtr);
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  typedef ServiceContext::SslContextPtr SslContextPtr;

  typedef std::shared_ptr<net::steady_timer> TimerPtr;

  void setHostRestFromUrlRest(std::string baseUrlRest) {
    auto hostPort = this->extractHostFromUrl(baseUrlRest);
    this->hostRest = hostPort.first;
    this->portRest = hostPort.second;
  }

  //   void setHostWsFromUrlWs(std::string baseUrlWs) {
  //     auto hostPort = this->extractHostFromUrl(baseUrlWs);
  //     this->hostWs = hostPort.first;
  //     this->portWs = hostPort.second;
  //   }

  //   void setHostWsFromUrlWsOrderEntry(std::string baseUrlWsOrderEntry) {
  //     auto hostPort = this->extractHostFromUrl(baseUrlWs);
  //     this->hostWsOrderEntry = hostPort.first;
  //     this->portWsOrderEntry = hostPort.second;
  //   }

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
                   std::function<void(const http::response<http::string_body>&)> responseHandler, long timeoutMilliseconds) {
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
#endif
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr{nullptr};
    try {
      streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr,
                                                                           this->hostRest);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    auto httpConnectionPtr = std::make_shared<HttpConnection>(this->hostRest, this->portRest, streamPtr);
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    auto newResolverPtr = std::make_shared<tcp::resolver>(*this->serviceContextPtr->ioContextPtr);
    CCAPI_LOGGER_TRACE("this->hostRest = " + this->hostRest);
    CCAPI_LOGGER_TRACE("this->portRest = " + this->portRest);
    newResolverPtr->async_resolve(this->hostRest, this->portRest,
                                  beast::bind_front_handler(&Service::onResolve, shared_from_this(), httpConnectionPtr, newResolverPtr, req, errorHandler,
                                                            responseHandler, timeoutMilliseconds));
    // this->startConnect(httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliseconds, this->tcpResolverResultsRest);
  }

  void sendRequest(const std::string& host, const std::string& port, const http::request<http::string_body>& req,
                   std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                   long timeoutMilliseconds) {
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n" + oss.str());
#endif
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr{nullptr};
    try {
      streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, host);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    auto httpConnectionPtr = std::make_shared<HttpConnection>(host, port, streamPtr);
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    auto newResolverPtr = std::make_shared<tcp::resolver>(*this->serviceContextPtr->ioContextPtr);
    CCAPI_LOGGER_TRACE("host = " + host);
    CCAPI_LOGGER_TRACE("port = " + port);
    newResolverPtr->async_resolve(host, port,
                                  beast::bind_front_handler(&Service::onResolve, shared_from_this(), httpConnectionPtr, newResolverPtr, req, errorHandler,
                                                            responseHandler, timeoutMilliseconds));
  }

  void onResolve(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<tcp::resolver> newResolverPtr, http::request<http::string_body> req,
                 std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                 long timeoutMilliseconds, beast::error_code ec, tcp::resolver::results_type tcpNewResolverResults) {
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
    }
    this->startConnect(httpConnectionPtr, req, errorHandler, responseHandler, timeoutMilliseconds, tcpNewResolverResults);
  }

  void startConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, http::request<http::string_body> req,
                    std::function<void(const beast::error_code&)> errorHandler, std::function<void(const http::response<http::string_body>&)> responseHandler,
                    long timeoutMilliseconds, tcp::resolver::results_type tcpNewResolverResults) {
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    if (timeoutMilliseconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(timeoutMilliseconds));
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
    auto reqPtr = std::make_shared<http::request<http::string_body>>(std::move(req));
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
    httpConnectionPtr->clearBuffer();
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    std::shared_ptr<http::response_parser<http::string_body>> resParserPtr = std::make_shared<http::response_parser<http::string_body>>();
    resParserPtr->body_limit(CCAPI_HTTP_RESPONSE_PARSER_BODY_LIMIT);
    http::async_read(stream, httpConnectionPtr->buffer, *resParserPtr,
                     beast::bind_front_handler(&Service::onRead, shared_from_this(), httpConnectionPtr, reqPtr, resParserPtr, errorHandler, responseHandler));
    CCAPI_LOGGER_TRACE("after async_read");
  }

  void onRead(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<http::request<http::string_body>> reqPtr,
              std::shared_ptr<http::response_parser<http::string_body>> resParserPtr, std::function<void(const beast::error_code&)> errorHandler,
              std::function<void(const http::response<http::string_body>&)> responseHandler, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    auto resPtr = &resParserPtr->get();
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      errorHandler(ec);
      return;
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
    responseHandler(*resPtr);
  }

  template <class T>
  std::shared_ptr<T> createStream(net::io_context* iocPtr, net::ssl::context* ctxPtr, const std::string& host) {
    auto streamPtr = std::make_shared<T>(*iocPtr, *ctxPtr);

    // Set SNI hostname (important for TLS handshakes)
    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
      beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }

    return streamPtr;
  }

  void performRequestWithNewHttpConnection(std::shared_ptr<HttpConnection> httpConnectionPtr, const Request& request, http::request<http::string_body>& req,
                                           const HttpRetry& retry, Queue<Event>* eventQueuePtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("httpConnection = " + toString(*httpConnectionPtr));
    CCAPI_LOGGER_DEBUG("request = " + toString(request));
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_DEBUG("this->sessionOptions.httpRequestTimeoutMilliseconds = " + toString(this->sessionOptions.httpRequestTimeoutMilliseconds));
    if (this->sessionOptions.httpRequestTimeoutMilliseconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliseconds));
    }
    const auto& localIpAddress = request.getLocalIpAddress();
    CCAPI_LOGGER_TRACE("localIpAddress = " + localIpAddress);
    if (!localIpAddress.empty()) {
      if (!beast::get_lowest_layer(stream).socket().is_open()) {
        ErrorCode ec;
        CCAPI_LOGGER_TRACE("before socket open");
        beast::get_lowest_layer(stream).socket().open(net::ip::tcp::v4(), ec);
        if (ec) {
          CCAPI_LOGGER_TRACE("fail");
          this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "socket open", {request.getCorrelationId()}, eventQueuePtr);
          return;
        }
      }
      ErrorCode ec;
      tcp::endpoint existingLocalEndpoint = beast::get_lowest_layer(stream).socket().local_endpoint(ec);
      if (ec) {
        CCAPI_LOGGER_TRACE("fail");
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "socket get local endpoint", {request.getCorrelationId()},
                      eventQueuePtr);
        return;
      }
      tcp::endpoint localEndpoint(net::ip::make_address(localIpAddress),
                                  0);  // Note: Setting the port to 0 means the OS will select a free port for you
      if (localEndpoint != existingLocalEndpoint) {
        ErrorCode ec;
        CCAPI_LOGGER_TRACE("before socket bind");
        beast::get_lowest_layer(stream).socket().bind(localEndpoint, ec);
        if (ec) {
          CCAPI_LOGGER_TRACE("fail");
          this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "socket bind", {request.getCorrelationId()}, eventQueuePtr);
          return;
        }
      }
    }
    auto newResolverPtr = std::make_shared<tcp::resolver>(*this->serviceContextPtr->ioContextPtr);
    CCAPI_LOGGER_TRACE("httpConnectionPtr->host = " + httpConnectionPtr->host);
    CCAPI_LOGGER_TRACE("httpConnectionPtr->port = " + httpConnectionPtr->port);
    newResolverPtr->async_resolve(
        httpConnectionPtr->host, httpConnectionPtr->port,
        beast::bind_front_handler(&Service::onResolveWorkaround, shared_from_this(), httpConnectionPtr, newResolverPtr, request, req, retry, eventQueuePtr));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void onResolveWorkaround(std::shared_ptr<HttpConnection> httpConnectionPtr, std::shared_ptr<tcp::resolver> newResolverPtr, Request request,
                           http::request<http::string_body> req, HttpRetry retry, Queue<Event>* eventQueuePtr, beast::error_code ec,
                           tcp::resolver::results_type tcpNewResolverResults) {
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "DNS resolve", {request.getCorrelationId()}, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("before asyncConnectWorkaround");
    TimerPtr timerPtr{nullptr};
    if (this->sessionOptions.httpRequestTimeoutMilliseconds > 0) {
      timerPtr = std::make_shared<boost::asio::steady_timer>(*this->serviceContextPtr->ioContextPtr,
                                                             std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliseconds));
      timerPtr->async_wait([httpConnectionPtr](ErrorCode const& ec) {
        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
            CCAPI_LOGGER_ERROR("httpConnectionPtr = " + toString(*httpConnectionPtr) + ", connect timeout timer error: " + ec.message());
          }
        } else {
          CCAPI_LOGGER_TRACE("httpConnectionPtr = " + toString(*httpConnectionPtr) + ", connect timeout timer triggered");
          beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
          beast::get_lowest_layer(stream).socket().cancel();
        }
      });
    }
    this->asyncConnectWorkaround(httpConnectionPtr, timerPtr, request, req, retry, eventQueuePtr, tcpNewResolverResults, 0);
    CCAPI_LOGGER_TRACE("after asyncConnectWorkaround");
  }

  // used to avoid asio close and reopen the socket and therefore losing the bound local ip address
  void asyncConnectWorkaround(std::shared_ptr<HttpConnection> httpConnectionPtr, TimerPtr timerPtr, Request request, http::request<http::string_body> req,
                              HttpRetry retry, Queue<Event>* eventQueuePtr, tcp::resolver::results_type tcpNewResolverResults,
                              size_t tcpNewResolverResultsIndex) {
    auto it = tcpNewResolverResults.begin();
    std::advance(it, tcpNewResolverResultsIndex);
    if (it == tcpNewResolverResults.end()) {
      ErrorCode ec = net::error::make_error_code(net::error::misc_errors::not_found);
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "connect", {request.getCorrelationId()}, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    beast::get_lowest_layer(stream).socket().async_connect(
        *it, beast::bind_front_handler(&Service::onConnect_2, shared_from_this(), httpConnectionPtr, timerPtr, request, req, retry, eventQueuePtr,
                                       tcpNewResolverResults, tcpNewResolverResultsIndex));
    CCAPI_LOGGER_TRACE("after async_connect");
  }

  void onConnect_2(std::shared_ptr<HttpConnection> httpConnectionPtr, TimerPtr timerPtr, Request request, http::request<http::string_body> req, HttpRetry retry,
                   Queue<Event>* eventQueuePtr, tcp::resolver::results_type tcpNewResolverResults, size_t tcpNewResolverResultsIndex, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    CCAPI_LOGGER_TRACE("local endpoint has address " + beast::get_lowest_layer(*httpConnectionPtr->streamPtr).socket().local_endpoint().address().to_string());
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      if (ec == net::error::make_error_code(net::error::basic_errors::operation_aborted)) {
        CCAPI_LOGGER_TRACE("fail");
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "connect attempt timeout", {request.getCorrelationId()}, eventQueuePtr);
        return;
      }
      this->asyncConnectWorkaround(httpConnectionPtr, timerPtr, request, req, retry, eventQueuePtr, tcpNewResolverResults, tcpNewResolverResultsIndex + 1);
      return;
    }
    timerPtr->cancel();
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
    if (this->sessionOptions.httpRequestTimeoutMilliseconds > 0) {
      beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliseconds));
    }
    auto reqPtr = std::make_shared<http::request<http::string_body>>(std::move(req));
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
      this->httpConnectionPool[request.getLocalIpAddress()][request.getBaseUrl()].clear();
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry, eventQueuePtr);
      return;
    }
    CCAPI_LOGGER_TRACE("written");
    httpConnectionPtr->clearBuffer();
    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    std::shared_ptr<http::response_parser<http::string_body>> resParserPtr = std::make_shared<http::response_parser<http::string_body>>();
    resParserPtr->body_limit(CCAPI_HTTP_RESPONSE_PARSER_BODY_LIMIT);
    http::async_read(stream, httpConnectionPtr->buffer, *resParserPtr,
                     beast::bind_front_handler(&Service::onRead_2, shared_from_this(), httpConnectionPtr, request, reqPtr, resParserPtr, retry, eventQueuePtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }

  void onRead_2(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body>> reqPtr,
                std::shared_ptr<http::response_parser<http::string_body>> resParserPtr, HttpRetry retry, Queue<Event>* eventQueuePtr, beast::error_code ec,
                std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    CCAPI_LOGGER_TRACE("local endpoint has address " + beast::get_lowest_layer(*httpConnectionPtr->streamPtr).socket().local_endpoint().address().to_string());
    auto resPtr = &resParserPtr->get();
    auto now = UtilTime::now();
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "read", {request.getCorrelationId()}, eventQueuePtr);
      this->httpConnectionPool[request.getLocalIpAddress()][request.getBaseUrl()].clear();
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry, eventQueuePtr);
      return;
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
    boost::beast::string_view bodyView(resPtr->body());
    try {
      if (statusCode / 100 == 2) {
        this->processSuccessfulTextMessageRest(statusCode, request, bodyView, now, eventQueuePtr);
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
        this->onResponseError(request, statusCode, bodyView, eventQueuePtr);
        return;
      } else if (statusCode / 100 == 4) {
        this->onResponseError(request, statusCode, bodyView, eventQueuePtr);
      } else if (statusCode / 100 == 5) {
        this->onResponseError(request, statusCode, bodyView, eventQueuePtr);
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
    if (!this->sessionOptions.enableOneHttpConnectionPerRequest) {
      httpConnectionPtr->lastReceiveDataTp = now;
      const auto& localIpAddress = request.getLocalIpAddress();
      const auto& requestBaseUrl = request.getBaseUrl();
      if (this->sessionOptions.httpConnectionPoolMaxSize > 0 &&
          this->httpConnectionPool[localIpAddress][requestBaseUrl].size() >= this->sessionOptions.httpConnectionPoolMaxSize) {
        CCAPI_LOGGER_TRACE("httpConnectionPool is full for localIpAddress = " + localIpAddress + ", requestBaseUrl = " + toString(requestBaseUrl));
        this->httpConnectionPool[localIpAddress][requestBaseUrl].pop_front();
      }
      this->httpConnectionPool[localIpAddress][requestBaseUrl].push_back(httpConnectionPtr);
      CCAPI_LOGGER_TRACE("pushed back httpConnectionPtr " + toString(*httpConnectionPtr) + " to httpConnectionPool for localIpAddress = " + localIpAddress +
                         ", requestBaseUrl = " + toString(requestBaseUrl));
    }
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    if (retry.promisePtr) {
      retry.promisePtr->set_value();
    }
  }

  virtual bool doesHttpBodyContainError(boost::beast::string_view bodyView) { return false; }

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
        const auto& localIpAddress = request.getLocalIpAddress();
        const auto& requestBaseUrl = request.getBaseUrl();

        // 如果这是第一次请求且启用了多IP连接池,为所有IP预建立连接
        if (!this->httpConnectionPoolInitialized && this->sessionOptions.enableHttpConnectionPoolMultiIP &&
            !this->sessionOptions.httpConnectionPoolBindIPs.empty() && !requestBaseUrl.empty()) {
          CCAPI_LOGGER_INFO("First REST request detected, initializing connection pool with URL: " + requestBaseUrl);
          this->initializeHttpConnectionPoolWithUrl(requestBaseUrl);
        }

        if (this->sessionOptions.enableOneHttpConnectionPerRequest || this->httpConnectionPool[localIpAddress][requestBaseUrl].empty() ||
            std::chrono::duration_cast<std::chrono::seconds>(request.getTimeSent() -
                                                             this->httpConnectionPool[localIpAddress][requestBaseUrl].back()->lastReceiveDataTp)
                    .count() >= this->sessionOptions.httpConnectionKeepAliveTimeoutSeconds) {
          this->httpConnectionPool[localIpAddress][requestBaseUrl].clear();
          std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr{nullptr};
          try {
            streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr,
                                                                                 this->hostRest);
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream", {request.getCorrelationId()}, eventQueuePtr);
            return;
          }
          std::string host, port;
          if (requestBaseUrl.empty()) {
            host = this->hostRest;
            port = this->portRest;
          } else {
            host = request.getHost();
            port = request.getPort();
          }
          auto httpConnectionPtr = std::make_shared<HttpConnection>(host, port, streamPtr);
          CCAPI_LOGGER_WARN("about to perform request with new httpConnectionPtr " + toString(*httpConnectionPtr) + " for request = " + toString(request) +
                            ", localIpAddress = " + localIpAddress + ", requestBaseUrl = " + toString(requestBaseUrl));
          this->performRequestWithNewHttpConnection(httpConnectionPtr, request, req, retry, eventQueuePtr);
        } else {
          std::shared_ptr<HttpConnection> httpConnectionPtr = this->httpConnectionPool[localIpAddress][requestBaseUrl].back();
          this->httpConnectionPool[localIpAddress][requestBaseUrl].pop_back();
          CCAPI_LOGGER_TRACE("about to perform request with existing httpConnectionPtr " + toString(*httpConnectionPtr) +
                             " for localIpAddress = " + localIpAddress + ", requestBaseUrl = " + toString(requestBaseUrl));
          this->startWrite_2(httpConnectionPtr, request, req, retry, eventQueuePtr);
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
    req.set(http::field::host, request.getBaseUrl().empty() ? this->hostRest : request.getHost());
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

  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> standardizationMap = {},
                   const std::map<std::string_view, std::function<std::string(const std::string&)>> conversionMap = {}) {
    int i = 0;
    for (const auto& kv : param) {
      std::string key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += key;
      queryString += "=";
      std::string value = conversionMap.find(kv.first) != conversionMap.end() ? conversionMap.at(kv.first)(kv.second) : kv.second;
      queryString += Url::urlEncode(value);
      queryString += "&";
      ++i;
    }
  }

  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId, const std::string& symbolIdCalled) {
    rjValue.AddMember(rj::Value(symbolIdCalled.c_str(), allocator).Move(), rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }

  void appendSymbolId(std::string& queryString, const std::string& symbolId, const std::string& symbolIdCalled) {
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

  void close(std::shared_ptr<WsConnection> wsConnectionPtr, beast::websocket::close_code const code, beast::websocket::close_reason reason, ErrorCode& ec) {
    if (wsConnectionPtr->status == WsConnection::Status::CLOSING) {
      CCAPI_LOGGER_WARN("websocket connection is already in the state of closing");
      return;
    }
    wsConnectionPtr->status = WsConnection::Status::CLOSING;
    wsConnectionPtr->remoteCloseCode = code;
    wsConnectionPtr->remoteCloseReason = reason;

    std::visit([&](auto& streamPtr) { streamPtr->async_close(code, beast::bind_front_handler(&Service::onClose, shared_from_this(), wsConnectionPtr)); },
               wsConnectionPtr->streamPtr);
  }

  virtual void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) { this->connect(wsConnectionPtr); }

  virtual void connect(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    wsConnectionPtr->status = WsConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("wsConnectionPtr = " + wsConnectionPtr->toString());
    this->startResolveWs(wsConnectionPtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void startResolveWs(std::shared_ptr<WsConnection> wsConnectionPtr) {
    auto newResolverPtr = std::make_shared<tcp::resolver>(*this->serviceContextPtr->ioContextPtr);
    CCAPI_LOGGER_TRACE("wsConnectionPtr = " + wsConnectionPtr->toString());
    CCAPI_LOGGER_TRACE("wsConnectionPtr->host = " + wsConnectionPtr->host);
    CCAPI_LOGGER_TRACE("wsConnectionPtr->port = " + wsConnectionPtr->port);
    CCAPI_LOGGER_TRACE("wsConnectionPtr->proxyUrl = " + wsConnectionPtr->proxyUrl);
    std::string host;
    std::string port;
    if (wsConnectionPtr->proxyUrl.empty()) {
      host = wsConnectionPtr->host;
      port = wsConnectionPtr->port;
    } else {
      const auto& splitted = UtilString::split(wsConnectionPtr->proxyUrl, ':');
      host = splitted.at(0);
      port = splitted.size() > 1 ? splitted.at(1) : CCAPI_HTTP_PORT_DEFAULT;
    }
    newResolverPtr->async_resolve(host, port, beast::bind_front_handler(&Service::onResolveWs, shared_from_this(), wsConnectionPtr, newResolverPtr));
  }

  void onResolveWs(std::shared_ptr<WsConnection> wsConnectionPtr, std::shared_ptr<tcp::resolver> newResolverPtr, beast::error_code ec,
                   tcp::resolver::results_type tcpNewResolverResultsWs) {
    if (ec) {
      CCAPI_LOGGER_TRACE("dns resolve fail");
      this->onFail(wsConnectionPtr);
      return;
    }
    this->startConnectWs(wsConnectionPtr, this->sessionOptions.websocketConnectTimeoutMilliseconds, tcpNewResolverResultsWs);
  }

  void startConnectWs(std::shared_ptr<WsConnection> wsConnectionPtr, long timeoutMilliseconds, tcp::resolver::results_type tcpResolverResults) {
    std::visit(
        [&](auto& streamPtr) {
          using StreamType = std::decay_t<decltype(*streamPtr)>;

          if (timeoutMilliseconds > 0) {
            beast::get_lowest_layer(*streamPtr).expires_after(std::chrono::milliseconds(timeoutMilliseconds));
          }

          if constexpr (std::is_same_v<StreamType, beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>>) {
            // Set SNI hostname (only for WSS)
            if (!SSL_set_tlsext_host_name(streamPtr->next_layer().native_handle(), wsConnectionPtr->host.c_str())) {
              beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
              CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
              this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "set SNI Hostname", wsConnectionPtr->correlationIdList);
              return;
            }
          }

          CCAPI_LOGGER_TRACE("before async_connect");

          beast::get_lowest_layer(*streamPtr)
              .async_connect(tcpResolverResults, beast::bind_front_handler(&Service::onConnectWs, shared_from_this(), wsConnectionPtr));

          CCAPI_LOGGER_TRACE("after async_connect");
        },
        wsConnectionPtr->streamPtr);
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

    wsConnectionPtr->hostHttpHeaderValue = this->hostHttpHeaderValueIgnorePort ? wsConnectionPtr->host : wsConnectionPtr->host + ':' + wsConnectionPtr->port;

    CCAPI_LOGGER_TRACE("wsConnectionPtr->hostHttpHeaderValue = " + wsConnectionPtr->hostHttpHeaderValue);

    // Use std::visit to access the concrete stream
    std::visit(
        [&](auto& streamPtr) {
          using StreamType = std::decay_t<decltype(*streamPtr)>;

          auto& lowestLayer = beast::get_lowest_layer(*streamPtr).socket();

          // Disable Nagle
          lowestLayer.set_option(tcp::no_delay(true));

          if constexpr (std::is_same_v<StreamType, beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>>) {
            CCAPI_LOGGER_TRACE("before ssl async_handshake");

            streamPtr->next_layer().async_handshake(ssl::stream_base::client,
                                                    beast::bind_front_handler(&Service::onSslHandshakeWs, shared_from_this(), wsConnectionPtr));

            CCAPI_LOGGER_TRACE("after ssl async_handshake");
          } else {
            // Non-SSL streams skip SSL handshake and go straight to WebSocket handshake
            this->onSslHandshakeWs(wsConnectionPtr, {});
          }
        },
        wsConnectionPtr->streamPtr);
  }

  void onSslHandshakeWs(std::shared_ptr<WsConnection> wsConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("ssl async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("ssl handshake fail");
      this->onFail(wsConnectionPtr);
      return;
    }
    CCAPI_LOGGER_TRACE("ssl handshaked");

    std::visit(
        [&](auto& streamPtr) {
          auto& stream = *streamPtr;
          beast::get_lowest_layer(stream).expires_never();

          beast::websocket::stream_base::timeout opt{std::chrono::milliseconds(this->sessionOptions.websocketConnectTimeoutMilliseconds),
                                                     std::chrono::milliseconds(this->sessionOptions.pongWebsocketProtocolLevelTimeoutMilliseconds), true};

          stream.set_option(opt);
          stream.set_option(beast::websocket::stream_base::decorator([wsConnectionPtr](beast::websocket::request_type& req) {
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            for (const auto& kv : wsConnectionPtr->headers) {
              req.set(kv.first, kv.second);
            }
          }));

          CCAPI_LOGGER_TRACE("before ws async_handshake");
          stream.async_handshake(wsConnectionPtr->hostHttpHeaderValue, wsConnectionPtr->path,
                                 beast::bind_front_handler(&Service::onWsHandshakeWs, shared_from_this(), wsConnectionPtr));
          CCAPI_LOGGER_TRACE("after ws async_handshake");
        },
        wsConnectionPtr->streamPtr);
  }

  void onWsHandshakeWs(std::shared_ptr<WsConnection> wsConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("ws async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("ws handshake fail");
      this->onFail(wsConnectionPtr);
      return;
    }
    CCAPI_LOGGER_TRACE("ws handshaked");

    // Finalize connection setup
    this->onOpen(wsConnectionPtr);
    this->wsConnectionPtrByIdMap.insert({wsConnectionPtr->id, wsConnectionPtr});
    CCAPI_LOGGER_TRACE("about to start read");

    // Start reading messages
    this->startReadWs(wsConnectionPtr);

    // Setup control callback (ping/pong/close)
    std::visit(
        [&](auto& streamPtr) {
          streamPtr->control_callback(
              [wsConnectionPtr, that = shared_from_this()](boost::beast::websocket::frame_type kind, boost::beast::string_view payload) {
                that->onControlCallback(wsConnectionPtr, kind, payload);
              });
        },
        wsConnectionPtr->streamPtr);
  }

  void startReadWs(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_TRACE("before async_read");
    auto& readMessageBuffer = wsConnectionPtr->readMessageBuffer;

    std::visit(
        [&](auto& streamPtr) { streamPtr->async_read(readMessageBuffer, beast::bind_front_handler(&Service::onReadWs, shared_from_this(), wsConnectionPtr)); },
        wsConnectionPtr->streamPtr);

    CCAPI_LOGGER_TRACE("after async_read");
  }

  void onReadWs(std::shared_ptr<WsConnection> wsConnectionPtr, const ErrorCode& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("n = " + toString(n));
    auto now = UtilTime::now();
    auto& connectionId = wsConnectionPtr->id;
    auto& readMessageBuffer = wsConnectionPtr->readMessageBuffer;
    if (ec) {
      readMessageBuffer.consume(readMessageBuffer.size());
      if (ec == boost::asio::error::operation_aborted) {
        return;
      } else if (ec == beast::error::timeout) {
        CCAPI_LOGGER_TRACE("timeout, connection closed");
      }
      if (wsConnectionPtr->status == WsConnection::Status::CLOSING) {
        return;
      }
      CCAPI_LOGGER_TRACE("fail");
      Event event;
      event.setType(Event::Type::SESSION_STATUS);
      Message message;
      message.setTimeReceived(now);
      message.setType(Message::Type::SESSION_CONNECTION_DOWN);
      message.setCorrelationIdList(wsConnectionPtr->correlationIdList);
      Element element;
      element.insert(CCAPI_CONNECTION_ID, connectionId);
      element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
      message.setElementList({element});
      event.setMessageList({message});
      this->eventHandler(event, nullptr);
      this->onFail(wsConnectionPtr);
      return;
    }
    if (wsConnectionPtr->status != WsConnection::Status::OPEN) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      readMessageBuffer.consume(readMessageBuffer.size());
      return;
    }
    this->onMessage(wsConnectionPtr, (const char*)readMessageBuffer.data().data(), readMessageBuffer.size());
    readMessageBuffer.consume(readMessageBuffer.size());
    this->startReadWs(wsConnectionPtr);
    this->onPongByMethod(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnectionPtr, now, false);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    wsConnectionPtr->status = WsConnection::Status::OPEN;
    CCAPI_LOGGER_INFO("connection " + toString(*wsConnectionPtr) + " established");
    auto urlBase = UtilString::split(wsConnectionPtr->url, "?").at(0);
    this->connectNumRetryOnFailByConnectionUrlMap[urlBase] = 0;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    std::vector<std::string> correlationIdList = wsConnectionPtr->correlationIdList;
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
    element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
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
    auto& writeMessageBuffer = wsConnectionPtr->writeMessageBuffer;
    auto& writeMessageBufferWrittenLength = wsConnectionPtr->writeMessageBufferWrittenLength;
    auto& writeMessageBufferBoundary = wsConnectionPtr->writeMessageBufferBoundary;
    size_t n = writeMessageBufferWrittenLength;
    [[maybe_unused]] const auto& connectionId = wsConnectionPtr->id;
    CCAPI_LOGGER_TRACE("connectionId = " + connectionId);
    memcpy(writeMessageBuffer.data() + n, data, dataSize);
    writeMessageBufferBoundary.push_back(dataSize);
    n += dataSize;
    CCAPI_LOGGER_TRACE("connectionId = " + connectionId);
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
    CCAPI_LOGGER_TRACE("before async_write");
    CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));

    std::visit(
        [&](auto& streamPtr) {
          auto& stream = *streamPtr;  // dereference shared_ptr
          stream.binary(false);
          stream.async_write(net::buffer(data, numBytesToWrite), beast::bind_front_handler(&Service::onWriteWs, shared_from_this(), wsConnectionPtr));
        },
        wsConnectionPtr->streamPtr);

    CCAPI_LOGGER_TRACE("after async_write");
  }

  void onWriteWs(std::shared_ptr<WsConnection> wsConnectionPtr, const ErrorCode& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    if (ec) {
      if (ec == beast::error::timeout) {
        CCAPI_LOGGER_TRACE("timeout, connection closed");
      }
      CCAPI_LOGGER_TRACE("fail");
      Event event;
      event.setType(Event::Type::SESSION_STATUS);
      Message message;
      message.setTimeReceived(now);
      message.setType(Message::Type::SESSION_CONNECTION_DOWN);
      message.setCorrelationIdList(wsConnectionPtr->correlationIdList);
      Element element;
      element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
      element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
      message.setElementList({element});
      event.setMessageList({message});
      this->eventHandler(event, nullptr);
      this->onFail(wsConnectionPtr);
      return;
    }
    auto& writeMessageBuffer = wsConnectionPtr->writeMessageBuffer;
    auto& writeMessageBufferWrittenLength = wsConnectionPtr->writeMessageBufferWrittenLength;
    auto& writeMessageBufferBoundary = wsConnectionPtr->writeMessageBufferBoundary;
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
    wsConnectionPtr->status = WsConnection::Status::FAILED;
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE,
                  "connection " + toString(*wsConnectionPtr) + " has failed before opening", wsConnectionPtr->correlationIdList);
    std::string wsConnectionId = wsConnectionPtr->id;
    std::string wsConnectionUrl = wsConnectionPtr->url;
    this->wsConnectionPtrByIdMap.erase(wsConnectionId);
    auto urlBase = UtilString::split(wsConnectionUrl, "?").at(0);
    long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[urlBase], 6)));
    CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(wsConnectionId) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(wsConnectionId)->cancel();
    }
    TimerPtr timerPtr(new net::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(seconds * 1000)));
    timerPtr->async_wait([wsConnectionPtr, that = shared_from_this(), urlBase](ErrorCode const& ec) {
      WsConnection& thisWsConnection = *wsConnectionPtr;
      if (that->wsConnectionPtrByIdMap.find(thisWsConnection.id) == that->wsConnectionPtrByIdMap.end()) {
        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
            CCAPI_LOGGER_ERROR("wsConnection = " + toString(thisWsConnection) + ", connect retry on fail timer error: " + ec.message());
            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
          }
        } else {
          CCAPI_LOGGER_INFO("about to retry");
          try {
            that->setWsConnectionStream(wsConnectionPtr);
            that->prepareConnect(wsConnectionPtr);
            that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "create stream", wsConnectionPtr->correlationIdList);
            return;
          }
        }
      }
    });
    this->connectRetryOnFailTimerByConnectionIdMap[wsConnectionId] = timerPtr;
  }

  void setWsConnectionStream(std::shared_ptr<WsConnection> wsConnectionPtr) {
    if (wsConnectionPtr->isSecure) {
      wsConnectionPtr->streamPtr = std::make_shared<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(*this->serviceContextPtr->ioContextPtr,
                                                                                                                    *this->serviceContextPtr->sslContextPtr);
    } else {
      wsConnectionPtr->streamPtr = std::make_shared<beast::websocket::stream<beast::tcp_stream>>(*this->serviceContextPtr->ioContextPtr);
    }
  }

  virtual void onFail(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->clearStates(wsConnectionPtr);
    this->onFail_(wsConnectionPtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void clearStates(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_INFO("clear states for wsConnection " + toString(*wsConnectionPtr));
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap.erase(wsConnectionPtr->id);
    this->lastPongTpByMethodByConnectionIdMap.erase(wsConnectionPtr->id);
    this->extraPropertyByConnectionIdMap.erase(wsConnectionPtr->id);
    if (this->pingTimerByMethodByConnectionIdMap.find(wsConnectionPtr->id) != this->pingTimerByMethodByConnectionIdMap.end()) {
      for (const auto& x : this->pingTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id)) {
        x.second->cancel();
      }
      this->pingTimerByMethodByConnectionIdMap.erase(wsConnectionPtr->id);
    }
    if (this->pongTimeOutTimerByMethodByConnectionIdMap.find(wsConnectionPtr->id) != this->pongTimeOutTimerByMethodByConnectionIdMap.end()) {
      for (const auto& x : this->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id)) {
        x.second->cancel();
      }
      this->pongTimeOutTimerByMethodByConnectionIdMap.erase(wsConnectionPtr->id);
    }
    // auto urlBase = UtilString::split(wsConnectionPtr->url, "?").at(0);
    // this->connectNumRetryOnFailByConnectionUrlMap.erase(urlBase);
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(wsConnectionPtr->id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(wsConnectionPtr->id)->cancel();
      this->connectRetryOnFailTimerByConnectionIdMap.erase(wsConnectionPtr->id);
    }
  }

  virtual void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    wsConnectionPtr->status = WsConnection::Status::CLOSED;
    CCAPI_LOGGER_INFO("connection " + toString(*wsConnectionPtr) + " is closed");
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
    element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
    element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
    element.insert(CCAPI_REASON, reason);
    message.setElementList({element});
    std::vector<std::string> correlationIdList;
    for (const auto& subscription : wsConnectionPtr->subscriptionList) {
      correlationIdList.push_back(subscription.getCorrelationId());
    }
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    CCAPI_LOGGER_INFO("connection " + toString(*wsConnectionPtr) + " is closed");
    this->clearStates(wsConnectionPtr);
    this->setWsConnectionStream(wsConnectionPtr);
    this->wsConnectionPtrByIdMap.erase(wsConnectionPtr->id);
    if (this->shouldContinue.load()) {
      this->prepareConnect(wsConnectionPtr);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void onMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const char* data, size_t dataSize) {
    auto now = UtilTime::now();
    CCAPI_LOGGER_DEBUG("received a message from connection " + toString(*wsConnectionPtr));
    if (wsConnectionPtr->status != WsConnection::Status::OPEN && !this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnectionPtr->id]) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }

    std::visit(
        [&](auto& streamPtr) {
          auto& stream = *streamPtr;  // dereference shared_ptr

          if (stream.got_text()) {
            boost::beast::string_view textMessage(data, dataSize);
            CCAPI_LOGGER_DEBUG("received a text message: " + std::string(textMessage));
            try {
              this->onTextMessage(wsConnectionPtr, textMessage, now);
            } catch (const std::exception& e) {
              CCAPI_LOGGER_ERROR("textMessage = " + std::string(textMessage));
              this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, e);
            }
          } else if (stream.got_binary()) {
            CCAPI_LOGGER_DEBUG("received a binary message: " + UtilAlgorithm::stringToHex(std::string(data, dataSize)));

#if CCAPI_REQUIRES_INFLATE_STREAM

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

#endif  // decompress block
          }
        },
        wsConnectionPtr->streamPtr);  // <-- std::variant
  }

  void onControlCallback(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::websocket::frame_type kind, boost::beast::string_view payload) {
    if (kind == boost::beast::websocket::frame_type::ping) {
      this->onPing(wsConnectionPtr, payload);
    } else if (kind == boost::beast::websocket::frame_type::pong) {
      this->onPong(wsConnectionPtr, payload);
    } else if (kind == boost::beast::websocket::frame_type::close) {
    }
  }

  void onPong(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload) {
    auto now = UtilTime::now();
    this->onPongByMethod(PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL, wsConnectionPtr, now, true);
  }

  void onPongByMethod(PingPongMethod method, std::shared_ptr<WsConnection> wsConnectionPtr, const TimePoint& timeReceived, bool truePong) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE(pingPongMethodToString(method) + ": received a " + (truePong ? "websocket protocol pong" : "data message") + " from " +
                       toString(*wsConnectionPtr));
    this->lastPongTpByMethodByConnectionIdMap[wsConnectionPtr->id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void onPing(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    CCAPI_LOGGER_TRACE("received a ping from " + toString(*wsConnectionPtr));
    this->lastPongTpByMethodByConnectionIdMap[wsConnectionPtr->id][PingPongMethod::WEBSOCKET_PROTOCOL_LEVEL] = now;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void send(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload, ErrorCode& ec) {
    this->writeMessage(wsConnectionPtr, payload.data(), payload.length());
  }

  void ping(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view payload, ErrorCode& ec) {
    if (!this->wsConnectionPendingPingingByConnectionIdMap[wsConnectionPtr->id]) {
      std::visit(
          [&](auto& streamPtr) {
            streamPtr->async_ping(boost::beast::websocket::ping_data(payload), [that = this, wsConnectionPtr](ErrorCode const& ec) {
              that->wsConnectionPendingPingingByConnectionIdMap[wsConnectionPtr->id] = false;
            });
            this->wsConnectionPendingPingingByConnectionIdMap[wsConnectionPtr->id] = true;
          },
          wsConnectionPtr->streamPtr);
    }
  }

  virtual void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) {}

  void setPingPongTimer(PingPongMethod method, std::shared_ptr<WsConnection> wsConnectionPtr, std::function<void(ErrorCode&)> pingMethod) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliseconds = this->pingIntervalMillisecondsByMethodMap[method];
    auto pongTimeoutMilliseconds = this->pongTimeoutMillisecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliseconds = " + toString(pingIntervalMilliseconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliseconds = " + toString(pongTimeoutMilliseconds));
    if (pingIntervalMilliseconds <= pongTimeoutMilliseconds) {
      return;
    }
    if (wsConnectionPtr->status == WsConnection::Status::OPEN) {
      if (this->pingTimerByMethodByConnectionIdMap.find(wsConnectionPtr->id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id).find(method) !=
              this->pingTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id).at(method)->cancel();
      }
      auto timerPtr = std::make_shared<net::steady_timer>(*this->serviceContextPtr->ioContextPtr,
                                                          std::chrono::milliseconds(pingIntervalMilliseconds - pongTimeoutMilliseconds));
      timerPtr->async_wait([wsConnectionPtr, that = shared_from_this(), pingMethod, pongTimeoutMilliseconds, method](ErrorCode const& ec) {
        if (that->wsConnectionPtrByIdMap.find(wsConnectionPtr->id) != that->wsConnectionPtrByIdMap.end()) {
          if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
              CCAPI_LOGGER_ERROR("wsConnection = " + toString(*wsConnectionPtr) + ", ping timer error: " + ec.message());
              that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
            }
          } else {
            if (that->wsConnectionPtrByIdMap.at(wsConnectionPtr->id)->status == WsConnection::Status::OPEN) {
              ErrorCode ec;
              pingMethod(ec);
              if (ec) {
                that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping");
              }
              if (pongTimeoutMilliseconds <= 0) {
                return;
              }
              if (that->pongTimeOutTimerByMethodByConnectionIdMap.find(wsConnectionPtr->id) != that->pongTimeOutTimerByMethodByConnectionIdMap.end() &&
                  that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id).find(method) !=
                      that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id).end()) {
                that->pongTimeOutTimerByMethodByConnectionIdMap.at(wsConnectionPtr->id).at(method)->cancel();
              }
              auto timerPtr = std::make_shared<net::steady_timer>(*that->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pongTimeoutMilliseconds));
              timerPtr->async_wait([wsConnectionPtr, that, pingMethod, pongTimeoutMilliseconds, method](ErrorCode const& ec) {
                if (that->wsConnectionPtrByIdMap.find(wsConnectionPtr->id) != that->wsConnectionPtrByIdMap.end()) {
                  if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                      CCAPI_LOGGER_ERROR("wsConnection = " + toString(*wsConnectionPtr) + ", pong time out timer error: " + ec.message());
                      that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                    }
                  } else {
                    if (that->wsConnectionPtrByIdMap.at(wsConnectionPtr->id)->status == WsConnection::Status::OPEN) {
                      auto now = UtilTime::now();
                      if (that->lastPongTpByMethodByConnectionIdMap.find(wsConnectionPtr->id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                          that->lastPongTpByMethodByConnectionIdMap.at(wsConnectionPtr->id).find(method) !=
                              that->lastPongTpByMethodByConnectionIdMap.at(wsConnectionPtr->id).end() &&
                          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                                that->lastPongTpByMethodByConnectionIdMap.at(wsConnectionPtr->id).at(method))
                                  .count() >= pongTimeoutMilliseconds) {
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
              that->pongTimeOutTimerByMethodByConnectionIdMap[wsConnectionPtr->id][method] = timerPtr;
            }
          }
        }
      });
      this->pingTimerByMethodByConnectionIdMap[wsConnectionPtr->id][method] = timerPtr;
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  std::string convertParamTimeSecondsToTimeMilliseconds(const std::string& input) {
    auto dotPosition = input.find('.');
    if (dotPosition == std::string::npos) {
      return std::to_string(std::stoll(input) * 1000);
    } else {
    }
    return std::to_string(std::stoll(input.substr(0, dotPosition)) * 1000 + std::stoll(UtilString::rightPadTo(input.substr(dotPosition + 1, 3), 3, '0')));
  }

  virtual void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessage, const TimePoint& timeReceived) {}

  bool hostHttpHeaderValueIgnorePort{};
  std::string apiKeyName;
  std::string apiSecretName;
  std::string apiPassphraseName;
  std::string exchangeName;
  std::string baseUrlWs;
  std::string baseUrlWsOrderEntry;
  std::string baseUrlRest;
  std::function<void(Event& event, Queue<Event>* eventQueue)> eventHandler;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  ServiceContextPtr serviceContextPtr;
  tcp::resolver resolver, resolverWs;
  std::string hostRest;
  std::string portRest;
  //   std::string hostWs;
  //   std::string portWs;
  //   std::string hostWsOrderEntry;
  //   std::string portWsOrderEntry;
  // tcp::resolver::results_type tcpResolverResultsRest, tcpResolverResultsWs;
  std::map<std::string, std::map<std::string, std::deque<std::shared_ptr<HttpConnection>>>> httpConnectionPool;
  std::map<std::string, std::string> credentialDefault;
  std::map<std::string, TimerPtr> sendRequestDelayTimerByCorrelationIdMap;

  // HTTP连接池主动保活相关
  std::map<std::string, std::map<std::string, TimerPtr>> httpConnectionPoolKeepAliveTimerMap;  // localIP -> baseURL -> timer
  std::map<std::string, std::map<std::string, int>> httpConnectionPoolReconnectAttemptMap;  // localIP -> baseURL -> retry count
  bool httpConnectionPoolInitialized = false;  // 标记连接池是否已初始化

  std::map<std::string, std::shared_ptr<WsConnection>> wsConnectionPtrByIdMap;

  std::map<std::string, bool> wsConnectionPendingPingingByConnectionIdMap;
  std::map<std::string, bool> shouldProcessRemainingMessageOnClosingByConnectionIdMap;
  std::map<std::string, int> connectNumRetryOnFailByConnectionUrlMap;
  std::map<std::string, TimerPtr> connectRetryOnFailTimerByConnectionIdMap;
  std::map<std::string, std::map<PingPongMethod, TimePoint>> lastPongTpByMethodByConnectionIdMap;
  std::map<std::string, std::map<PingPongMethod, TimerPtr>> pingTimerByMethodByConnectionIdMap;
  std::map<std::string, std::map<PingPongMethod, TimerPtr>> pongTimeOutTimerByMethodByConnectionIdMap;
  std::map<PingPongMethod, long> pingIntervalMillisecondsByMethodMap;
  std::map<PingPongMethod, long> pongTimeoutMillisecondsByMethodMap;
  std::atomic<bool> shouldContinue{true};
  std::map<std::string, std::map<std::string, std::string>> extraPropertyByConnectionIdMap;
  bool enableCheckPingPongWebsocketProtocolLevel{};
  bool enableCheckPingPongWebsocketApplicationLevel{};
  std::map<Request::Operation, Message::Type> requestOperationToMessageTypeMap;
  // std::regex convertNumberToStringInJsonRegex{"(\\[|,|\":)\\s?(-?\\d+\\.?\\d*)"};
  // std::string convertNumberToStringInJsonRewrite{"$1\"$2\""};
  bool needDecompressWebsocketMessage{};
#if CCAPI_REQUIRES_INFLATE_STREAM
  InflateStream inflater;
#endif

  std::array<char, CCAPI_JSON_PARSE_BUFFER_SIZE> jsonParseBuffer;
  rj::MemoryPoolAllocator<> jsonDocumentAllocator;

 protected:
  // ========== HTTP连接池主动保活和重连实现 ==========

  void stopHttpConnectionPoolKeepAliveTimer(const std::string& localIpAddress, const std::string& baseUrl) {
    if (this->httpConnectionPoolKeepAliveTimerMap.find(localIpAddress) != this->httpConnectionPoolKeepAliveTimerMap.end() &&
        this->httpConnectionPoolKeepAliveTimerMap[localIpAddress].find(baseUrl) != this->httpConnectionPoolKeepAliveTimerMap[localIpAddress].end()) {
      this->httpConnectionPoolKeepAliveTimerMap[localIpAddress][baseUrl]->cancel();
      this->httpConnectionPoolKeepAliveTimerMap[localIpAddress].erase(baseUrl);
    }
  }

  void stopHttpConnectionPoolKeepAliveTimers(const std::string& localIpAddress) {
    if (this->httpConnectionPoolKeepAliveTimerMap.find(localIpAddress) != this->httpConnectionPoolKeepAliveTimerMap.end()) {
      for (auto& kv : this->httpConnectionPoolKeepAliveTimerMap[localIpAddress]) {
        kv.second->cancel();
      }
      this->httpConnectionPoolKeepAliveTimerMap.erase(localIpAddress);
    }
  }

  void stopAllHttpConnectionPoolKeepAliveTimers() {
    for (auto& ipMap : this->httpConnectionPoolKeepAliveTimerMap) {
      for (auto& kv : ipMap.second) {
        kv.second->cancel();
      }
    }
    this->httpConnectionPoolKeepAliveTimerMap.clear();
  }

  void sendHttpConnectionPoolKeepAliveRequest(const std::string& localIpAddress, const std::string& baseUrl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("Sending keep-alive request for localIpAddress=" + localIpAddress + ", baseUrl=" + baseUrl);

    if (!this->sessionOptions.enableHttpConnectionPoolKeepAlive) {
      CCAPI_LOGGER_DEBUG("Keep-alive disabled, skipping");
      return;
    }

    // 构造保活请求
    http::request<http::string_body> req;
    req.version(11);
    req.keep_alive(true);

    // 设置HTTP方法
    auto method = this->sessionOptions.httpConnectionPoolKeepAliveMethod;
    if (method == "HEAD") {
      req.method(http::verb::head);
    } else if (method == "OPTIONS") {
      req.method(http::verb::options);
    } else if (method == "GET") {
      req.method(http::verb::get);
    } else {
      req.method(http::verb::head);  // 默认使用HEAD
    }

    // 解析baseUrl获取host和port
    auto hostPort = this->extractHostFromUrl(baseUrl);
    std::string host = hostPort.first;
    std::string port = hostPort.second;

    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.target(this->sessionOptions.httpConnectionPoolKeepAlivePath);

    CCAPI_LOGGER_TRACE("Keep-alive request: " + method + " " + this->sessionOptions.httpConnectionPoolKeepAlivePath);

    // 检查连接池中是否有可用连接
    if (!this->httpConnectionPool[localIpAddress][baseUrl].empty()) {
      auto httpConnectionPtr = this->httpConnectionPool[localIpAddress][baseUrl].back();
      this->httpConnectionPool[localIpAddress][baseUrl].pop_back();

      CCAPI_LOGGER_TRACE("Using existing connection for keep-alive");

      // 发送保活请求
      beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
      if (this->sessionOptions.httpRequestTimeoutMilliseconds > 0) {
        beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliseconds));
      }

      auto reqPtr = std::make_shared<http::request<http::string_body>>(std::move(req));
      http::async_write(stream, *reqPtr,
        [that = shared_from_this(), httpConnectionPtr, reqPtr, localIpAddress, baseUrl](beast::error_code ec, std::size_t bytes_transferred) {
          boost::ignore_unused(bytes_transferred);
          if (ec) {
            CCAPI_LOGGER_WARN("Keep-alive write failed: " + ec.message());
            that->httpConnectionPool[localIpAddress][baseUrl].clear();
            // 尝试重连
            if (that->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
              that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
            }
            return;
          }

          // 读取响应
          httpConnectionPtr->clearBuffer();
          beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
          auto resParserPtr = std::make_shared<http::response_parser<http::string_body>>();
          resParserPtr->body_limit(CCAPI_HTTP_RESPONSE_PARSER_BODY_LIMIT);

          http::async_read(stream, httpConnectionPtr->buffer, *resParserPtr,
            [that, httpConnectionPtr, localIpAddress, baseUrl, resParserPtr](beast::error_code ec, std::size_t bytes_transferred) {
              boost::ignore_unused(bytes_transferred);
              if (ec) {
                CCAPI_LOGGER_WARN("Keep-alive read failed: " + ec.message());
                that->httpConnectionPool[localIpAddress][baseUrl].clear();
                // 尝试重连
                if (that->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
                  that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
                }
                return;
              }

              auto now = UtilTime::now();
              httpConnectionPtr->lastReceiveDataTp = now;

              // 将连接归还到池中
              if (that->sessionOptions.httpConnectionPoolMaxSize > 0 &&
                  that->httpConnectionPool[localIpAddress][baseUrl].size() >= that->sessionOptions.httpConnectionPoolMaxSize) {
                that->httpConnectionPool[localIpAddress][baseUrl].pop_front();
              }
              that->httpConnectionPool[localIpAddress][baseUrl].push_back(httpConnectionPtr);

              CCAPI_LOGGER_TRACE("Keep-alive successful, connection returned to pool");

              // 重置重连计数
              that->httpConnectionPoolReconnectAttemptMap[localIpAddress][baseUrl] = 0;
            });
        });
    } else {
      CCAPI_LOGGER_DEBUG("No connection in pool, creating new connection for keep-alive");
      // 如果池中没有连接,尝试预连接
      if (this->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
        this->preConnectHttpConnectionPool(localIpAddress, baseUrl);
      }
    }

    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void startHttpConnectionPoolKeepAlive(const std::string& localIpAddress, const std::string& baseUrl) {
    CCAPI_LOGGER_FUNCTION_ENTER;

    if (!this->sessionOptions.enableHttpConnectionPoolKeepAlive) {
      CCAPI_LOGGER_DEBUG("Keep-alive disabled");
      return;
    }

    long intervalMilliseconds = this->sessionOptions.httpConnectionPoolKeepAliveIntervalSeconds * 1000;
    if (intervalMilliseconds <= 0) {
      CCAPI_LOGGER_WARN("Invalid keep-alive interval: " + std::to_string(intervalMilliseconds));
      return;
    }

    CCAPI_LOGGER_DEBUG("Starting keep-alive timer for localIpAddress=" + localIpAddress +
                       ", baseUrl=" + baseUrl + ", interval=" + std::to_string(intervalMilliseconds) + "ms");

    // 取消现有定时器
    this->stopHttpConnectionPoolKeepAliveTimer(localIpAddress, baseUrl);

    // 创建新定时器
    auto timerPtr = std::make_shared<net::steady_timer>(*this->serviceContextPtr->ioContextPtr,
                                                        std::chrono::milliseconds(intervalMilliseconds));

    timerPtr->async_wait([that = shared_from_this(), localIpAddress, baseUrl, intervalMilliseconds](ErrorCode const& ec) {
      if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
          CCAPI_LOGGER_ERROR("Keep-alive timer error: " + ec.message());
        }
        return;
      }

      // 发送保活请求
      that->sendHttpConnectionPoolKeepAliveRequest(localIpAddress, baseUrl);

      // 重新启动定时器
      that->startHttpConnectionPoolKeepAlive(localIpAddress, baseUrl);
    });

    this->httpConnectionPoolKeepAliveTimerMap[localIpAddress][baseUrl] = timerPtr;

    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void preConnectHttpConnectionPool(const std::string& localIpAddress, const std::string& baseUrl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("Pre-connecting for localIpAddress=" + localIpAddress + ", baseUrl=" + baseUrl);

    // 检查重连次数
    int& reconnectAttempt = this->httpConnectionPoolReconnectAttemptMap[localIpAddress][baseUrl];
    if (this->sessionOptions.enableHttpConnectionPoolAutoReconnect &&
        reconnectAttempt >= this->sessionOptions.httpConnectionPoolReconnectMaxRetries) {
      CCAPI_LOGGER_WARN("Max reconnect attempts reached for localIpAddress=" + localIpAddress + ", baseUrl=" + baseUrl);
      return;
    }

    reconnectAttempt++;

    // 解析baseUrl
    auto hostPort = this->extractHostFromUrl(baseUrl);
    std::string host = hostPort.first;
    std::string port = hostPort.second;

    CCAPI_LOGGER_DEBUG("Connecting to host=" + host + ", port=" + port);

    // 创建新连接
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr{nullptr};
    try {
      streamPtr = this->createStream<beast::ssl_stream<beast::tcp_stream>>(
        this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, host);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_ERROR("Failed to create stream: " + ec.message());
      // 延迟重试
      if (this->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
        auto timerPtr = std::make_shared<net::steady_timer>(
          *this->serviceContextPtr->ioContextPtr,
          std::chrono::milliseconds(this->sessionOptions.httpConnectionPoolReconnectDelayMilliseconds));
        timerPtr->async_wait([that = shared_from_this(), localIpAddress, baseUrl](ErrorCode const& ec) {
          if (!ec) {
            that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
          }
        });
      }
      return;
    }

    auto httpConnectionPtr = std::make_shared<HttpConnection>(host, port, streamPtr);

    // 绑定本地IP
    if (!localIpAddress.empty()) {
      ErrorCode ec;
      beast::get_lowest_layer(*streamPtr).socket().open(net::ip::tcp::v4(), ec);
      if (!ec) {
        tcp::endpoint localEndpoint(net::ip::make_address(localIpAddress), 0);
        beast::get_lowest_layer(*streamPtr).socket().bind(localEndpoint, ec);
      }
      if (ec) {
        CCAPI_LOGGER_ERROR("Failed to bind to local IP: " + ec.message());
        return;
      }
    }

    // 解析DNS
    auto newResolverPtr = std::make_shared<tcp::resolver>(*this->serviceContextPtr->ioContextPtr);
    newResolverPtr->async_resolve(host, port,
      [that = shared_from_this(), httpConnectionPtr, newResolverPtr, localIpAddress, baseUrl]
      (beast::error_code ec, tcp::resolver::results_type results) {
        if (ec) {
          CCAPI_LOGGER_ERROR("DNS resolve failed: " + ec.message());
          // 延迟重试
          if (that->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
            auto timerPtr = std::make_shared<net::steady_timer>(
              *that->serviceContextPtr->ioContextPtr,
              std::chrono::milliseconds(that->sessionOptions.httpConnectionPoolReconnectDelayMilliseconds));
            timerPtr->async_wait([that, localIpAddress, baseUrl](ErrorCode const& ec) {
              if (!ec) {
                that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
              }
            });
          }
          return;
        }

        // 使用workaround方法手动连接,避免async_connect重新打开socket导致bind失效
        that->asyncConnectWorkaroundForPreConnect(httpConnectionPtr, localIpAddress, baseUrl, results, 0);
      });

    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void initializeHttpConnectionPool() {
    initializeHttpConnectionPoolWithUrl(this->baseUrlRest);
  }

  void initializeHttpConnectionPoolWithUrl(const std::string& targetUrl) {
    CCAPI_LOGGER_FUNCTION_ENTER;

    if (!this->sessionOptions.enableHttpConnectionPoolMultiIP) {
      CCAPI_LOGGER_DEBUG("Multi-IP connection pool disabled");
      return;
    }

    if (this->sessionOptions.httpConnectionPoolBindIPs.empty()) {
      CCAPI_LOGGER_WARN("No bind IPs configured for connection pool");
      return;
    }

    // 检查targetUrl是否有效
    if (targetUrl.empty()) {
      CCAPI_LOGGER_WARN("Target URL is empty, cannot initialize connection pool");
      CCAPI_LOGGER_WARN("Connection pool will be initialized on first request");
      return;
    }

    // 检查是否已经初始化过
    if (this->httpConnectionPoolInitialized) {
      CCAPI_LOGGER_DEBUG("Connection pool already initialized");
      return;
    }

    this->httpConnectionPoolInitialized = true;

    size_t numIPs = this->sessionOptions.httpConnectionPoolBindIPs.size();
    size_t poolSize = this->sessionOptions.httpConnectionPoolMaxSize;

    // 计算每个IP应该建立的连接数
    size_t connectionsPerIP = (poolSize > 0 && numIPs > 0) ? (poolSize / numIPs) : 1;
    if (connectionsPerIP == 0) {
      connectionsPerIP = 1;
    }

    CCAPI_LOGGER_INFO("Initializing HTTP connection pool:");
    CCAPI_LOGGER_INFO("  - Total IPs: " + std::to_string(numIPs));
    CCAPI_LOGGER_INFO("  - Pool size: " + std::to_string(poolSize));
    CCAPI_LOGGER_INFO("  - Connections per IP: " + std::to_string(connectionsPerIP));
    CCAPI_LOGGER_INFO("  - Target URL: " + targetUrl);

    // 为每个IP预建立连接
    // 注意: 如果connectionsPerIP=1,我们只为前N-1个IP预建立连接
    // 最后一个IP的连接将由第一次实际请求创建,避免创建多余连接
    size_t numIPsToPreConnect = (connectionsPerIP == 1) ? (numIPs > 0 ? numIPs - 1 : 0) : numIPs;

    for (size_t ipIndex = 0; ipIndex < numIPsToPreConnect; ++ipIndex) {
      const auto& ip = this->sessionOptions.httpConnectionPoolBindIPs[ipIndex];
      CCAPI_LOGGER_INFO("Pre-connecting IP: " + ip);
      for (size_t i = 0; i < connectionsPerIP; ++i) {
        CCAPI_LOGGER_DEBUG("  - Creating connection " + std::to_string(i + 1) + "/" + std::to_string(connectionsPerIP));
        this->preConnectHttpConnectionPool(ip, targetUrl);
      }
    }

    // 如果每个IP只需要1个连接,最后一个IP的连接将由第一次请求创建
    if (connectionsPerIP == 1 && numIPs > 0) {
      CCAPI_LOGGER_INFO("Last IP (" + this->sessionOptions.httpConnectionPoolBindIPs.back() +
                       ") will be connected on first request to avoid creating extra connection");
    }

    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  // 预连接专用的连接workaround方法实现
  void asyncConnectWorkaroundForPreConnect(std::shared_ptr<HttpConnection> httpConnectionPtr,
                                          const std::string& localIpAddress, const std::string& baseUrl,
                                          tcp::resolver::results_type results, size_t resultIndex) {
    auto it = results.begin();
    std::advance(it, resultIndex);
    if (it == results.end()) {
      CCAPI_LOGGER_ERROR("All connection attempts failed for localIpAddress=" + localIpAddress + ", baseUrl=" + baseUrl);
      // 延迟重试
      if (this->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
        auto timerPtr = std::make_shared<net::steady_timer>(
          *this->serviceContextPtr->ioContextPtr,
          std::chrono::milliseconds(this->sessionOptions.httpConnectionPoolReconnectDelayMilliseconds));
        timerPtr->async_wait([that = shared_from_this(), localIpAddress, baseUrl](ErrorCode const& ec) {
          if (!ec) {
            that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
          }
        });
      }
      return;
    }

    beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    if (this->sessionOptions.httpRequestTimeoutMilliseconds > 0) {
      beast::get_lowest_layer(stream).expires_after(
        std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliseconds));
    }

    CCAPI_LOGGER_TRACE("Attempting to connect to endpoint: " + it->endpoint().address().to_string() + ":" + std::to_string(it->endpoint().port()));

    beast::get_lowest_layer(stream).socket().async_connect(*it,
      [that = shared_from_this(), httpConnectionPtr, localIpAddress, baseUrl, results, resultIndex]
      (beast::error_code ec) {
        if (ec) {
          CCAPI_LOGGER_WARN("TCP connect attempt failed: " + ec.message() + ", trying next endpoint");
          // 尝试下一个endpoint
          that->asyncConnectWorkaroundForPreConnect(httpConnectionPtr, localIpAddress, baseUrl, results, resultIndex + 1);
          return;
        }

        // 连接成功,验证本地绑定地址
        beast::ssl_stream<beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
        auto localEndpoint = beast::get_lowest_layer(stream).socket().local_endpoint();
        std::string actualLocalIP = localEndpoint.address().to_string();

        CCAPI_LOGGER_INFO("TCP connected! Local IP: " + actualLocalIP + ", Expected: " + localIpAddress);

        if (!localIpAddress.empty() && actualLocalIP != localIpAddress) {
          CCAPI_LOGGER_ERROR("Local IP mismatch! Expected: " + localIpAddress + ", Got: " + actualLocalIP);
          // 关闭连接并重试
          ErrorCode closeEc;
          beast::get_lowest_layer(stream).socket().close(closeEc);

          if (that->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
            auto timerPtr = std::make_shared<net::steady_timer>(
              *that->serviceContextPtr->ioContextPtr,
              std::chrono::milliseconds(that->sessionOptions.httpConnectionPoolReconnectDelayMilliseconds));
            timerPtr->async_wait([that, localIpAddress, baseUrl](ErrorCode const& ec) {
              if (!ec) {
                that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
              }
            });
          }
          return;
        }

        // 设置TCP_NODELAY
        beast::get_lowest_layer(stream).socket().set_option(tcp::no_delay(true));

        // SSL握手
        stream.async_handshake(ssl::stream_base::client,
          [that, httpConnectionPtr, localIpAddress, baseUrl](beast::error_code ec) {
            if (ec) {
              CCAPI_LOGGER_ERROR("SSL handshake failed: " + ec.message());
              // 延迟重试
              if (that->sessionOptions.enableHttpConnectionPoolAutoReconnect) {
                auto timerPtr = std::make_shared<net::steady_timer>(
                  *that->serviceContextPtr->ioContextPtr,
                  std::chrono::milliseconds(that->sessionOptions.httpConnectionPoolReconnectDelayMilliseconds));
                timerPtr->async_wait([that, localIpAddress, baseUrl](ErrorCode const& ec) {
                  if (!ec) {
                    that->preConnectHttpConnectionPool(localIpAddress, baseUrl);
                  }
                });
              }
              return;
            }

            CCAPI_LOGGER_INFO("✓ Pre-connection fully established for localIpAddress=" + localIpAddress + ", baseUrl=" + baseUrl);

            // 更新时间戳
            auto now = UtilTime::now();
            httpConnectionPtr->lastReceiveDataTp = now;

            // 将连接放入池中
            if (that->sessionOptions.httpConnectionPoolMaxSize > 0 &&
                that->httpConnectionPool[localIpAddress][baseUrl].size() >= that->sessionOptions.httpConnectionPoolMaxSize) {
              that->httpConnectionPool[localIpAddress][baseUrl].pop_front();
            }
            that->httpConnectionPool[localIpAddress][baseUrl].push_back(httpConnectionPtr);

            // 重置重连计数
            that->httpConnectionPoolReconnectAttemptMap[localIpAddress][baseUrl] = 0;

            // 启动保活定时器
            that->startHttpConnectionPoolKeepAlive(localIpAddress, baseUrl);
          });
      });
  }
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_SERVICE_H_

