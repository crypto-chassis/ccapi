#ifndef INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#include <map>
#include <string>

#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
/**
 * Element represents an item in a message. The value(s) in an Element can be queried in a number of ways. Use the getValue() functions to retrieve a single
 * value. Use the getNameValueMap() function (or getTagValueMap() function for FIX API) to retrieve all the values.
 */
class Element CCAPI_FINAL {
 public:
  explicit Element(bool isFix = false) : isFix(isFix) {}
  void insert(const std::string& name, const std::string& value) { this->nameValueMap.insert(std::pair<std::string, std::string>(name, value)); }
  void insert(int tag, const std::string& value) { this->tagValueMap.insert(std::pair<int, std::string>(tag, value)); }
  void emplace(std::string& name, std::string& value) { this->nameValueMap.emplace(std::move(name), std::move(value)); }
  void emplace(int tag, std::string& value) { this->tagValueMap.emplace(std::move(tag), std::move(value)); }
  bool has(const std::string& name) const { return this->nameValueMap.find(name) != this->nameValueMap.end(); }
  bool has(int tag) const { return this->tagValueMap.find(tag) != this->tagValueMap.end(); }
  std::string getValue(const std::string& name, const std::string valueDefault = "") const {
    auto it = this->nameValueMap.find(name);
    return it == this->nameValueMap.end() ? valueDefault : it->second;
  }
  std::string getValue(int tag, const std::string valueDefault = "") const {
    auto it = this->tagValueMap.find(tag);
    return it == this->tagValueMap.end() ? valueDefault : it->second;
  }
  std::string toString() const {
    std::string output =
        isFix ? "Element [tagValueMap = " + ccapi::toString(tagValueMap) + "]" : "Element [nameValueMap = " + ccapi::toString(nameValueMap) + "]";
    return output;
  }
  std::string toStringPretty(const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) const {
    std::string sl(leftToIndent, ' ');
    std::string ss(leftToIndent + space, ' ');
    std::string output = isFix ? (indentFirstLine ? sl : "") + "Element [\n" + ss +
                                     "tagValueMap = " + ccapi::toStringPretty(tagValueMap, space, space + leftToIndent, false) + "\n" + sl + "]"
                               : (indentFirstLine ? sl : "") + "Element [\n" + ss +
                                     "nameValueMap = " + ccapi::toStringPretty(nameValueMap, space, space + leftToIndent, false) + "\n" + sl + "]";
    return output;
  }
  const std::map<std::string, std::string>& getNameValueMap() const { return nameValueMap; }
  const std::map<int, std::string>& getTagValueMap() const { return tagValueMap; }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  bool isFix;
  std::map<std::string, std::string> nameValueMap;
  std::map<int, std::string> tagValueMap;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
