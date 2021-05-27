#ifndef INCLUDE_CCAPI_CPP_CCAPI_DECIMAL_H_
#define INCLUDE_CCAPI_CPP_CCAPI_DECIMAL_H_
#include <string>
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
// minimalistic just for the purpose of being used as the key of a map
class Decimal CCAPI_FINAL {
 public:
  Decimal() {}
  explicit Decimal(const std::string& originalValue, bool keepTrailingZero = false) {
    if (originalValue.empty()) {
      CCAPI_LOGGER_FATAL("Decimal constructor input value cannot be empty");
    }
    std::string fixedPointValue = originalValue;
    this->sign = true;
    if (fixedPointValue.at(0) == '-') {
      fixedPointValue.erase(0, 1);
      this->sign = false;
    }
    if (fixedPointValue.find("E") != std::string::npos || fixedPointValue.find("e") != std::string::npos) {
      std::vector<std::string> splitted = UtilString::split(fixedPointValue, fixedPointValue.find("E") != std::string::npos ? "E" : "e");
      fixedPointValue = splitted.at(0);
      if (fixedPointValue.find(".") != std::string::npos) {
        fixedPointValue = UtilString::rtrim(UtilString::rtrim(fixedPointValue, "0"), ".");
      }
      if (splitted.at(1) != "0") {
        if (fixedPointValue.find(".") != std::string::npos) {
          std::vector<std::string> splittedByDecimal = UtilString::split(fixedPointValue, ".");
          if (splitted.at(1).at(0) != '-') {
            if (std::stoi(splitted.at(1)) < splittedByDecimal.at(1).length()) {
              fixedPointValue = splittedByDecimal.at(0) + splittedByDecimal.at(1).substr(0, std::stoi(splitted.at(1))) + "." +
                                splittedByDecimal.at(1).substr(std::stoi(splitted.at(1)));
            } else {
              fixedPointValue =
                  splittedByDecimal.at(0) + splittedByDecimal.at(1) + std::string(std::stoi(splitted.at(1)) - splittedByDecimal.at(1).length(), '0');
            }
          } else {
            fixedPointValue = "0." + std::string(-std::stoi(splitted.at(1)) - 1, '0') + splittedByDecimal.at(0) + splittedByDecimal.at(1);
          }
        } else {
          if (splitted.at(1).at(0) != '-') {
            fixedPointValue += std::string(std::stoi(splitted.at(1)), '0');
          } else {
            fixedPointValue = "0." + std::string(-std::stoi(splitted.at(1)) - 1, '0') + fixedPointValue;
          }
        }
      }
    }
    std::vector<std::string> splitted = UtilString::split(fixedPointValue, ".");
    // TODO(cryptochassis): replace with std::from_chars() once upgrade to C++17
    this->before = std::stoul(splitted.at(0));
    if (splitted.size() > 1) {
      this->frac = splitted.at(1);
      if (!keepTrailingZero) {
        this->frac = UtilString::rtrim(this->frac, "0");
      }
    }
  }
  std::string toString() const {
    std::string stringValue;
    if (!this->sign) {
      stringValue += "-";
    }
    stringValue += std::to_string(this->before);
    if (!this->frac.empty()) {
      stringValue += ".";
      stringValue += this->frac;
    }
    return stringValue;
  }
  friend bool operator<(const Decimal& l, const Decimal& r) {
    if (l.sign && r.sign) {
      if (l.before < r.before) {
        return true;
      } else if (l.before > r.before) {
        return false;
      } else {
        return l.frac < r.frac;
      }
    } else if (l.sign && !r.sign) {
      return false;
    } else if (!l.sign && r.sign) {
      return true;
    } else {
      Decimal nl = l;
      nl.sign = true;
      Decimal nr = r;
      nr.sign = true;
      return nl > nr;
    }
  }
  friend bool operator>(const Decimal& l, const Decimal& r) { return r < l; }
  friend bool operator<=(const Decimal& l, const Decimal& r) { return !(l > r); }
  friend bool operator>=(const Decimal& l, const Decimal& r) { return !(l < r); }
  friend bool operator==(const Decimal& l, const Decimal& r) { return !(l > r) && !(l < r); }
  friend bool operator!=(const Decimal& l, const Decimal& r) { return !(l == r); }

 private:
  // {-}bbbb.aaaa
  unsigned long before{};
  std::string frac;
  // -1 means negative sign needed
  bool sign{};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_DECIMAL_H_
