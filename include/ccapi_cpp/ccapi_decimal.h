#ifndef INCLUDE_CCAPI_CPP_CCAPI_DECIMAL_H_
#define INCLUDE_CCAPI_CPP_CCAPI_DECIMAL_H_
#include <string>
namespace ccapi {
// minimalistic just for the purpose of being used as the key of a map
class Decimal final {
 public:
  explicit Decimal(std::string originalValue) {
//    if (value.at(0) == '-') {
//      throw std::runtime_error("Decimal could only be used to represent unsigned decimal.");
//    }
    std::string value = originalValue;
    this->sign = true;
    if (originalValue.at(0) == '-') {
      value.erase(0);
      this->sign = false;
    }
    std::string fixedPointValue = value;
    if (value.find("e") != std::string::npos) {
      std::vector<std::string> splitted = UtilString::split(value, "e");
      fixedPointValue = splitted.at(0);
      if (fixedPointValue.find(".") != std::string::npos) {
        fixedPointValue = UtilString::rtrim(UtilString::rtrim(fixedPointValue, "0"), ".");
      }
      if (splitted.at(1) != "0") {
        if (fixedPointValue.find(".") != std::string::npos) {
          std::vector<std::string> splittedByDecimal = UtilString::split(fixedPointValue, ".");
          if (splitted.at(1).at(0) != '-') {
            if (std::stoi(splitted.at(1)) < splittedByDecimal.at(1).length()) {
              fixedPointValue = splittedByDecimal.at(0) + splittedByDecimal.at(1).substr(0, std::stoi(splitted.at(1)))
                  + "." + splittedByDecimal.at(1).substr(std::stoi(splitted.at(1)));
            } else {
              fixedPointValue = splittedByDecimal.at(0) + splittedByDecimal.at(1)
                  + std::string(std::stoi(splitted.at(1)) - splittedByDecimal.at(1).length(), '0');
            }
          } else {
            fixedPointValue = "0." + std::string(-std::stoi(splitted.at(1)) - 1, '0') + splittedByDecimal.at(0)
                + splittedByDecimal.at(1);
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
    this->before = std::stoul(splitted.at(0));
    if (splitted.size() > 1) {
      this->frac = splitted.at(1);
      UtilString::rtrim(this->frac, "0");
    }


//    this->whole = splitted.at(0);
//    if (splitted.size() > 1) {
//      this->frac = splitted.at(1);
//      UtilString::rtrim(this->frac, "0");
//    } else {
//      this->frac = "";
//    }
//    this->value = this->frac.empty() ? this->whole : this->whole + "." + this->frac;
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
//  Decimal static mid(const Decimal& a, const Decimal& b) {
//    auto aScale = a.frac.length();
//    auto bScale = b.frac.length();
//    unsigned long long ua;
//    unsigned long long ub;
//    size_t scale;
//    if (aScale < bScale) {
//      ua = std::stoull(a.whole + a.frac + std::string(bScale - aScale, '0'));
//      ub = std::stoull(b.whole + b.frac);
//      scale = bScale;
//    } else {
//      ua = std::stoull(a.whole + a.frac);
//      ub = std::stoull(b.whole + b.frac + std::string(aScale - bScale, '0'));
//      scale = aScale;
//    }
//    unsigned long long umid;
//    div_t divResult;
//    if (ua < ub) {
//      divResult = std::div(ub - ua, 2);
//      umid = ua + divResult.quot;
//    } else {
//      divResult = std::div(ua - ub, 2);
//      umid = ub + divResult.quot;
//    }
//    std::string umidString = std::to_string(umid);
//    std::string midString = umidString.substr(0, umidString.length() - scale) + "."
//        + umidString.substr(umidString.length() - scale);
//    if (divResult.rem > 0) {
//      midString += "5";
//    }
//    return Decimal(midString);
//  }
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




//    if (l.whole.length() < r.whole.length()) {
//      return true;
//    } else if (l.whole.length() == r.whole.length()) {
//      if (l.whole < r.whole) {
//        return true;
//      } else if (l.whole == r.whole) {
//        return l.frac < r.frac;
//      } else {
//        return false;
//      }
//    } else {
//      return false;
//    }
  }
//  friend bool operator<(const Decimal& l, const Decimal& r) {
//    if (l.whole.length() < r.whole.length()) {
//      return true;
//    } else if (l.whole.length() == r.whole.length()) {
//      if (l.whole < r.whole) {
//        return true;
//      } else if (l.whole == r.whole) {
//        return l.frac < r.frac;
//      } else {
//        return false;
//      }
//    } else {
//      return false;
//    }
//  }
  friend bool operator>(const Decimal& l, const Decimal& r) {
    return r < l;
  }
  friend bool operator<=(const Decimal& l, const Decimal& r) {
    return !(l > r);
  }
  friend bool operator>=(const Decimal& l, const Decimal& r) {
    return !(l < r);
  }
  friend bool operator==(const Decimal& l, const Decimal& r) {
    return !(l > r) && !(l < r);
  }
  friend bool operator!=(const Decimal& l, const Decimal& r) {
    return !(l == r);
  }

 private:
//  std::string stringValue;

//  std::string whole;
//  std::string frac;

//  {-}bbbb.aaaa
  unsigned long before{};
  std::string frac;
  // -1 means negative sign needed
  bool sign{};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_DECIMAL_H_
