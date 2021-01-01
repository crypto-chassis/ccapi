#ifndef INCLUDE_CCAPI_CPP_CCAPI_UTIL_PRIVATE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_UTIL_PRIVATE_H_
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <array>
#include <cstdint>
#include <numeric>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include "date/date.h"
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class UtilString CCAPI_FINAL {
 public:
  static std::string generateRandomString(const size_t length) {
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
  static std::vector<std::string> split(const std::string& original, const std::string& delimiter) {
    std::string s = original;
    std::vector<std::string> output;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
      token = s.substr(0, pos);
      output.push_back(token);
      s.erase(0, pos + delimiter.length());
    }
    output.push_back(s);
    return output;
  }
  static std::string join(const std::vector<std::string>& strings, const std::string& delimiter) {
    switch (strings.size()) {
      case 0:
        return "";
      case 1:
        return strings[0];
      default:
        std::ostringstream joined;
        std::copy(strings.begin(), strings.end() - 1, std::ostream_iterator<std::string>(joined, delimiter.c_str()));
        joined << *strings.rbegin();
        return joined.str();
    }
  }
  static std::string toUpper(const std::string& input) {
    std::string output(input);
    std::transform(output.begin(), output.end(), output.begin(), ::toupper);
    return output;
  }
  static std::string toLower(const std::string& input) {
    std::string output(input);
    std::transform(output.begin(), output.end(), output.begin(), ::tolower);
    return output;
  }
  static std::string ltrim(const std::string& original, const std::string& chars = "\t\n\v\f\r ") {
    std::string str = original;
    str.erase(0, str.find_first_not_of(chars));
    return str;
  }
  static std::string rtrim(const std::string& original, const std::string& chars = "\t\n\v\f\r ") {
    std::string str = original;
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
  }
  static std::string trim(const std::string& original, const std::string& chars = "\t\n\v\f\r ") {
    return ltrim(rtrim(original, chars), chars);
  }
  static std::string firstNCharacter(const std::string& str, size_t n) {
    if (str.length() > n) {
      return str.substr(0, n) + "...";
    } else {
      return str;
    }
  }
  static std::string normalizeDecimalString(const std::string& str) {
    if (str.find('.') != std::string::npos) {
      return UtilString::rtrim(UtilString::rtrim(str, "0"), ".");
    } else {
      return str;
    }
  }
  static std::string leftPadTo(const std::string str, const size_t padToLength, const char paddingChar) {
    std::string copy = str;
    if (padToLength > copy.size()) {
      copy.insert(0, padToLength - copy.size(), paddingChar);
    }
    return copy;
  }
  static std::string rightPadTo(const std::string str, const size_t padToLength, const char paddingChar) {
    std::string copy = str;
    if (padToLength > copy.size()) {
      copy.append(padToLength - copy.size(), paddingChar);
    }
    return copy;
  }
};
class UtilTime CCAPI_FINAL {
 public:
  static TimePoint parse(std::string s) {
    TimePoint tp;
    std::istringstream ss { s };
    ss >> date::parse("%FT%TZ", tp);
    if (ss.fail()) {
      CCAPI_LOGGER_FATAL("unable to parse time string");
    }
    return tp;
  }
  static TimePoint makeTimePoint(std::pair<long long, long long> timePair) {
    auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
    tp += std::chrono::nanoseconds(timePair.second);
    return tp;
  }
  static std::pair<long long, long long> divide(TimePoint tp) {
    auto then = tp.time_since_epoch();
    auto s = std::chrono::duration_cast<std::chrono::seconds>(then);
    then -= s;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(then);
    return std::make_pair(s.count(), ns.count());
  }
  static std::pair<long long, long long> divide(std::string seconds) {
    if (seconds.find(".") != std::string::npos) {
      auto splittedSeconds = UtilString::split(UtilString::rtrim(UtilString::rtrim(seconds, "0"), "."), ".");
      return std::make_pair(
          std::stoi(splittedSeconds[0]),
          splittedSeconds.size() == 1 ? 0 : std::stoi(UtilString::rightPadTo(splittedSeconds[1], 9, '0')));
    } else {
      return std::make_pair(std::stoi(seconds), 0);
    }
  }
  template<typename T = std::chrono::nanoseconds>
  static std::string getISOTimestamp(TimePoint tp, std::string fmt = "%FT%TZ") {
    return date::format(fmt.c_str(), date::floor<T>(tp));
  }
  static TimePoint makeTimePointFromMilliseconds(long milliseconds) {
    return TimePoint(std::chrono::milliseconds(milliseconds));
  }
};
class UtilAlgorithm CCAPI_FINAL {
 public:
  static std::string stringToHex(const std::string& input) {
      static const char hex_digits[] = "0123456789ABCDEF";
      std::string output;
      output.reserve(input.length() * 2);
      for (unsigned char c : input) {
          output.push_back(hex_digits[c >> 4]);
          output.push_back(hex_digits[c & 15]);
      }
      return output;
  }
  static int hexValue(unsigned char hex_digit) {
      static const signed char hex_values[256] = {
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
           0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
          -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      };
      int value = hex_values[hex_digit];
      if (value == -1) throw std::invalid_argument("invalid hex digit");
      return value;
  }
  std::string hexToString(const std::string& input) {
      const auto len = input.length();
      if (len & 1) throw std::invalid_argument("odd length");
      std::string output;
      output.reserve(len / 2);
      for (auto it = input.begin(); it != input.end(); ) {
          int hi = hexValue(*it++);
          int lo = hexValue(*it++);
          output.push_back(hi << 4 | lo);
      }
      return output;
  }
  static std::string base64Encode(const std::string &in) {
      std::string out;
      int val = 0, valb = -6;
      for (unsigned char c : in) {
          val = (val << 8) + c;
          valb += 8;
          while (valb >= 0) {
              out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb)&0x3F]);
              valb -= 6;
          }
      }
      if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8))&0x3F]);
      while (out.size()%4) out.push_back('=');
      return out;
  }
  static std::string base64Decode(const std::string &in) {
      std::string out;
      std::vector<int> T(256, -1);
      for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
      int val = 0, valb = -8;
      for (unsigned char c : in) {
          if (T[c] == -1) break;
          val = (val << 6) + T[c];
          valb += 6;
          if (valb >= 0) {
              out.push_back(static_cast<char>((val >> valb)&0xFF));
              valb -= 8;
          }
      }
      return out;
  }
  static double exponentialBackoff(double initial, double multiplier, double base, double exponent) {
    return initial + multiplier * (pow(base, exponent) - 1);
  }
  template<typename InputIterator> static uint_fast32_t crc(InputIterator first, InputIterator last);
};
template<typename InputIterator> uint_fast32_t UtilAlgorithm::crc(InputIterator first, InputIterator last) {
  static auto const table = []() {
      auto const reversed_polynomial = uint_fast32_t {0xEDB88320uL};
      // This is a function object that calculates the checksum for a value,
      // then increments the value, starting from zero.
      struct byte_checksum {
        uint_fast32_t operator()() noexcept {
          auto checksum = static_cast<uint_fast32_t>(n++);
          for (auto i = 0; i < 8; ++i)
          checksum = (checksum >> 1) ^ ((checksum & 0x1u) ? reversed_polynomial : 0);
          return checksum;
        }
        unsigned n = 0;
      };
      auto table = std::array<uint_fast32_t, 256> {};
      std::generate(table.begin(), table.end(), byte_checksum {});
      return table;
    }();
  // Calculate the checksum - make sure to clip to 32 bits, for systems that don't
  // have a true (fast) 32-bit type.
  return uint_fast32_t { 0xFFFFFFFFuL }
      & ~std::accumulate(first, last, ~uint_fast32_t { 0 } & uint_fast32_t { 0xFFFFFFFFuL },
                         [](uint_fast32_t checksum, std::uint_fast8_t value)
                         { return table[(checksum ^ value) & 0xFFu] ^ (checksum >> 8);});
}
class UtilSystem CCAPI_FINAL {
 public:
  static bool getEnvAsBool(const std::string variableName,
                           const bool defaultValue = false) {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return UtilString::toLower(std::string(env_p)) == "true";
    } else {
      return defaultValue;
    }
  }
  static std::string getEnvAsString(const std::string variableName,
                                    const std::string defaultValue = "") {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return std::string(env_p);
    } else {
      return defaultValue;
    }
  }
  static int getEnvAsInt(const std::string variableName,
                         const int defaultValue = 0) {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return std::stoi(std::string(env_p));
    } else {
      return defaultValue;
    }
  }
  static long getEnvAsLong(const std::string variableName,
                           const long defaultValue = 0) {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return std::stol(std::string(env_p));
    } else {
      return defaultValue;
    }
  }
  static bool checkEnvExist(const std::string& variableName) {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return true;
    } else {
      return false;
    }
  }
};
inline std::string size_tToString(const size_t &t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}
template<typename T>
std::string intToHex(T i) {
  std::stringstream stream;
  stream << std::hex << i;
  return stream.str();
}
template<typename T>
int ceilSearch(const std::vector<T>& c, int low, int high, T x) {
  int i = 0;
  if (x <= c[low]) {
    return low;
  }
  for (i = low; i < high; i++) {
    if (c[i] == x) {
      return i;
    }
    if (c[i] < x && c[i + 1] >= x) {
      return i + 1;
    }
  }
  return -1;
}
template<typename T>
struct reversion_wrapper {
  T& iterable;
};
template<typename T>
auto begin(reversion_wrapper<T> w) {
  return std::rbegin(w.iterable);
}
template<typename T>
auto end(reversion_wrapper<T> w) {
  return std::rend(w.iterable);
}
template<typename T>
reversion_wrapper<T> reverse(T&& iterable) {
  return {iterable};
}
template<typename K, typename V> bool firstNSame(const std::map<K, V>& c1, const std::map<K, V>& c2, size_t n) {
  if (c1.empty() || c2.empty()) {
    return c1.empty() && c2.empty();
  }
  size_t i = 0;
  for (auto i1 = c1.begin(), i2 = c2.begin(); i1 != c1.end() && i2 != c2.end(); ++i1, ++i2) {
    if (i >= n) {
      return true;
    }
    if (i1 == c1.end() || i2 == c2.end()) {
      return false;
    }
    if (i1->first != i2->first || i1->second != i2->second) {
      return false;
    }
    ++i;
  }
  return true;
}
template<typename K, typename V> bool lastNSame(const std::map<K, V>& c1, const std::map<K, V>& c2, size_t n) {
  if (c1.empty() || c2.empty()) {
    return c1.empty() && c2.empty();
  }
  size_t i = 0;
  for (auto i1 = c1.rbegin(), i2 = c2.rbegin(); i1 != c1.rend() || i2 != c2.rend(); ++i1, ++i2) {
    if (i >= n) {
      return true;
    }
    if (i1 == c1.rend() || i2 == c2.rend()) {
      return false;
    }
    if (i1->first != i2->first || i1->second != i2->second) {
      return false;
    }
    ++i;
  }
  return true;
}
template<typename K, typename V> void keepFirstN(std::map<K, V>& c, size_t n) {
  if (!c.empty()) {
    auto it = c.begin();
    std::advance(it, std::min(n, c.size()));
    c.erase(it, c.end());
  }
}
template<typename K, typename V> void keepLastN(std::map<K, V>& c, size_t n) {
  if (!c.empty()) {
    auto it = c.end();
    std::advance(it, -std::min(n, c.size()));
    c.erase(c.begin(), it);
  }
}
template<typename T> typename std::enable_if<
    std::is_same<decltype(std::declval<const T&>().toString()), std::string>::value, std::string>::type toString(
    const T &t) {
  return t.toString();
}
template<typename T> typename std::enable_if<
    std::is_same<decltype(std::to_string(std::declval<T&>())), std::string>::value, std::string>::type toString(
    const T &t) {
  return std::to_string(t);
}
template<typename T> typename std::enable_if<std::is_same<T, std::string>::value, std::string>::type toString(
    const T &t) {
  return t;
}
template<typename T> typename std::enable_if<std::is_same<T, TimePoint>::value, std::string>::type toString(
    const T &t) {
  auto timePair = UtilTime::divide(t);
  return "(" + std::to_string(timePair.first) + "," + std::to_string(timePair.second) + ")";
}
template<typename T, typename ... Args> std::string toString(const std::unordered_set<T, Args...>& c);
template<typename T, typename ... Args> std::string toString(const std::set<T, Args...>& c);
template<typename K, typename V> std::string toString(const std::map<K, V>& c);
template<typename K, typename V, typename ... Args> std::string toString(const std::unordered_map<K, V, Args...>& c);
template<typename K, typename V> std::string firstNToString(const std::map<K, V>& c, size_t n);
template<typename K, typename V> std::string lastNToString(const std::map<K, V>& c, size_t n);
template<typename T> std::string toString(const std::vector<T>& c);
template<typename T> std::string firstNToString(const std::vector<T>& c, size_t n);
template<typename T, typename ... Args> std::string toString(const std::unordered_set<T, Args...>& c) {
  std::string output = "[";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toString(elem);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  output += "]";
  return output;
}
template<typename T, typename ... Args> std::string toString(const std::set<T, Args...>& c) {
  std::string output = "[";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toString(elem);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  output += "]";
  return output;
}
template<typename K, typename V> std::string toString(const std::map<K, V>& c) {
  std::string output = "{";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toString(elem.first);
    output += "=";
    output += toString(elem.second);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  output += "}";
  return output;
}
template<typename K, typename V, typename ... Args> std::string toString(const std::unordered_map<K, V, Args...>& c) {
  std::string output = "{";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toString(elem.first);
    output += "=";
    output += toString(elem.second);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  output += "}";
  return output;
}
template<typename K, typename V> std::string firstNToString(const std::map<K, V>& c, size_t n) {
  std::string output = "{";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    if (i >= n) {
      break;
    }
    output += toString(elem.first);
    output += "=";
    output += toString(elem.second);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  if (i < size - 1 && i > 0) {
    output += "...";
  }
  output += "}";
  return output;
}
template<typename K, typename V> std::string lastNToString(const std::map<K, V>& c, size_t n) {
  std::string output = "{";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : reverse(c)) {
    if (i >= n) {
      break;
    }
    output += toString(elem.first);
    output += "=";
    output += toString(elem.second);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  if (i < size - 1 && i > 0) {
    output += "...";
  }
  output += "}";
  return output;
}
template<typename T> std::string toString(const std::vector<T>& c) {
  std::string output = "[ ";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toString(elem);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  output += " ]";
  return output;
}
template<typename T> std::string firstNToString(const std::vector<T>& c, size_t n) {
  std::string output = "[ ";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    if (i >= n) {
      break;
    }
    output += toString(elem);
    if (i < size - 1) {
      output += ", ";
    }
    ++i;
  }
  if (i < size - 1 && i > 0) {
    output += "...";
  }
  output += " ]";
  return output;
}
template<typename K, typename V> std::map<V, std::vector<K> > invertMapMulti(const std::map<K, V>& c) {
  std::map<V, std::vector<K> > output;
  for (const auto& elem : c) {
    output[elem.second].push_back(elem.first);
  }
  return output;
}
template<typename K, typename V> std::map<V, K> invertMap(const std::map<K, V>& c) {
  std::map<V, K> output;
  for (const auto& elem : c) {
    output.insert(std::make_pair(elem.second, elem.first));
  }
  return output;
}
template <template<class, class, class...> class C, typename K, typename V, typename... Args>
V mapGetWithDefault(const C<K, V, Args...>& m, K const& key, const V & defaultValue) {
    typename C<K, V, Args...>::const_iterator it = m.find(key);
    if (it == m.end()) {
      return defaultValue;
    }
    return it->second;
}
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_UTIL_PRIVATE_H_
