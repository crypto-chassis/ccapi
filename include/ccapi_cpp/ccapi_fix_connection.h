#ifndef INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
#include <string>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace beast = boost::beast;
namespace ccapi {
template <class T>
class FixConnection CCAPI_FINAL {
  /**
   * This class represents a TCP socket connection for the FIX API.
   */
 public:
  FixConnection(std::string host, std::string port, Subscription subscription, std::shared_ptr<T> streamPtr)
      : host(host), port(port), subscription(subscription), streamPtr(streamPtr) {
    this->id = subscription.getCorrelationId();
    this->url = host + ":" + port;
  }
  std::string toString() const {
    std::ostringstream oss;
    oss << streamPtr;
    std::string output = "FixConnection [id = " + id + ", host = " + host + ", port = " + port + ", subscription = " + ccapi::toString(subscription) +
                         ", status = " + statusToString(status) + ", streamPtr = " + oss.str() + "]";
    return output;
  }
  enum class Status {
    UNKNOWN,
    CONNECTING,
    OPEN,
    FAILED,
    CLOSING,
    CLOSED,
  };
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
  std::string url;
  Subscription subscription;
  Status status{Status::UNKNOWN};
  std::shared_ptr<T> streamPtr;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
