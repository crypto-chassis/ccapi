#ifndef INCLUDE_CCAPI_CPP_CCAPI_CORRELATIONID_H_
#define INCLUDE_CCAPI_CPP_CCAPI_CORRELATIONID_H_
#include <string>
#include <iostream>
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class CorrelationId final {
 public:
  CorrelationId() {
    this->charId = this->genRandomString(32);
  }
  explicit CorrelationId(std::string charId)
      : charId(charId) {
  }
  friend bool operator ==(const CorrelationId& l, const CorrelationId& r) {
    return l.charId == r.charId;
  }
  const std::string& getCharId() const {
    return charId;
  }
  std::string genRandomString(const size_t length) {
    auto randchar = []() -> char {
      const char charset[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
      const size_t max_index = (sizeof(charset) - 1);
      return charset[ rand() % max_index ];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
  }
  std::string toString() const {
    std::string output = "CorrelationId [charId = " + ccapi::toString(charId) + "]";
    return output;
  }

 private:
  std::string charId;
};
class CorrelationIdHash {
 public:
  size_t operator()(const CorrelationId& x) const {
    return (std::hash<std::string> { }(x.getCharId()));
  }
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_CORRELATIONID_H_
