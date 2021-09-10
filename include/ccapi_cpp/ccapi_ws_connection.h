#ifndef INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#include <string>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace wspp = websocketpp;
namespace ccapi {
class WsConnection CCAPI_FINAL {
 public:
  WsConnection(std::string url, std::string group, std::vector<Subscription> subscriptionList) : url(url), group(group), subscriptionList(subscriptionList) {
    this->assignDummyId();
  }
  void assignDummyId() {
    this->id = this->url + "||" + this->group + "||" + ccapi::toString(this->subscriptionList);
    this->hdl.reset();
  }
  std::string toString() const {
    std::string output = "WsConnection [id = " + id + ", url = " + url + ", group = " + group + ", subscriptionList = " + ccapi::toString(subscriptionList) +
                         ", status = " + statusToString(status) + ", headers = " + ccapi::toString(headers) + "]";
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
  std::string url;
  std::string group;
  std::vector<Subscription> subscriptionList;
  Status status{Status::UNKNOWN};
  wspp::connection_hdl hdl = wspp::lib::weak_ptr<void>();
  std::map<std::string, std::string> headers;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
