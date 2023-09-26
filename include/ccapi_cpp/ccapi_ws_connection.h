#ifndef INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#include <string>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace wspp = websocketpp;
namespace ccapi {
/**
 * This class represents a TCP socket connection for the websocket API.
 */
class WsConnection CCAPI_FINAL {
 public:
  WsConnection(std::string url, std::string group, std::vector<Subscription> subscriptionList, std::map<std::string, std::string> credential)
      : url(url), group(group), subscriptionList(subscriptionList), credential(credential) {
    this->assignDummyId();
  }
  WsConnection() {}
  void assignDummyId() {
    this->id = this->url + "||" + this->group + "||" + ccapi::toString(this->subscriptionList) + "||" + ccapi::toString(this->credential);
    this->hdl.reset();
  }
  std::string toString() const {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    std::string output = "WsConnection [id = " + id + ", url = " + url + ", group = " + group + ", subscriptionList = " + ccapi::toString(subscriptionList) +
                         ", credential = " + ccapi::toString(shortCredential) + ", status = " + statusToString(status) +
                         ", headers = " + ccapi::toString(headers) + "]";
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
  std::map<std::string, std::string> credential;
};
} /* namespace ccapi */
#else
#include <string>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace ccapi {
/**
 * This class represents a TCP socket connection for the websocket API.
 */
class WsConnection CCAPI_FINAL {
 public:
  WsConnection(std::string url, std::string group, std::vector<Subscription> subscriptionList, std::map<std::string, std::string> credential,
               std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream> > > streamPtr)
      : url(url), group(group), subscriptionList(subscriptionList), credential(credential), streamPtr(streamPtr) {
    this->id = this->url + "||" + this->group + "||" + ccapi::toString(this->subscriptionList) + "||" + ccapi::toString(this->credential);
    this->correlationIdList.reserve(subscriptionList.size());
    std::transform(subscriptionList.cbegin(), subscriptionList.cend(), std::back_inserter(this->correlationIdList),
                   [](Subscription subscription) { return subscription.getCorrelationId(); });
    this->setUrlParts();
  }
  WsConnection() {}
  std::string toString() const {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    std::ostringstream oss;
    oss << streamPtr;
    std::string output = "WsConnection [id = " + id + ", url = " + url + ", group = " + group + ", subscriptionList = " + ccapi::toString(subscriptionList) +
                         ", credential = " + ccapi::toString(shortCredential) + ", status = " + statusToString(status) +
                         ", headers = " + ccapi::toString(headers) + ", streamPtr = " + oss.str() + ", remoteCloseCode = " + std::to_string(remoteCloseCode) +
                         ", remoteCloseReason = " + std::string(remoteCloseReason.reason.c_str()) +
                         ", hostHttpHeaderValue = " + ccapi::toString(hostHttpHeaderValue) + ", path = " + ccapi::toString(path) +
                         ", host = " + ccapi::toString(host) + ", port = " + ccapi::toString(port) + "]";
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
  std::string getUrl() const { return url; }
  void setUrl(const std::string& url) {
    this->url = url;
    this->setUrlParts();
  }
  void setUrlParts() {
    auto splitted1 = UtilString::split(url, "://");
    if (splitted1.size() >= 2) {
      auto foundSlash = splitted1.at(1).find_first_of('/');
      auto foundQuestionMark = splitted1.at(1).find_first_of('?');
      if (foundSlash == std::string::npos && foundQuestionMark == std::string::npos) {
        this->path = "/";
      } else if (foundSlash == std::string::npos && foundQuestionMark != std::string::npos) {
        this->path = "/" + splitted1.at(1).substr(foundQuestionMark);
      } else if (foundSlash != std::string::npos && foundQuestionMark == std::string::npos) {
        this->path = splitted1.at(1).substr(foundSlash);
      } else {
        this->path = splitted1.at(1).substr(foundSlash);
      }
      auto splitted2 = UtilString::split(UtilString::split(splitted1.at(1), "/").at(0), ":");
      this->host = splitted2.at(0);
      if (splitted2.size() == 2) {
        this->port = splitted2.at(1);
      } else {
        if (splitted1.at(0) == "https" || splitted1.at(0) == "wss") {
          this->port = CCAPI_HTTPS_PORT_DEFAULT;
        } else {
          this->port = CCAPI_HTTP_PORT_DEFAULT;
        }
      }
    }
  }
  void appendUrlPart(const std::string& urlPart) {
    this->url += urlPart;
    this->setUrlParts();
  }
  std::string id;
  std::string group;
  std::vector<Subscription> subscriptionList;
  std::vector<std::string> correlationIdList;
  Status status{Status::UNKNOWN};
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> credential;
  std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream> > > streamPtr;
  beast::websocket::close_code remoteCloseCode{};
  beast::websocket::close_reason remoteCloseReason{};
  std::string hostHttpHeaderValue;
  std::string path;
  std::string host;
  std::string port;
#ifndef CCAPI_EXPOSE_INTERNAL
 private:
#endif
  std::string url;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
