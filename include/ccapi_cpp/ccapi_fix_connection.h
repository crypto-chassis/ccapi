#ifndef INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
#include <string>
#include <variant>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_subscription.h"
namespace beast = boost::beast;

namespace ccapi {

/**
 * This class represents a TCP socket connection for the FIX API.
 */

class FixConnection {
 public:
  FixConnection(const FixConnection&) = delete;
  FixConnection& operator=(const FixConnection&) = delete;

  FixConnection(const std::string& url, const Subscription& subscription, const std::map<std::string, std::string>& credential)
      : url(url), subscription(subscription), credential(credential) {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    this->longId = this->url + "||" + ccapi::toString(this->subscription) + "||" + ccapi::toString(shortCredential);
    this->id = UtilAlgorithm::shortBase62Hash(this->longId);
    this->setUrlParts();
  }

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
    std::string output = "FixConnection [longId = " + longId + ", id = " + id + ", url = " + url + ", subscription = " + ccapi::toString(subscription) +
                         ", credential = " + ccapi::toString(shortCredential) + ", status = " + statusToString(status) + ", streamPtr = " + oss.str() +
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
      auto splitted2 = UtilString::split(splitted1.at(1), ":");
      this->host = splitted2.at(0);
      if (splitted2.size() == 2) {
        this->port = splitted2.at(1);
      } else {
        if (splitted1.at(0) == "tcp+tls" || splitted1.at(0) == "tcp+ssl") {
          this->port = CCAPI_HTTPS_PORT_DEFAULT;
        } else {
          this->port = CCAPI_HTTP_PORT_DEFAULT;
        }
      }
      if (splitted1.at(0) == "tcp+tls" || splitted1.at(0) == "tcp+ssl") {
        this->isSecure = true;
      }
    }
  }

  std::string longId;
  std::string id;
  std::string url;
  Subscription subscription;
  Status status{Status::UNKNOWN};
  std::map<std::string, std::string> credential;
  std::variant<std::shared_ptr<beast::ssl_stream<beast::tcp_stream>>, std::shared_ptr<beast::tcp_stream>> streamPtr;
  std::string host;
  std::string port;
  bool isSecure{};
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_FIX_CONNECTION_H_
