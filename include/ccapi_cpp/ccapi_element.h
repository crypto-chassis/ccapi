#ifndef INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#include <string>
#include <map>
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class Element CCAPI_FINAL {
 public:
  void insert(std::string name, std::string value) {
    this->nameValueMap.insert(std::pair<std::string, std::string>(name, value));
  }
  bool has(std::string name) const {
    return this->nameValueMap.find(name) != this->nameValueMap.end();
  }
  std::string getValue(std::string name) const {
    return this->nameValueMap.at(name);
  }
  std::string toString() const {
    std::string output = "Element [nameValueMap = " + ccapi::toString(nameValueMap) + "]";
    return output;
  }
  const std::map<std::string, std::string>& getNameValueMap() const {
    return nameValueMap;
  }

 private:
  std::map<std::string, std::string> nameValueMap;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
