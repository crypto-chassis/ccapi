#ifndef INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_

#include <string>
#include <variant>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"

namespace ccapi {

/**
 * This class represents a TCP socket connection for the websocket API.
 */
class WsConnection {
 public:
  WsConnection(const WsConnection&) = delete;
  WsConnection& operator=(const WsConnection&) = delete;

  WsConnection(const std::string& url, const std::string& group, const std::vector<Subscription>& subscriptionList,
               const std::map<std::string, std::string>& credential, const std::string& proxyUrl = "")
      : url(url), group(group), subscriptionList(subscriptionList), credential(credential), proxyUrl(proxyUrl) {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    this->longId = this->url + "||" + this->group + "||" + ccapi::toString(this->subscriptionList) + "||" + ccapi::toString(shortCredential);
    this->id = UtilAlgorithm::shortBase62Hash(this->longId);
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
    std::visit(
        [&oss](auto&& streamPtr) {
          if (streamPtr) {
            oss << streamPtr.get();
          } else {
            oss << "nullptr";
          }
        },
        streamPtr);
    std::string output = "WsConnection [longId = " + longId + ", id = " + id + ", url = " + url + ", group = " + group +
                         ", subscriptionList = " + ccapi::toString(subscriptionList) + ", credential = " + ccapi::toString(shortCredential) +
                         ", proxyUrl = " + proxyUrl + ", status = " + statusToString(status) + ", headers = " + ccapi::toString(headers) +
                         ", streamPtr = " + oss.str() + ", remoteCloseCode = " + std::to_string(remoteCloseCode) +
                         ", remoteCloseReason = " + std::string(remoteCloseReason.reason.c_str()) +
                         ", hostHttpHeaderValue = " + ccapi::toString(hostHttpHeaderValue) + ", path = " + ccapi::toString(path) +
                         ", host = " + ccapi::toString(host) + ", port = " + ccapi::toString(port) + ", isSecure = " + ccapi::toString(isSecure) + "]";
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
      if (splitted1.at(0) == "https" || splitted1.at(0) == "wss") {
        this->isSecure = true;
      }
    }
  }

  void appendUrlPart(const std::string& urlPart) {
    this->url += urlPart;
    this->setUrlParts();
  }

  std::string longId;
  std::string id;
  std::string url;
  std::string group;
  std::vector<Subscription> subscriptionList;
  std::vector<std::string> correlationIdList;
  Status status{Status::UNKNOWN};
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> credential;
  std::string proxyUrl;
  std::variant<std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>>, std::shared_ptr<beast::websocket::stream<beast::tcp_stream>>>
      streamPtr;
  beast::websocket::close_code remoteCloseCode{};
  beast::websocket::close_reason remoteCloseReason{};
  std::string hostHttpHeaderValue;
  std::string path;
  std::string host;
  std::string port;

  beast::flat_buffer readMessageBuffer;
  std::array<char, CCAPI_WEBSOCKET_WRITE_BUFFER_SIZE> writeMessageBuffer;
  size_t writeMessageBufferWrittenLength{};
  std::vector<size_t> writeMessageBufferBoundary;
  bool isSecure{};
};

} /* namespace ccapi */

#endif  // INCLUDE_CCAPI_CPP_CCAPI_WS_CONNECTION_H_
