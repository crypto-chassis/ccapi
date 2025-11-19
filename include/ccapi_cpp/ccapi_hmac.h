#ifndef INCLUDE_CCAPI_CPP_CCAPI_HMAC_H_
#define INCLUDE_CCAPI_CPP_CCAPI_HMAC_H_

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdint.h>

#include <iomanip>
#include <sstream>

namespace ccapi {

class Hmac {
 public:
  enum class ShaVersion {
    UNKNOWN,
    SHA1,
    SHA224,
    SHA256,
    SHA384,
    SHA512,
  };

  static std::string hmac(const ShaVersion shaVersion, const std::string& key, const std::string& msg, bool returnHex = false) {
    const EVP_MD* md = nullptr;

    switch (shaVersion) {
      case ShaVersion::SHA1:
        md = EVP_sha1();
        break;
      case ShaVersion::SHA224:
        md = EVP_sha224();
        break;
      case ShaVersion::SHA256:
        md = EVP_sha256();
        break;
      case ShaVersion::SHA384:
        md = EVP_sha384();
        break;
      case ShaVersion::SHA512:
        md = EVP_sha512();
        break;
      default:
        return "";  // Optionally throw or log unsupported algorithm
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    HMAC_CTX* hmac = HMAC_CTX_new();
    HMAC_Init_ex(hmac, key.data(), static_cast<int>(key.size()), md, nullptr);
    HMAC_Update(hmac, reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
    HMAC_Final(hmac, hash, &len);
    HMAC_CTX_free(hmac);

    std::stringstream ss;
    if (returnHex) {
      ss << std::hex << std::setfill('0');
      for (unsigned int i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(hash[i]);
      }
    } else {
      for (unsigned int i = 0; i < len; ++i) {
        ss << static_cast<char>(hash[i]);
      }
    }

    return ss.str();
  }
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_HMAC_H_
