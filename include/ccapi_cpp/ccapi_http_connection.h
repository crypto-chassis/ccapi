#ifndef INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
#include <string>

#include "ccapi_cpp/ccapi_logger.h"
namespace beast = boost::beast;

namespace ccapi {

class HttpConnection {
  /**
   * This class represents a TCP socket connection for the REST API.
   */
 public:
  HttpConnection(const HttpConnection&) = delete;
  HttpConnection& operator=(const HttpConnection&) = delete;

  HttpConnection(std::string host, std::string port, std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr)
      : host(host), port(port), streamPtr(streamPtr) {}

  std::string toString() const {
    std::ostringstream oss;
    oss << streamPtr;
    std::string output = "HttpConnection [host = " + host + ", port = " + port + ", streamPtr = " + oss.str() +
                         ", lastReceiveDataTp = " + UtilTime::getISOTimestamp(lastReceiveDataTp) + "]";
    return output;
  }

  void clearBuffer() { this->buffer.consume(this->buffer.size()); }

  std::string host;
  std::string port;
  std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr{nullptr};
  TimePoint lastReceiveDataTp{std::chrono::seconds{0}};

  boost::beast::flat_buffer buffer;
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
