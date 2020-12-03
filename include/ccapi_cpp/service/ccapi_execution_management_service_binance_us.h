#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) if (!(x)) { throw std::runtime_error("rapidjson internal assertion failure"); }
#endif
#ifdef ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef ENABLE_EXCHANGE_BINANCE_US
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_http_connection.h"
#include "ccapi_cpp/service/ccapi_service.h"
#include "boost/shared_ptr.hpp"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_http_retry.h"
#include "ccapi_cpp/ccapi_url.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#ifndef CCAPI_BINANCE_US_CREATE_ORDER_TARGET
#define CCAPI_BINANCE_US_CREATE_ORDER_TARGET "/api/v3/order"
#endif
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
namespace rj = rapidjson;
namespace ccapi {
class ExecutionManagementServiceBinanceUs final : public Service, public std::enable_shared_from_this<ExecutionManagementServiceBinanceUs> {
 public:
  ExecutionManagementServiceBinanceUs(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr),
        resolver(*serviceContextPtr->ioContextPtr),
        httpConnectionPool(sessionOptions.httpConnectionPoolMaxSize) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->name = CCAPI_EXCHANGE_NAME_BINANCE_US;
    CCAPI_LOGGER_TRACE(this->name);
    CCAPI_LOGGER_TRACE(toString(sessionConfigs.getUrlRestBase()));
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->name);
    auto splitted1 = UtilString::split(baseUrlRest, "://");
    auto splitted2 = UtilString::split(UtilString::split(splitted1[1], "/")[0], ":");
    this->host = splitted2[0];
    if (splitted1[0] == "https") {
      this->port = "443";
    } else {
      this->port = "80";
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void stop() override {}
  void subscribe(const std::vector<Subscription>& subscriptionList) override {}
  void onFailure(beast::error_code ec, const std::string & what) {
    std::string errorMessage = what + ": " + ec.message() + ", category: " + ec.category().name();
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    Event event;
    event.setType(Event::Type::REQUEST_STATUS);
    Message message;
    auto now = std::chrono::system_clock::now();
    message.setTimeReceived(now);
    message.setType(Message::Type::REQUEST_FAILURE);
    Element element;
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({ element });
    event.setMessageList({ message });
    this->eventHandler(event);
  }
  void onResponseError(int statusCode, const std::string & errorMessage) {
    std::string statusCodeStr = std::to_string(statusCode);
    CCAPI_LOGGER_ERROR("statusCode = " + statusCodeStr + ", errorMessage = " + errorMessage);
    Event event;
    event.setType(Event::Type::REQUEST_STATUS);
    Message message;
    auto now = std::chrono::system_clock::now();
    message.setTimeReceived(now);
    message.setType(Message::Type::RESPONSE_ERROR);
    Element element;
    element.insert(CCAPI_HTTP_STATUS_CODE, statusCodeStr);
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({ element });
    event.setMessageList({ message });
    this->eventHandler(event);
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
  http::request<http::string_body> convertRequest(const Request& request, const TimePoint& now) {
    auto credential = request.getCredential().empty() ? this->sessionConfigs.getCredential() : request.getCredential();
    auto instrument = request.getInstrument();
    auto symbolId = this->convertInstrumentToRestSymbolId(instrument);
    CCAPI_LOGGER_TRACE("instrument = "+instrument);
    auto operation = request.getOperation();
    http::request<http::string_body> req;
//    req.version(11);
    req.set(http::field::host, this->host+":"+this->port);
//    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
//    req.set(beast::http::field::content_type, "application/json");
    req.set("X-MBX-APIKEY", credential.at("BINANCE_US_API_KEY"));
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        req.method(http::verb::post);
        req.target(CCAPI_BINANCE_US_CREATE_ORDER_TARGET);
        std::string bodyString;
        const std::map<std::string, std::string>& paramMap = request.getParamMap();
        for (const auto& kv : paramMap) {
          std::string first = kv.first;
          std::string second = kv.second;
          if (first == CCAPI_EM_ORDER_SIDE) {
            first = "side";
          } else if (first == CCAPI_EM_ORDER_QUANTITY) {
            first = "quantity";
          } else if (first == CCAPI_EM_ORDER_LIMIT_PRICE) {
            first = "price";
          } else if (first == CCAPI_EM_CLIENT_ORDER_ID) {
            first = "newClientOrderId";
          }
          bodyString += first;
          bodyString += "=";
          bodyString += second;
          bodyString += "&";
        }
        if (!symbolId.empty()) {
          bodyString += "symbol=";
          bodyString += symbolId;
          bodyString += "&";
        }
        if (paramMap.find("type") == paramMap.end()) {
          bodyString += "type=LIMIT&";
          if (paramMap.find("timeInForce") == paramMap.end()) {
            bodyString += "timeInForce=GTC&";
          }
        }
        if (paramMap.find("timestamp") == paramMap.end()) {
          bodyString += "timestamp=";
          bodyString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
          bodyString += "&";
        }
        bodyString.pop_back();
        CCAPI_LOGGER_TRACE("bodyString = "+bodyString);
        std::string signature = UtilAlgorithm::hmacHex(credential.at(CCAPI_BINANCE_US_API_SECRET), bodyString);
        CCAPI_LOGGER_TRACE("signature = "+signature);
        bodyString += "&signature=";
        bodyString += signature;
        req.body() = bodyString;
        req.prepare_payload();
      }
      break;
      case Request::Operation::CANCEL_ORDER:
      {
        req.method(http::verb::delete_);
        req.target("/api/v3/order");
        std::string bodyString;
        const std::map<std::string, std::string>& paramMap = request.getParamMap();
        for (const auto& kv : paramMap) {
          std::string first = kv.first;
          std::string second = kv.second;
          if (first == CCAPI_EM_ORDER_ID) {
            first = "orderId";
          } else if (first == CCAPI_EM_CLIENT_ORDER_ID) {
            first = "origClientOrderId";
          }
          bodyString += first;
          bodyString += "=";
          bodyString += second;
          bodyString += "&";
        }
        if (!symbolId.empty()) {
          bodyString += "symbol=";
          bodyString += symbolId;
          bodyString += "&";
        }
        if (paramMap.find("timestamp") == paramMap.end()) {
          bodyString += "timestamp=";
          bodyString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
          bodyString += "&";
        }
        bodyString.pop_back();
        CCAPI_LOGGER_TRACE("bodyString = "+bodyString);
        std::string signature = UtilAlgorithm::hmacHex(credential.at(CCAPI_BINANCE_US_API_SECRET), bodyString);
        CCAPI_LOGGER_TRACE("signature = "+signature);
        bodyString += "&signature=";
        bodyString += signature;
        req.body() = bodyString;
        req.prepare_payload();
      }
      break;
      case Request::Operation::GET_ORDER:
      {
        req.method(http::verb::get);
        req.target("/api/v3/order");
        std::string bodyString;
        const std::map<std::string, std::string>& paramMap = request.getParamMap();
        for (const auto& kv : paramMap) {
          std::string first = kv.first;
          std::string second = kv.second;
          if (first == CCAPI_EM_ORDER_ID) {
            first = "orderId";
          } else if (first == CCAPI_EM_CLIENT_ORDER_ID) {
            first = "origClientOrderId";
          }
          bodyString += first;
          bodyString += "=";
          bodyString += second;
          bodyString += "&";
        }
        if (!symbolId.empty()) {
          bodyString += "symbol=";
          bodyString += symbolId;
          bodyString += "&";
        }
        if (paramMap.find("timestamp") == paramMap.end()) {
          bodyString += "timestamp=";
          bodyString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
          bodyString += "&";
        }
        bodyString.pop_back();
        CCAPI_LOGGER_TRACE("bodyString = "+bodyString);
        std::string signature = UtilAlgorithm::hmacHex(credential.at(CCAPI_BINANCE_US_API_SECRET), bodyString);
        CCAPI_LOGGER_TRACE("signature = "+signature);
        bodyString += "&signature=";
        bodyString += signature;
        req.body() = bodyString;
        req.prepare_payload();
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        req.method(http::verb::get);
        req.target("/api/v3/openOrders");
        std::string bodyString;
        const std::map<std::string, std::string>& paramMap = request.getParamMap();
        for (const auto& kv : paramMap) {
          std::string first = kv.first;
          std::string second = kv.second;
          bodyString += first;
          bodyString += "=";
          bodyString += second;
          bodyString += "&";
        }
        if (!symbolId.empty()) {
          bodyString += "symbol=";
          bodyString += symbolId;
          bodyString += "&";
        }
        if (paramMap.find("timestamp") == paramMap.end()) {
          bodyString += "timestamp=";
          bodyString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
          bodyString += "&";
        }
        bodyString.pop_back();
        CCAPI_LOGGER_TRACE("bodyString = "+bodyString);
        std::string signature = UtilAlgorithm::hmacHex(credential.at(CCAPI_BINANCE_US_API_SECRET), bodyString);
        CCAPI_LOGGER_TRACE("signature = "+signature);
        bodyString += "&signature=";
        bodyString += signature;
        req.body() = bodyString;
        req.prepare_payload();
      }
      break;
      case Request::Operation::CANCEL_OPEN_ORDERS:
      {
        req.method(http::verb::delete_);
        req.target("/api/v3/openOrders");
        std::string bodyString;
        const std::map<std::string, std::string>& paramMap = request.getParamMap();
        for (const auto& kv : paramMap) {
          std::string first = kv.first;
          std::string second = kv.second;
          bodyString += first;
          bodyString += "=";
          bodyString += second;
          bodyString += "&";
        }
        if (!symbolId.empty()) {
          bodyString += "symbol=";
          bodyString += symbolId;
          bodyString += "&";
        }
        if (paramMap.find("timestamp") == paramMap.end()) {
          bodyString += "timestamp=";
          bodyString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
          bodyString += "&";
        }
        bodyString.pop_back();
        CCAPI_LOGGER_TRACE("bodyString = "+bodyString);
        std::string signature = UtilAlgorithm::hmacHex(credential.at(CCAPI_BINANCE_US_API_SECRET), bodyString);
        CCAPI_LOGGER_TRACE("signature = "+signature);
        bodyString += "&signature=";
        bodyString += signature;
        req.body() = bodyString;
        req.prepare_payload();
      }
      break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return req;
  }
  void performRequest(std::shared_ptr<HttpConnection> httpConnectionPtr, const Request& request, http::request<http::string_body>& req, const HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("httpConnection = "+toString(*httpConnectionPtr));
    CCAPI_LOGGER_DEBUG("retry = "+toString(retry));
    try {
      std::call_once(tcpResolverResultsFlag, [that = shared_from_this()](){
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
        beast::bind_front_handler(&ExecutionManagementServiceBinanceUs::onConnect, shared_from_this(), httpConnectionPtr, request, req, retry));
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onConnect(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
        if (ec) {
          CCAPI_LOGGER_TRACE("fail");
          this->onFailure(ec, "connect");
          return;
        }
        CCAPI_LOGGER_TRACE("connected");
        beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
        CCAPI_LOGGER_TRACE("before async_handshake");
        stream.async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(&ExecutionManagementServiceBinanceUs::onHandshake, shared_from_this(), httpConnectionPtr, request, req, retry));
        CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, http::request<http::string_body> req, HttpRetry retry, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFailure(ec, "handshake");
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnectionPtr->streamPtr;
    std::shared_ptr<http::request<http::string_body> > reqPtr(new http::request<http::string_body>(std::move(req)));
    CCAPI_LOGGER_TRACE("before async_write");
    http::async_write(stream, *reqPtr, beast::bind_front_handler(&ExecutionManagementServiceBinanceUs::onWrite, shared_from_this(), httpConnectionPtr, request, reqPtr, retry));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body> > reqPtr, HttpRetry retry, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_write callback start");
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFailure(ec, "write");
      auto now = std::chrono::system_clock::now();
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
    http::async_read(stream, *bufferPtr, *resPtr, beast::bind_front_handler(&ExecutionManagementServiceBinanceUs::onRead, shared_from_this(), httpConnectionPtr, request, reqPtr, retry, bufferPtr, resPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  std::vector<Message> processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) {
    rj::Document document;
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.Parse(textMessage.c_str());
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({request.getCorrelationId()});
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        message.setType(Message::Type::CREATE_ORDER);
        std::vector<Element> elementList;
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, std::to_string(document["orderId"].GetInt64()));
        element.insert(CCAPI_EM_CLIENT_ORDER_ID, document["clientOrderId"].GetString());
        elementList.push_back(std::move(element));
        message.setElementList(elementList);
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
        std::vector<Element> elementList;
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, std::to_string(document["orderId"].GetInt64()));
        element.insert(CCAPI_EM_CLIENT_ORDER_ID, document["clientOrderId"].GetString());
        element.insert(CCAPI_EM_ORDER_SIDE, document["side"].GetString());
        element.insert(CCAPI_EM_ORDER_QUANTITY, document["origQty"].GetString());
        if (document.HasMember("price")) {
          element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, document["price"].GetString());
        }
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, document["executedQty"].GetString());
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, document["cummulativeQuoteQty"].GetString());
        element.insert(CCAPI_EM_ORDER_STATUS, this->convertOrderStatus(document["status"].GetString()));
        elementList.push_back(std::move(element));
        message.setElementList(elementList);
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        message.setType(Message::Type::GET_OPEN_ORDERS);
        std::vector<Element> elementList;
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ORDER_ID, std::to_string(x["orderId"].GetInt64()));
          element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["clientOrderId"].GetString());
          element.insert(CCAPI_EM_ORDER_SIDE, x["side"].GetString());
          element.insert(CCAPI_EM_ORDER_QUANTITY, x["origQty"].GetString());
          if (x.HasMember("price")) {
            element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, x["price"].GetString());
          }
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, x["executedQty"].GetString());
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, x["cummulativeQuoteQty"].GetString());
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, this->convertRestSymbolIdToInstrument(x["symbol"].GetString()));
          elementList.push_back(std::move(element));
        }
        message.setElementList(elementList);
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
    std::vector<Message> messageList;
    messageList.push_back(std::move(message));
    return messageList;
  }
    std::string convertOrderStatus(std::string status) {
      return this->orderStatusOpenSet.find(status) != this->orderStatusOpenSet.end() ? CCAPI_EM_ORDER_STATUS_OPEN
      : this->orderStatusClosedSet.find(status) != this->orderStatusClosedSet.end() ? CCAPI_EM_ORDER_STATUS_CLOSED
      : CCAPI_EM_ORDER_STATUS_UNKNOWN;
    }
  void onRead(std::shared_ptr<HttpConnection> httpConnectionPtr, Request request, std::shared_ptr<http::request<http::string_body> > reqPtr, HttpRetry retry, std::shared_ptr<beast::flat_buffer> bufferPtr, std::shared_ptr<http::response<http::string_body> > resPtr, beast::error_code ec, std::size_t bytes_transferred) {
    CCAPI_LOGGER_TRACE("async_read callback start");
    auto now = std::chrono::system_clock::now();
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFailure(ec, "read");
      auto now = std::chrono::system_clock::now();
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
                          &ExecutionManagementServiceBinanceUs::onShutdown,
                          shared_from_this()));
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
#if defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
      std::ostringstream oss;
      oss << *resPtr;
      CCAPI_LOGGER_DEBUG("res = \n"+oss.str());
