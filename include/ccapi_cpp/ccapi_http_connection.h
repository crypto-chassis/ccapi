#ifndef INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
#include <string>
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_request.h"
#include <regex>
namespace beast = boost::beast;
namespace ccapi {
class HttpConnection final {
 public:
  HttpConnection(std::string host, std::string port, std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr):host(host),port(port),streamPtr(streamPtr){

  }
  std::string toString() const {
    std::ostringstream oss;
    oss << streamPtr;
    std::string output = "HttpConnection [host = " + host + ", port = " + port + ", streamPtr = " + oss.str() + "]";
    return output;
  }
  std::string host;
  std::string port;
  std::shared_ptr<beast::ssl_stream <beast::tcp_stream> > streamPtr;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_HTTP_CONNECTION_H_
