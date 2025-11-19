#ifndef INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#define INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
#include <map>
#include <string>

#include "ccapi_cpp/ccapi_util_private.h"

namespace ccapi {

/**
 * Element represents an item in a message. The value(s) in an Element can be queried in a number of ways. Use the getValue() functions to retrieve a single
 * value. Use the getNameValueMap() function (or getTagValueList() function for FIX API) to retrieve all the values.
 */
class Element {
 public:
  explicit Element(bool isFix = false) : isFix(isFix) {}

  // Template insert: accept any string-like value
  template <typename S>
  void insert(std::string_view name, S&& value) {
    // Construct std::string only if necessary
    if constexpr (std::is_same_v<std::decay_t<S>, std::string>) {
      // If value is std::string, move it in to avoid copy
      nameValueMap.emplace(name, std::forward<S>(value));
    } else {
      // Otherwise, construct std::string from value (string_view, const char*)
      nameValueMap.emplace(name, std::string(std::forward<S>(value)));
    }
  }

  template <typename S>
  void insert(int tag, S&& value) {
    if constexpr (std::is_same_v<std::decay_t<S>, std::string>) {
      // Already a std::string — emplace directly
      tagValueList.emplace_back(tag, std::forward<S>(value));
    } else {
      // Convert to std::string
      tagValueList.emplace_back(tag, std::string(std::forward<S>(value)));
    }
  }

  // Template insert: accept any string-like value
  template <typename S>
  void insert_or_assign(std::string_view name, S&& value) {
    // Construct std::string only if necessary
    if constexpr (std::is_same_v<std::decay_t<S>, std::string>) {
      // If value is std::string, move it in to avoid copy
      nameValueMap.insert_or_assign(name, std::forward<S>(value));
    } else {
      // Otherwise, construct std::string from value (string_view, const char*)
      nameValueMap.insert_or_assign(name, std::string(std::forward<S>(value)));
    }
  }

  //   template <typename S>
  //   void insert_or_assign(int tag, S&& value) {
  //     if constexpr (std::is_same_v<std::decay_t<S>, std::string>) {
  //       // If already std::string, move to avoid copy
  //       tagValueList.insert_or_assign(tag, std::forward<S>(value));
  //     } else {
  //       // Otherwise, construct std::string from value (string_view, literal, etc.)
  //       tagValueList.insert_or_assign(tag, std::string(std::forward<S>(value)));
  //     }
  //   }

  //   void emplace(std::string& name, std::string& value) { this->nameValueMap.emplace(std::move(name), std::move(value)); }

  //   void emplace(int tag, std::string& value) { this->tagValueList.emplace(std::move(tag), std::move(value)); }

  bool has(std::string_view name) const { return this->nameValueMap.find(name) != this->nameValueMap.end(); }

  bool has(int tag) const {
    return std::any_of(tagValueList.begin(), tagValueList.end(), [tag](const auto& pair) { return pair.first == tag; });
  }

  std::string getValue(std::string_view name, const std::string& valueDefault = "") const {
    auto it = this->nameValueMap.find(name);
    return it == this->nameValueMap.end() ? valueDefault : it->second;
  }

  std::string getValue(int tag, const std::string& valueDefault = "") const {
    for (const auto& [key, value] : tagValueList) {
      if (key == tag) return value;
    }
    return valueDefault;
  }

  std::string toString() const {
    std::string output = isFix ? "Element [tagValueList = " + ccapi::toString(tagValueList) + ", nameValueMap = " + ccapi::toString(nameValueMap) + "]"
                               : "Element [nameValueMap = " + ccapi::toString(nameValueMap) + "]";
    return output;
  }

  std::string toPrettyString(const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) const {
    std::string sl(leftToIndent, ' ');
    std::string ss(leftToIndent + space, ' ');
    std::string output = isFix ? (indentFirstLine ? sl : "") + "Element [\n" + ss +
                                     "tagValueList = " + ccapi::toPrettyString(tagValueList, space, space + leftToIndent, false) + ",\n" + ss +
                                     "nameValueMap = " + ccapi::toPrettyString(nameValueMap, space, space + leftToIndent, false) + "\n" + sl + "]"
                               : (indentFirstLine ? sl : "") + "Element [\n" + ss +
                                     "nameValueMap = " + ccapi::toPrettyString(nameValueMap, space, space + leftToIndent, false) + "\n" + sl + "]";
    return output;
  }
#ifdef SWIG
  std::map<std::string, std::string> getNameValueMap() const {
    std::map<std::string, std::string> result;
    for (const auto& [key, value] : nameValueMap) {
        result.emplace(std::string(key), value);
    }
    return result;
}
#else
  const std::map<std::string_view, std::string>& getNameValueMap() const { return nameValueMap; }
#endif

  const std::vector<std::pair<int, std::string>>& getTagValueList() const { return tagValueList; }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  bool isFix{};
  std::map<std::string_view, std::string> nameValueMap;  // They key std::string_view is created from a string literal and therefore is safe, because string
                                                         // literals have static storage duration, meaning they live for the entire duration of the program.
  std::vector<std::pair<int, std::string>> tagValueList;
};

} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_ELEMENT_H_