#endif
    int statusCode = resPtr->result_int();
    std::string body = resPtr->body();
    try {
      if (statusCode / 100 == 2) {
        Event event;
        event.setType(Event::Type::RESPONSE);
        std::vector<Message> messageList = std::move(this->processSuccessfulTextMessage(request, body, now));
        event.addMessages(messageList);
        this->eventHandler(event);
      } else if (statusCode / 100 == 3) {
        if (resPtr->base().find("Location") != resPtr->base().end()) {
          Url url(resPtr->base().at("Location").to_string());
          std::string host(url.host);
          if (!url.port.empty()) {
            host += ":";
            host += url.port;
          }
          auto now = std::chrono::system_clock::now();
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
    }
    CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
    if (retry.promisePtr) {
      retry.promisePtr->set_value();
    }
  }
  void onShutdown(beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_shutdown callback start");
    if (ec == net::error::eof) {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
    }
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFailure(ec, "shutdown");
      return;
    }
    CCAPI_LOGGER_TRACE("shutdown");
    // If we get here then the connection is closed gracefully
  }
  void tryRequest(const Request& request, http::request<http::string_body>& req, const HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("retry = " + toString(retry));
    if (retry.numRetry <= this->sessionOptions.httpMaxNumRetry && retry.numRedirect <= this->sessionOptions.httpMaxNumRedirect) {
      if (this->sessionOptions.enableOneHttpConnectionPerRequest || this->httpConnectionPool.empty()) {
        std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(nullptr);
        try {
          streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->host, this->port);
        } catch (const beast::error_code& ec) {
          CCAPI_LOGGER_TRACE("fail");
          this->onFailure(ec, "create stream");
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
            streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->host, this->port);
          } catch (const beast::error_code& ec) {
            CCAPI_LOGGER_TRACE("fail");
            this->onFailure(ec, "create stream");
            return;
          }
          httpConnectionPtr = std::make_shared<HttpConnection>(this->host, this->port, streamPtr);
          this->performRequest(httpConnectionPtr, request, req, retry);
        }
      }
    } else {
      CCAPI_LOGGER_ERROR(this->sessionOptions.httpMaxNumRetry ? "max retry exceeded" : "max redirect exceeded");
      CCAPI_LOGGER_DEBUG("retry = " + toString(retry));
      if (retry.promisePtr) {
        retry.promisePtr->set_value();
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > createStream(std::shared_ptr<net::io_context> iocPtr, std::shared_ptr<net::ssl::context> ctxPtr, const std::string& host, const std::string& port) {
    std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(new beast::ssl_stream <beast::tcp_stream>(*iocPtr, *ctxPtr));
    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), (host+":"+port).c_str())) {
      beast::error_code ec { static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
      CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
      throw ec;
    }
    return streamPtr;
  }
  std::shared_ptr<std::future<void> > sendRequest(const Request& request, const bool useFuture, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("request = "+toString(request));
    CCAPI_LOGGER_DEBUG("useFuture = "+toString(useFuture));
    auto req = this->convertRequest(request, now);
#if defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
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
    std::shared_ptr<std::future<void> >  futurePtr(nullptr);
    if (useFuture) {
      futurePtr = std::make_shared<std::future<void> >(std::move(promisePtr->get_future()));
    }
    return futurePtr;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

 private:
  std::string host;
  std::string port;
  tcp::resolver resolver;
  tcp::resolver::results_type tcpResolverResults;
  std::once_flag tcpResolverResultsFlag;
  Queue<std::shared_ptr<HttpConnection> > httpConnectionPool;

  std::set<std::string> orderStatusOpenSet = { "NEW", "PARTIALLY_FILLED" };
  std::set<std::string> orderStatusClosedSet = { "FILLED", "CANCELED", "REJECTED", "EXPIRED" };
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
