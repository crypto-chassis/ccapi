#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "ccapi_cpp/service/ccapi_service.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_request.h"
#include "boost/shared_ptr.hpp"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_http_retry.h"
#include "ccapi_cpp/ccapi_url.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_hmac.h"
namespace ccapi {
class ExecutionManagementService : public Service {
 public:
  enum class JsonDataType {
    STRING,
    INTEGER,
    BOOLEAN
  };
  ExecutionManagementService(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
  : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr),
//  resolver(*serviceContextPtr->ioContextPtr),
  httpConnectionPool(sessionOptions.httpConnectionPoolMaxSize) {
  }
  virtual ~ExecutionManagementService() {
  }
  std::shared_ptr<std::future<void> > sendRequest(const Request& request, const bool useFuture, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("request = "+toString(request));
    CCAPI_LOGGER_DEBUG("useFuture = "+toString(useFuture));
    auto req = this->convertRequest(request, now);
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
    std::ostringstream oss;
    oss << req;
    CCAPI_LOGGER_DEBUG("req = \n"+oss.str());
#endif
    std::promise<void>* promisePtrRaw = nullptr;
    if (useFuture) {
      promisePtrRaw = new std::promise<void>();
    }
    std::shared_ptr<std::promise<void> > promisePtr(promisePtrRaw);
    HttpRetry retry(0, 0, "", promisePtr);
    this->tryRequest(request, req, retry);
    std::shared_ptr<std::future<void> > futurePtr(nullptr);
    if (useFuture) {
      futurePtr = std::make_shared<std::future<void> >(std::move(promisePtr->get_future()));
    }
    return futurePtr;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void stop() override {}
  void subscribe(const std::vector<Subscription>& subscriptionList) override {}

 protected:
  void setHostFromUrl(std::string baseUrlRest) {
    auto hostPort = this->extractHostFromUrl(baseUrlRest);
    this->host = hostPort.first;
    this->port = hostPort.second;
  }
  std::string convertInstrumentToRestSymbolId(std::string instrument) {
    std::string symbolId = instrument;
    if (!instrument.empty()) {
      if (this->sessionConfigs.getExchangeInstrumentSymbolMapRest().find(this->name) != this->sessionConfigs.getExchangeInstrumentSymbolMapRest().end() &&
          this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at(this->name).find(instrument) != this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at(this->name).end()) {
        symbolId = this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at(this->name).at(instrument);
      } else if (this->sessionConfigs.getExchangeInstrumentSymbolMap().find(this->name) != this->sessionConfigs.getExchangeInstrumentSymbolMap().end() &&
          this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).find(instrument) != this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).end()) {
        symbolId = this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).at(instrument);
      }
    }
    return symbolId;
  }
  std::string convertRestSymbolIdToInstrument(std::string symbolId) {
    std::string instrument = symbolId;
    if (!symbolId.empty()) {
      if (this->sessionConfigs.getExchangeSymbolInstrumentMapRest().find(this->name) != this->sessionConfigs.getExchangeSymbolInstrumentMapRest().end() &&
          this->sessionConfigs.getExchangeSymbolInstrumentMapRest().at(this->name).find(symbolId) != this->sessionConfigs.getExchangeSymbolInstrumentMapRest().at(this->name).end()) {
        instrument = this->sessionConfigs.getExchangeSymbolInstrumentMapRest().at(this->name).at(symbolId);
      } else if (this->sessionConfigs.getExchangeSymbolInstrumentMap().find(this->name) != this->sessionConfigs.getExchangeSymbolInstrumentMap().end() &&
          this->sessionConfigs.getExchangeSymbolInstrumentMap().at(this->name).find(symbolId) != this->sessionConfigs.getExchangeSymbolInstrumentMap().at(this->name).end()) {
        instrument = this->sessionConfigs.getExchangeSymbolInstrumentMap().at(this->name).at(symbolId);
      }
    }
    return instrument;
  }
  void performRequest(std::shared_ptr<HttpConnection> httpConnectionPtr, const Request& request, http::request<http::string_body>& req, const HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("httpConnection = "+toString(*httpConnectionPtr));
    CCAPI_LOGGER_DEBUG("retry = "+toString(retry));
    try {
      std::call_once(tcpResolverResultsFlag, [that = shared_from_base<ExecutionManagementService>()]() {
        that->tcpResolverResults = that->resolver.resolve(that->host, that->port);
      });
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_DEBUG("this->sessionOptions.httpRequestTimeoutMilliSeconds = "+toString(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
      this->tcpResolverResults,
      beast::bind_front_handler(&ExecutionManagementService::onConnect, shared_from_base<ExecutionManagementService>(), httpConnectionPtr, request, req, retry));
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "connect");
      return;
    }
    CCAPI_LOGGER_TRACE("connected");
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(
      ssl::stream_base::client,
      beast::bind_front_handler(&ExecutionManagementService::onHandshake, shared_from_base<ExecutionManagementService>(), httpConnectionPtr, request, req, retry));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "handshake");
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    std::shared_ptr<http::request<http::string_body> > reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr, beast::bind_front_handler(&ExecutionManagementService::onWrite, shared_from_base<ExecutionManagementService>(), httpConnectionPtr, request, reqPtr, retry));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body> > reqPtr, HttpRetry retry, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_write callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "write");
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry);
      return;
    }
    CCAPI_LOGGER_TRACE("written");
    std::shared_ptr<beast::flat_buffer> bufferPtr(new beast::flat_buffer());
    std::shared_ptr<http::response<http::string_body> > resPtr(new http::response < http::string_body >());
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    http::async_read(stream, *bufferPtr, *resPtr, beast::bind_front_handler(&ExecutionManagementService::onRead, shared_from_base<ExecutionManagementService>(), httpConnectionPtr, request, reqPtr, retry, bufferPtr, resPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body> > reqPtr, HttpRetry retry, std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body> > resPtr, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    auto now = UtilTime::now();
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "read");
      auto now = UtilTime::now();
      auto req = this->convertRequest(request, now);
      retry.numRetry += 1;
      this->tryRequest(request, req, retry);
      return;
    }
    if (this->sessionOptions.enableOneHttpConnectionPerRequest) {
      beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
      CCAPI_LOGGER_TRACE("before async_shutdown");
      stream.async_shutdown(
        beast::bind_front_handler(
          &ExecutionManagementService::onShutdown,
          shared_from_base<ExecutionManagementService>(), httpConnectionPtr));
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
    CCAPI_LOGGER_DEBUG("res = \n"+oss.str());
#endif
    int statusCode = resPtr->result_int();
    std::string body = resPtr->body();
    try {
      if (statusCode / 100 == 2) {
        if ((this->name == CCAPI_EXCHANGE_NAME_HUOBI && body.find("err-code") != std::string::npos) ||
            (this->name == CCAPI_EXCHANGE_NAME_ERISX && (body.find("\"ordStatus\":\"REJECTED\"") != std::string::npos || body.find("\"message\":\"Rejected with reason NO RESTING ORDERS\"") != std::string::npos))) {
          this->onResponseError(400, body);
        } else {
          Event event;
          event.setType(Event::Type::RESPONSE);
          std::vector<Message> messageList = std::move(this->processSuccessfulTextMessage(request, body, now));
          event.addMessages(messageList);
          this->eventHandler(event);
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
          this->onResponseError(statusCode, body);
        }
      } else if (statusCode / 100 == 4) {
        this->onResponseError(statusCode, body);
      } else if (statusCode / 100 == 5) {
        this->onResponseError(statusCode, body);
        retry.numRetry += 1;
        this->tryRequest(request, *reqPtr, retry);
        return;
      } else {
        this->onResponseError(statusCode, "unhandled response");
      }
    }
    catch (const std::exception& e) {
      CCAPI_LOGGER_ERROR(e.what());
      std::ostringstream oss;
      oss << *resPtr;
      CCAPI_LOGGER_ERROR("res = " + oss.str());
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, e);
    }
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    if (retry.promisePtr) {
      retry.promisePtr->set_value();
    }
  }
  void onShutdown(std::shared_ptr<HttpConnection> httpConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_shutdown callback start");
    if (ec == net::error::eof) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
    }
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
      return;
    }
    CCAPI_LOGGER_TRACE("shutdown");
    // If we get here then the connection is closed gracefully
  }
  void tryRequest(const Request& request, http::request<http::string_body>& req, const HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("retry = " + toString(retry));
    if (retry.numRetry <= this->sessionOptions.httpMaxNumRetry && retry.numRedirect <= this->sessionOptions.httpMaxNumRedirect) {
      try {
        if (this->sessionOptions.enableOneHttpConnectionPerRequest || this->httpConnectionPool.empty()) {
          std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(nullptr);
          try {
            streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->host);
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream");
            return;
          }
          std::shared_ptr<HttpConnection> httpConnectionPtr(new HttpConnection(this->host, this->port, streamPtr));
          this->performRequest(httpConnectionPtr, request, req, retry);
        } else {
          std::shared_ptr<HttpConnection> httpConnectionPtr(nullptr);
          try {
            httpConnectionPtr = std::move(this->httpConnectionPool.popBack());
            this->onHandshake(httpConnectionPtr, request, req, retry, {});
          } catch (const std::runtime_error& e) {
            if (e.what() != this->httpConnectionPool.EXCEPTION_QUEUE_EMPTY) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
            std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(nullptr);
            try {
              streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->host);
            } catch (const beast::error_code& ec) {
              CCAPI_LOGGER_TRACE("fail");
              this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "create stream");
              return;
            }
            httpConnectionPtr = std::make_shared<HttpConnection>(this->host, this->port, streamPtr);
            this->performRequest(httpConnectionPtr, request, req, retry);
          }
        }
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, e);
      }
    } else {
      std::string errorMessage = this->sessionOptions.httpMaxNumRetry ? "max retry exceeded" : "max redirect exceeded";
      CCAPI_LOGGER_ERROR(errorMessage);
      CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, std::runtime_error(errorMessage));
      if (retry.promisePtr) {
        retry.promisePtr->set_value();
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
//  std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr, const std::string& host) {
//    std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(new beast::ssl_stream <beast::tcp_stream>(*iocPtr, *ctxPtr));
//    // Set SNI Hostname (many hosts need this to handshake successfully)
//    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
//      beast::error_code ec {static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
//      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
//      throw ec;
//    }
//    return streamPtr;
//  }
  http::request<http::string_body> convertRequest(const Request& request, const TimePoint& now) {
    auto credential = request.getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto instrument = request.getInstrument();
    auto symbolId = this->convertInstrumentToRestSymbolId(instrument);
    CCAPI_LOGGER_TRACE("instrument = "+instrument);
    auto operation = request.getOperation();
    http::request<http::string_body> req;
    req.set(http::field::host, this->host+":"+this->port);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    this->convertReq(request, now, req, credential, symbolId, operation);
    return req;
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
  std::string convertOrderStatus(const std::string& status) {
    return this->orderStatusOpenSet.find(status) != this->orderStatusOpenSet.end() ? CCAPI_EM_ORDER_STATUS_OPEN
    : CCAPI_EM_ORDER_STATUS_CLOSED;
  }
  virtual std::vector<Message> processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_DEBUG("textMessage = " + textMessage);
    rj::Document document;
    document.Parse(textMessage.c_str());
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({request.getCorrelationId()});
    std::vector<Element> elementList;
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        message.setType(Message::Type::CREATE_ORDER);
      }
      break;
      case Request::Operation::CANCEL_ORDER:
      {
        message.setType(Message::Type::CANCEL_ORDER);
      }
      break;
      case Request::Operation::GET_ORDER:
      {
        message.setType(Message::Type::GET_ORDER);
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        message.setType(Message::Type::GET_OPEN_ORDERS);
      }
      break;
      case Request::Operation::CANCEL_OPEN_ORDERS:
      {
        message.setType(Message::Type::CANCEL_OPEN_ORDERS);
      }
      break;
      default:
      CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    message.setElementList(this->extractOrderInfo(operation, document));
    std::vector<Message> messageList;
    messageList.push_back(std::move(message));
    return messageList;
  }
  virtual Element extractOrderInfo(const rj::Value& x, const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) {
    Element element;
    for (const auto& y : extractionFieldNameMap) {
      auto it = x.FindMember(y.second.first.c_str());
      if (it != x.MemberEnd()) {
        std::string value = y.second.second == JsonDataType::STRING ? it->value.GetString() :
          y.second.second == JsonDataType::INTEGER ? std::to_string(it->value.GetInt64()) :
          y.second.second == JsonDataType::BOOLEAN ? std::to_string(static_cast<int>(it->value.GetBool())) :
          "null";
        if (y.first == CCAPI_EM_ORDER_INSTRUMENT) {
          value = this->convertRestSymbolIdToInstrument(value);
        } else if (y.first == CCAPI_EM_ORDER_STATUS) {
          value = this->convertOrderStatus(value);
        } else if (y.first == CCAPI_EM_ORDER_SIDE) {
          value = UtilString::toLower(value).rfind("buy", 0) == 0 ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL;
        }
        element.insert(y.first, value);
      }
    }
    return element;
  }
  virtual void convertReq(const Request& request, const TimePoint& now, http::request<http::string_body>& req, const std::map<std::string, std::string>& credential, const std::string& symbolId, const Request::Operation operation) = 0;
  virtual std::vector<Element> extractOrderInfo(const Request::Operation operation, const rj::Document& document) = 0;
  std::string host;
  std::string port;
//  tcp::resolver resolver;
  tcp::resolver::results_type tcpResolverResults;
  std::once_flag tcpResolverResultsFlag;
  Queue<std::shared_ptr<HttpConnection> > httpConnectionPool;
  std::string apiKeyName;
  std::string apiSecretName;
  std::map<std::string, std::string> credentialDefault;
  std::string createOrderTarget;
  std::string cancelOrderTarget;
  std::string getOrderTarget;
  std::string getOpenOrdersTarget;
  std::string cancelOpenOrdersTarget;
  std::set<std::string> orderStatusOpenSet;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_H_
