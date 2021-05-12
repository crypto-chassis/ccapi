#ifndef INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
#include <string>
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace wspp = websocketpp;
namespace ccapi {
class FixConnection CCAPI_FINAL {
 public:
  FixConnection(std::string host, std::string port, Subscription subscription, std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > streamPtr)
      : host(host), port(port), subscription(subscription), streamPtr(streamPtr) {
    this->id = subscription.getCorrelationId();
  }
  std::string toString() const {
    std::ostringstream oss;
    oss << streamPtr;
    std::string output = "FixConnection [id = " + id + ", host = " + host + ", port = " + port + ", subscription = " + ccapi::toString(subscription) +
                         ", status = " + statusToString(status) + ", streamPtr = " + oss.str() + "]";
    return output;
  }
  enum class Status { UNKNOWN, CONNECTING, OPEN, FAILED, CLOSING, CLOSED };
  static std::string statusToString(Status status) {
    std::string output;
    switch (status) {
      case Status::UNKNOWN:
        output = "UNKNOWN";
        break;
      case Status::CONNECTING:
        output = "CONNECTING";
        break;
      case Status::OPEN:
        output = "OPEN";
        break;
      case Status::FAILED:
        output = "FAILED";
        break;
      case Status::CLOSING:
        output = "CLOSING";
        break;
      case Status::CLOSED:
        output = "CLOSED";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  std::string id;
  std::string host;
  std::string port;
  Subscription subscription;
  Status status{Status::UNKNOWN};
  std::shared_ptr<beast::ssl_stream<beast::tcp_stream> > streamPtr;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
