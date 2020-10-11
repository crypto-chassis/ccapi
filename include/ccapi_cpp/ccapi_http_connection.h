#ifndef INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
#include <string>
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_request.h"
#include <regex>
//#include "boost/regex.hpp"
namespace beast = boost::beast;
namespace ccapi {
class HttpConnection final {
 public:
//  enum class Status {
//    UNKNOWN,
//    BUSY,
//    IDLE
//  };
//  static std::string statusToString(Status status) {
//    std::string output;
//    switch (status) {
//      case Status::UNKNOWN:
//        output = "UNKNOWN";
//        break;
//      case Status::BUSY:
//        output = "BUSY";
//        break;
//      case Status::IDLE:
//        output = "IDLE";
//        break;
//      default:
//        CCAPI_LOGGER_FATAL("");
//    }
//    return output;
//  }
  HttpConnection(std::string host, std::string port, std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr):host(host),port(port),streamPtr(streamPtr){

  }
  std::string toString() const {
    std::ostringstream oss;
    oss << streamPtr;
    std::string output = "HttpConnection [host = " + host + ", port = " + port + ", streamPtr = " + oss.str() + "]";
    return output;
  }
//  int id;
  std::string host;
  std::string port;
//  Status status{Status::UNKNOWN};
//  Request request;
  std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr;
};
//class HttpConnectionPool final {
// public:
//  explicit HttpConnectionPool(const size_t maxSize = 0)
//        : maxSize(maxSize) {}
//  bool empty() const {
//    std::lock_guard<std::mutex> lock(this->m);
//    return this->idlePool.empty();
//  }
//  HttpConnection createConnection(std::string host, std::string port, std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr) {
//    int id = this->nextConnectionId++;
//    HttpConnection httpConnection = HttpConnection(id, host, port, streamPtr);
//    this->busyPool.emplace(id, std::move(httpConnection));
//    return this->busyPool.at(id);
//  }
// private:
//  std::vector<HttpConnection> idlePool;
//  std::map<int, HttpConnection> busyPool;
//  mutable std::mutex m;
//  size_t maxSize{};
//  int nextConnectionId{};
//}
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
