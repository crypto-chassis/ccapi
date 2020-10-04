#ifndef INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#include <string>
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription_list.h"
namespace ccapi {
class WsConnection final {
 public:
  WsConnection(std::string url, SubscriptionList subscriptionList)
      : url(url),
        subscriptionList(subscriptionList) {
    this->assignDummyId();
  }
  void assignDummyId() {
    this->id = this->url + "|" + ccapi::toString(this->subscriptionList);
  }
  std::string toString() const {
    std::string output = "WsConnection [id = " + id + ", url = " + url + ", subscriptionList = "
        + ccapi::toString(subscriptionList) + ", status = " + statusToString(status) + "]";
    return output;
  }
  enum class Status {
    UNKNOWN,
    CONNECTING,
    OPEN,
    FAILED,
    CLOSING,
    CLOSED
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
        CCAPI_LOGGER_FATAL("");
    }
    return output;
  }
  std::string id;
  std::string url;
  SubscriptionList subscriptionList;
  Status status{Status::UNKNOWN};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
