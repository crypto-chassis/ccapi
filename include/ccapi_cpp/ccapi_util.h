#ifndef INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
#define INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <string>
#include <chrono>
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
#include "date/date.h"
#include "ccapi_cpp/ccapi_logger.h"
namespace ccapi {
typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> TimePoint;
class UtilString final {
 public:
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
class UtilTime final {
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
  static TimePoint makeTimePoint(std::pair<int, int> timePair) {
    auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
    tp += std::chrono::nanoseconds(timePair.second);
    return tp;
  }
  static std::pair<int, int> divide(TimePoint tp) {
    auto then = tp.time_since_epoch();
    auto s = std::chrono::duration_cast<std::chrono::seconds>(then);
    then -= s;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(then);
    return std::make_pair(s.count(), ns.count());
  }
  static std::pair<int, int> divide(std::string seconds) {
    if (seconds.find(".") != std::string::npos) {
      auto splittedSeconds = UtilString::split(UtilString::rtrim(UtilString::rtrim(seconds, "0"), "."), ".");
      return std::make_pair(
          std::stoi(splittedSeconds[0]),
          splittedSeconds.size() == 1 ? 0 : std::stoi(UtilString::rightPadTo(splittedSeconds[1], 9, '0')));
    } else {
      return std::make_pair(std::stoi(seconds), 0);
    }
  }
  static std::string getISOTimestamp(TimePoint tp) {
    return date::format("%FT%TZ", date::floor<std::chrono::nanoseconds>(tp));
  }
  static TimePoint makeTimePointFromMilliseconds(long milliseconds) {
    return TimePoint(std::chrono::milliseconds(milliseconds));
  }
};
class UtilAlgorithm final {
 public:
  static double exponentialBackoff(double initial, double multiplier, double base, double exponent) {
    return initial + multiplier * (pow(base, exponent) - 1);
  }
  template<typename InputIterator> static uint_fast32_t crc(InputIterator first, InputIterator last);
  static std::string hmacHex(std::string key, std::string msg) {
      unsigned char hash[32];
#if OPENSSL_VERSION_MAJOR > 1 || OPENSSL_VERSION_MAJOR == 1 && OPENSSL_VERSION_MINOR >= 1
      HMAC_CTX *hmac = HMAC_CTX_new();
      HMAC_Init_ex(hmac, &key[0], key.length(), EVP_sha256(), NULL);
      HMAC_Update(hmac, (unsigned char*)&msg[0], msg.length());
      unsigned int len = 32;
      HMAC_Final(hmac, hash, &len);
      HMAC_CTX_free(hmac);
#else
      HMAC_CTX hmac;
      HMAC_CTX_init(&hmac);
      HMAC_Init_ex(&hmac, &key[0], key.length(), EVP_sha256(), NULL);
      HMAC_Update(&hmac, (unsigned char*)&msg[0], msg.length());
      unsigned int len = 32;
      HMAC_Final(&hmac, hash, &len);
      HMAC_CTX_cleanup(&hmac);
#endif
      std::stringstream ss;
      ss << std::hex << std::setfill('0');
      for (int i = 0; i < len; i++) {
          ss << std::hex << std::setw(2)  << (unsigned int)hash[i];
      }
      return (ss.str());
  }
};
class UtilSystem final {
 public:
  static bool getEnvAsBool(const char * envVar) {
    return std::string(std::getenv(envVar)) == "true";
  }
  static std::string getEnvAsString(const char * envVar) {
    return std::string(std::getenv(envVar));
  }
  static int getEnvAsInt(const char * envVar) {
    return std::stoi(std::string(std::getenv(envVar)));
  }
  static long getEnvAsLong(const char * envVar) {
    return std::stol(std::string(std::getenv(envVar)));
  }
  static bool checkEnvExist(const char * envVar) {
    const char* env_p = std::getenv(envVar);
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
  std::pair<int, int> timePair = UtilTime::divide(t);
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
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_UTIL_H_
