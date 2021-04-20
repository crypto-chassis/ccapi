#ifndef INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#include <map>
#include <string>
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class Element CCAPI_FINAL {
 public:
  void insert(const std::string& name, const std::string& value) { this->nameValueMap.insert(std::pair<std::string, std::string>(name, value)); }
  void emplace(std::string& name, std::string& value) { this->nameValueMap.emplace(std::move(name), std::move(value)); }
  bool has(const std::string& name) const { return this->nameValueMap.find(name) != this->nameValueMap.end(); }
  std::string getValue(const std::string& name, const std::string valueDefault = "") const {
    auto it = this->nameValueMap.find(name);
    return it == this->nameValueMap.end() ? valueDefault : it->second;
  }
  std::string toString() const {
    std::string output = "Element [nameValueMap = " + ccapi::toString(nameValueMap) + "]";
    return output;
  }
  std::string toStringPretty(const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) const {
    std::string sl(leftToIndent, ' ');
    std::string ss(leftToIndent + space, ' ');
    std::string output = (indentFirstLine ? sl : "") + "Element [\n" + ss +
                         "nameValueMap = " + ccapi::toStringPretty(nameValueMap, space, space + leftToIndent, false) + "\n" + sl + "]";
    return output;
  }
  const std::map<std::string, std::string>& getNameValueMap() const { return nameValueMap; }

 private:
  std::map<std::string, std::string> nameValueMap;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
