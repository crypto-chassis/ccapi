#ifndef INCLUDE_CCAPI_CPP_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
#define INCLUDE_CCAPI_CPP_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) if (!(x)) { throw std::runtime_error("rapidjson internal assertion failure"); }
#endif
#ifdef ENABLE_EXECUTION_MANAGEMENT_SERVICE
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_BINANCE_US
//#include "ccapi_cpp/ccapi_service_context.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_service_context.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_http_connection.h"
#include "ccapi_cpp/ccapi_service.h"
#include "boost/shared_ptr.hpp"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_http_retry.h"
#include "ccapi_cpp/ccapi_url.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
namespace beast = boost::beast;
// from <boost/beast.hpp>
namespace http = beast::http;
// from <boost/beast/http.hpp>
namespace net = boost::asio;
// from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;
// from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;
// from <boost/asio/ip/tcp.hpp>
namespace rj = rapidjson;
namespace ccapi {
class ExecutionManagementServiceBinanceUs final : public Service, public std::enable_shared_from_this<ExecutionManagementServiceBinanceUs> {
 public:
  ExecutionManagementServiceBinanceUs(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr),
        resolver(*serviceContextPtr->ioContextPtr),
        httpConnectionPool(sessionOptions.httpConnectionPoolMaxSize)
  {
    CCAPI_LOGGER_FUNCTION_ENTER;
    try {
      this->tcpResolverResults = this->resolver.resolve("api.binance.us", "443");
    }
    catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onError(beast::error_code ec, const std::string & what) {
    std::string errorMessage = what + ": " + ec.message();
    CCAPI_LOGGER_ERROR(errorMessage);
  }
  void onHttpResponseError(int statusCode, const std::string & errorMessage) {

  }
  http::request<http::string_body> convertRequest(const Request& request) {
    std::map<std::string, std::string> credential = request.getCredential().empty() ? this->sessionConfigs.getCredential() : request.getCredential();
//    CCAPI_LOGGER_TRACE("credential = "+toString(credential));
    std::string instrument = request.getInstrument();
    std::string symbol = instrument;
    if (!instrument.empty()) {
      if (this->sessionConfigs.getExchangeInstrumentSymbolMapRest().find("binance-us") != this->sessionConfigs.getExchangeInstrumentSymbolMapRest().end() &&
          this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at("binance-us").find(instrument) != this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at("binance-us").end()) {
        symbol = this->sessionConfigs.getExchangeInstrumentSymbolMapRest().at("binance-us").at(instrument);
      } else if (this->sessionConfigs.getExchangeInstrumentSymbolMap().find("binance-us") != this->sessionConfigs.getExchangeInstrumentSymbolMap().end() &&
          this->sessionConfigs.getExchangeInstrumentSymbolMap().at("binance-us").find(instrument) != this->sessionConfigs.getExchangeInstrumentSymbolMap().at("binance-us").end()) {
        symbol = this->sessionConfigs.getExchangeInstrumentSymbolMap().at("binance-us").at(instrument);
      }
    }
    CCAPI_LOGGER_TRACE("instrument = "+instrument);
    Request::Operation operation = request.getOperation();
    http::request < http::string_body > req;
    req.version(11);
    req.set(http::field::host, "api.binance.us");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(beast::http::field::content_type, "application/json");
    req.set("X-MBX-APIKEY", credential.at("BINANCE_US_API_KEY"));
    if (operation == Request::Operation::CREATE_ORDER) {
      // extract from req to get
      req.method(http::verb::post);
      req.target("/api/v3/order");
      std::string bodyString;
      const std::map<std::string, std::string>& paramMap = request.getParamMap();
      for (const auto& kv : paramMap) {
        std::string first = kv.first;
        std::string second = kv.second;
        if (first == CCAPI_EM_SIDE) {
          first = "side";
        } else if (first == CCAPI_EM_QUANTITY) {
          first = "quantity";
        } else if (first == CCAPI_EM_LIMIT_PRICE) {
          first = "price";
        }
        bodyString += first;
        bodyString += "=";
        bodyString += second;
        bodyString += "&";
      }
      bodyString += "symbol=";
      bodyString += symbol;
      bodyString += "&";
      if (paramMap.find("type") == paramMap.end()) {
        bodyString += "type=LIMIT&";
        if (paramMap.find("timeInForce") == paramMap.end()) {
          bodyString += "timeInForce=GTC&";
        }
      }
      if (paramMap.find("timestamp") == paramMap.end()) {
        auto now = std::chrono::system_clock::now();
        bodyString += "timestamp=";
        bodyString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
        bodyString += "&";
      }
//      if (paramMap.find("recvWindow") == paramMap.end()) {
//        bodyString += "recvWindow=";
//        bodyString += std::to_string(this->sessionOptions.httpRequestTimeoutMilliSeconds);
//        bodyString += "&";
//      }
      bodyString.pop_back();
      CCAPI_LOGGER_TRACE("bodyString = "+bodyString);
      std::string signature = UtilAlgorithm::hmacHex(credential.at(BINANCE_US_API_SECRET), bodyString);
      CCAPI_LOGGER_TRACE("signature = "+signature);
      bodyString += "&signature=";
      bodyString += signature;
      req.body() = bodyString;
      req.prepare_payload();
    }
    return req;
  }
//  std::string connectionAddressToString(const HttpConnection& httpConnection) {
//    const void * address = static_cast<const void*>(httpConnection);
//    std::stringstream ss;
//    ss << address;
//    return ss.str();
//  }
  void performRequest(const HttpConnection& httpConnection, const Request& request, const http::request < http::string_body >& req, HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("httpConnection = "+toString(httpConnection));
    CCAPI_LOGGER_DEBUG("retry = "+toString(retry));
    beast::ssl_stream <beast::tcp_stream>& stream = *httpConnection.streamPtr;
    CCAPI_LOGGER_DEBUG("this->sessionOptions.httpRequestTimeoutMilliSeconds = "+toString(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(this->sessionOptions.httpRequestTimeoutMilliSeconds));
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::get_lowest_layer(stream).async_connect(
        this->tcpResolverResults,
        [that = shared_from_this(),httpConnection,request,req,retry](beast::error_code ec, tcp::resolver::results_type::endpoint_type){
      CCAPI_LOGGER_TRACE("async_connect callback start");
          if(ec)
          return that->onError(ec, "connect");
          CCAPI_LOGGER_TRACE("connected");
//          stream.async_handshake(ssl::stream_base::client, [](beast::error_code ec){
//            CCAPI_LOGGER_TRACE("async_handshake callback start");
//          });
          beast::ssl_stream <beast::tcp_stream>& stream = *httpConnection.streamPtr;
          CCAPI_LOGGER_TRACE("before async_handshake");
          stream.async_handshake(
              ssl::stream_base::client,
              [that,httpConnection,request,req,retry](beast::error_code ec){
                CCAPI_LOGGER_TRACE("async_handshake callback start");
                if(ec)
                return that->onError(ec, "handshake");
                CCAPI_LOGGER_TRACE("handshaked");
                beast::ssl_stream <beast::tcp_stream>& stream = *httpConnection.streamPtr;
                std::shared_ptr<http::request< http::string_body >> reqPtr(new http::request < http::string_body >(std::move(req)));
                CCAPI_LOGGER_TRACE("before async_write");
                http::async_write(stream, *reqPtr,
                    [that,httpConnection,request,reqPtr,retry](beast::error_code ec,
                        std::size_t bytes_transferred){
                      CCAPI_LOGGER_TRACE("async_write callback start");
                      boost::ignore_unused(bytes_transferred);
                      if(ec)
                      return that->onError(ec, "write");
                      CCAPI_LOGGER_TRACE("written");
//                      std::cout<<"write"<<std::endl;
                      // Receive the HTTP response
                      std::shared_ptr<beast::flat_buffer> bufferPtr(new beast::flat_buffer());// (Must persist between reads)
//                      http::response<http::string_body> res();
                      std::shared_ptr<http::response< http::string_body >> resPtr(new http::response < http::string_body >());
                      beast::ssl_stream <beast::tcp_stream>& stream = *httpConnection.streamPtr;
                      CCAPI_LOGGER_TRACE("before async_read");
                      http::async_read(stream, *bufferPtr, *resPtr,
                          [that,httpConnection,request,retry,bufferPtr,resPtr](beast::error_code ec,
                              std::size_t bytes_transferred){
                            CCAPI_LOGGER_TRACE("async_read callback start");
                            auto now = std::chrono::system_clock::now();
                            boost::ignore_unused(bytes_transferred);
                            if(ec)
                            return that->onError(ec, "read");
                            if (that->sessionOptions.enableOneHttpConnectionPerRequest) {
                              beast::ssl_stream <beast::tcp_stream>& stream = *httpConnection.streamPtr;
                              CCAPI_LOGGER_TRACE("before async_shutdown");
                              stream.async_shutdown(
                                  [that,httpConnection](beast::error_code ec){
                                CCAPI_LOGGER_TRACE("async_shutdown callback start");
                                if(ec == net::error::eof)
                                {
                                    // Rationale:
                                    // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                                    ec = {};
                                }
                                if(ec)
                                    return that->onError(ec, "shutdown");
                                CCAPI_LOGGER_TRACE("shutdown");
                                // If we get here then the connection is closed gracefully
                              });
                              CCAPI_LOGGER_TRACE("after async_shutdown");
                              } else {
                                  auto thatHttpConnection = httpConnection;
                                  try {
                                    that->httpConnectionPool.pushBack(std::move(thatHttpConnection));
                                  } catch (const std::runtime_error& e) {
                                    if (e.what() != that->httpConnectionPool.EXCEPTION_QUEUE_FULL) {
                                      CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
                                    }
                                  }
                              }
  #if defined(ENABLE_DEBUG_LOG) || defined(ENABLE_TRACE_LOG)
                              std::ostringstream oss;
                              oss << *resPtr;
                              CCAPI_LOGGER_DEBUG("res = \n"+oss.str());
  #endif
  //                            std::cout<<"read"<<std::endl;
                              // Write the message to standard out
  //                            std::cout << *resPtr << std::endl;
                            try {
                              int statusCode = resPtr->result_int();
                              std::string body = resPtr->body();
                              if (statusCode / 100 == 2) {
                                rj::Document document;
                                rj::Document::AllocatorType& allocator = document.GetAllocator();
                                document.Parse(body.c_str());
                                std::string orderId = std::to_string(document["orderId"].GetInt64());
                                Event event;
                                event.setType(Event::Type::RESPONSE);
                                std::vector<Element> elementList;
                                Element element;
                                element.insert(CCAPI_EM_ORDER_ID, orderId);
                                elementList.push_back(std::move(element));
                                CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
                                std::vector<Message> messageList;
                                Message message;
                                message.setTime(TimePoint(std::chrono::milliseconds(document["transactTime"].GetInt64())));
                                message.setTimeReceived(now);
                                message.setType(Message::Type::RESPONSE_SUCCESS);
                                message.setElementList(elementList);
                                message.setCorrelationIdList({request.getCorrelationId()});
                                messageList.push_back(std::move(message));
                                event.addMessages(messageList);
                                that->eventHandler(event);
                              } else {
                                that->onHttpResponseError(statusCode, body);
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
                          });
                      CCAPI_LOGGER_TRACE("after async_read");
                    });
                CCAPI_LOGGER_TRACE("after async_write");
              });
          CCAPI_LOGGER_TRACE("after async_handshake");
        });
    CCAPI_LOGGER_TRACE("after async_connect");
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void tryRequest(const Request& request, http::request < http::string_body >& req, HttpRetry& retry) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (retry.numRetry <= this->sessionOptions.httpMaxNumRetry && retry.numRedirect <= this->sessionOptions.httpMaxNumRedirect) {
      if (!retry.redirectUrlStr.empty()) {
        Url url(retry.redirectUrlStr);
        std::string host(url.host);
        if (!url.port.empty()) {
          host += ":";
          host += url.port;
        }
        req.set(http::field::host, host);
        req.target(url.target);
      }

      if (this->sessionOptions.enableOneHttpConnectionPerRequest || this->httpConnectionPool.empty()) {

//        httpConnection.id = this->connectionAddressToString(httpConnection);
        std::string host = "api.binance.us";
        std::string port = "443";
//        httpConnection.request = request;
        std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(new beast::ssl_stream <beast::tcp_stream>(*this->serviceContextPtr->ioContextPtr, *this->serviceContextPtr->sslContextPtr));
//        beast::ssl_stream <beast::tcp_stream> stream();
//        httpConnection.streamPtr = std::make_shared(stream);
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), "api.binance.us")) {
          beast::error_code ec { static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
          std::cerr << ec.message() << "\n";
          return;
        }

        HttpConnection httpConnection(host, port, streamPtr);
        this->performRequest(httpConnection, request, req, retry);
      } else {
//        boost::asio::post(this->strandPtr.get(), []()->{
        std::unique_ptr<HttpConnection> httpConnectionPtr(nullptr);
        try {
          HttpConnection httpConnection = this->httpConnectionPool.popBack();
          httpConnectionPtr.reset(&httpConnection);
//          httpConnection = std::make_shared<HttpConnection>(httpConnection);
        } catch (const std::runtime_error& e) {
          if (e.what() != this->httpConnectionPool.EXCEPTION_QUEUE_EMPTY) {
            CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
          }
          std::string host = "api.binance.us";
          std::string port = "443";
//          httpConnection.request = request;
          std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr(new beast::ssl_stream <beast::tcp_stream>(*this->serviceContextPtr->ioContextPtr, *this->serviceContextPtr->sslContextPtr));
//          httpConnection.streamPtr = std::make_shared(stream);
          // Set SNI Hostname (many hosts need this to handshake successfully)
          if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), "api.binance.us")) {
            beast::error_code ec { static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
            std::cerr << ec.message() << "\n";
            return;
          }
          httpConnectionPtr = std::make_unique<HttpConnection>(host, port, streamPtr);
        }
//          HttpConnection httpConnection = std::move(this->httpConnectionPool.back());

//          HttpConnection httpConnection = this->httpConnectionPool.popBack()
          HttpConnection httpConnection = *httpConnectionPtr;
          this->performRequest(httpConnection, request, req, retry);
//        });
      }
    } else {
      // error event: exceed max retry or redirect
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  // A successful request will generate zero or more PARTIAL_RESPONSE
          // Messages followed by exactly one RESPONSE Message. Once the final
          // RESPONSE Message has been received the correlation ID associated
          // with this request may be re-used. If the request fails at any stage
          // a REQUEST_STATUS will be generated after which the correlation ID
          // associated with the request may be re-used.
  void sendRequest(const Request& request, bool block) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("request = "+toString(request));
    CCAPI_LOGGER_DEBUG("block = "+toString(block));
    http::request < http::string_body > req = this->convertRequest(request);
#if defined(ENABLE_DEBUG_LOG) || defined(ENABLE_TRACE_LOG)
    std::ostringstream oss;
    oss << req;
//    std::string reqStr = oss.str();
//    reqStr.erase(std::unique(reqStr.begin(), reqStr.end(),
//                          [] (char a, char b) {return a == '\n' && b == '\n';}),
//                 reqStr.end());
    CCAPI_LOGGER_DEBUG("req = \n"+oss.str());
#endif
    std::promise<void>* promisePtrRaw = nullptr;
    if (block) {
      promisePtrRaw = new std::promise<void>();
    }
    std::shared_ptr<std::promise<void> > promisePtr(promisePtrRaw);
    HttpRetry retry(0,0,"",promisePtr);
    this->tryRequest(request, req, retry);
    if (block) {
      std::future<void> future = promisePtr->get_future();
      CCAPI_LOGGER_TRACE("before future wait");
      future.wait();
      CCAPI_LOGGER_TRACE("after future wait");
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
}
 private:
  tcp::resolver resolver;
  tcp::resolver::results_type tcpResolverResults;
  Queue<HttpConnection> httpConnectionPool;

};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_US_H_
