#ifndef INCLUDE_CCAPI_CPP_CCAPI_JWT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_JWT_H_
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
/**
 * This class is used for handling jwt tokens.
 */
class Jwt CCAPI_FINAL {
 public:
  static std::string generate(const Hmac::ShaVersion shaVersion, const std::string& secret, const std::string& payload) {
    std::string output;
    switch (shaVersion) {
      case Hmac::ShaVersion::SHA256:
        output += UtilAlgorithm::base64UrlEncode("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    output += ".";
    output += UtilAlgorithm::base64UrlEncode(payload);
    std::string signature = Hmac::hmac(shaVersion, secret, output);
    output += ".";
    output += UtilAlgorithm::base64UrlEncode(signature);
    return output;
  }
};
}  // namespace ccapi
#endif  // INCLUDE_CCAPI_CPP_CCAPI_JWT_H_
