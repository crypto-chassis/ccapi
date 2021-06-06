#ifndef INCLUDE_CCAPI_CPP_CCAPI_UTIL_PRIVATE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_UTIL_PRIVATE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ccapi_cpp/ccapi_date.h"
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class UtilString CCAPI_FINAL {
 public:
  static bool isNumber(const std::string& s) {
    return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
  }
  // https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
  static std::string generateRandomString(const size_t len) {
    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::srand( (unsigned) std::time(NULL) * getpid());
    tmp_s.reserve(len);
    for (int i = 0; i < len; ++i) {
      tmp_s += alphanum[std::rand() % (sizeof(alphanum) - 1)];
    }
    return tmp_s;
  }
  static std::vector<std::string> split(const std::string& original, const std::string& delimiter) {
    std::string s = original;
    std::vector<std::string> output;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
      token = s.substr(0, pos);
      output.push_back(std::move(token));
      s.erase(0, pos + delimiter.length());
    }
    output.push_back(std::move(s));
    return output;
  }
  static std::set<std::string> splitToSet(const std::string& original, const std::string& delimiter) {
    std::string s = original;
    std::set<std::string> output;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
      token = s.substr(0, pos);
      output.insert(std::move(token));
      s.erase(0, pos + delimiter.length());
    }
    output.insert(std::move(s));
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
  static std::string trim(const std::string& original, const std::string& chars = "\t\n\v\f\r ") { return ltrim(rtrim(original, chars), chars); }
  static std::string firstNCharacter(const std::string& str, const size_t n) {
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
  static std::string convertFIXTimeToISO(const std::string& fixTime) {
    //  convert 20200925-15:55:28.093490622 to 2020-09-25T15:55:28.093490622Z
    std::string output;
    output += fixTime.substr(0, 4);
    output += "-";
    output += fixTime.substr(4, 2);
    output += "-";
    output += fixTime.substr(6, 2);
    output += "T";
    output += fixTime.substr(9);
    output += "Z";
    return output;
  }
  static std::string convertTimePointToFIXTime(const TimePoint& tp) {
    int year, month, day, hour, minute, second, millisecond;
    timePointToParts(tp, year, month, day, hour, minute, second, millisecond);
    std::string output;
    output += std::to_string(year);
    auto monthStr = std::to_string(month);
    output += std::string(2 - monthStr.length(), '0');
    output += monthStr;
    auto dayStr = std::to_string(day);
    output += std::string(2 - dayStr.length(), '0');
    output += dayStr;
    output += "-";
    auto hourStr = std::to_string(hour);
    output += std::string(2 - hourStr.length(), '0');
    output += hourStr;
    output += ":";
    auto minuteStr = std::to_string(minute);
    output += std::string(2 - minuteStr.length(), '0');
    output += minuteStr;
    output += ":";
    auto secondStr = std::to_string(second);
    output += std::string(2 - secondStr.length(), '0');
    output += secondStr;
    output += ".";
    auto millisecondStr = std::to_string(millisecond);
    output += std::string(3 - millisecondStr.length(), '0');
    output += millisecondStr;
    return output;
  }
  static void timePointToParts(TimePoint tp, int& year, int& month, int& day, int& hour, int& minute, int& second, int& millisecond) {
    auto epoch_sec = std::chrono::time_point_cast<std::chrono::seconds>(tp).time_since_epoch().count();
    auto day_sec = epoch_sec - (epoch_sec % 86400);
    auto days_since_epoch = day_sec / 86400;
    // see http://howardhinnant.github.io/date_algorithms.html
    days_since_epoch += 719468;
    const unsigned era = (days_since_epoch >= 0 ? days_since_epoch : days_since_epoch - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(days_since_epoch - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    year = static_cast<unsigned>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp + (mp < 10 ? 3 : -9);
    year += month <= 2;
    auto in_day = tp - std::chrono::seconds(day_sec);
    millisecond = std::chrono::time_point_cast<std::chrono::milliseconds>(in_day).time_since_epoch().count();
    hour = millisecond / (60 * 60 * 1000);
    millisecond -= hour * 60 * 60 * 1000;
    minute = millisecond / (60 * 1000);
    millisecond -= minute * 60 * 1000;
    second = millisecond / 1000;
    millisecond -= second * 1000;
  }

  static TimePoint now() {
    auto now = std::chrono::system_clock::now();
    return TimePoint(now);
  }
  static TimePoint parse(const std::string& s, const std::string& fmt = "%FT%TZ") {
    TimePoint tp;
    std::istringstream ss{s};
    ss >> date::parse(fmt, tp);
    if (ss.fail()) {
      CCAPI_LOGGER_FATAL("unable to parse time string");
    }
    return tp;
  }
  static TimePoint makeTimePoint(const std::pair<long long, long long>& timePair) {
    auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
    tp += std::chrono::nanoseconds(timePair.second);
    return tp;
  }
  static std::pair<long long, long long> divide(const TimePoint& tp) {
    auto then = tp.time_since_epoch();
    auto s = std::chrono::duration_cast<std::chrono::seconds>(then);
    then -= s;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(then);
    return std::make_pair(s.count(), ns.count());
  }
  static std::pair<long long, long long> divide(const std::string& seconds) {
    if (seconds.find(".") != std::string::npos) {
      auto splittedSeconds = UtilString::split(UtilString::rtrim(UtilString::rtrim(seconds, "0"), "."), ".");
      return std::make_pair(std::stoi(splittedSeconds[0]), splittedSeconds.size() == 1 ? 0 : std::stoi(UtilString::rightPadTo(splittedSeconds[1], 9, '0')));
    } else {
      return std::make_pair(std::stoi(seconds), 0);
    }
  }
  template <typename T = std::chrono::nanoseconds>
  static std::string getISOTimestamp(const TimePoint& tp, const std::string& fmt = "%FT%TZ") {
    return date::format(fmt.c_str(), date::floor<T>(tp));
  }
  static int getUnixTimestamp(const TimePoint& tp) {
    auto then = tp.time_since_epoch();
    auto s = std::chrono::duration_cast<std::chrono::seconds>(then);
    return s.count();
  }
  static TimePoint makeTimePointFromMilliseconds(long long milliseconds) { return TimePoint(std::chrono::milliseconds(milliseconds)); }
  static TimePoint makeTimePointFromSeconds(long seconds) { return TimePoint(std::chrono::seconds(seconds)); }
};
class UtilAlgorithm CCAPI_FINAL {
 public:
  static std::string stringToHex(const std::string& input) {
    static const char hex_digits[] = "0123456789abcdef";
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
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    int value = hex_values[hex_digit];
    if (value == -1) throw std::invalid_argument("invalid hex digit");
    return value;
  }
  static std::string hexToString(const std::string& input) {
    const auto len = input.length();
    if (len & 1) throw std::invalid_argument("odd length");
    std::string output;
    output.reserve(len / 2);
    for (auto it = input.begin(); it != input.end();) {
      int hi = hexValue(*it++);
      int lo = hexValue(*it++);
      output.push_back(hi << 4 | lo);
    }
    return output;
  }
  // https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
  static std::string base64Encode(const std::string& input) {
    static const unsigned char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *src = reinterpret_cast<const unsigned char*>(input.c_str());
    size_t len = input.length();
    unsigned char *out, *pos;
    const unsigned char *end, *in;
    size_t olen;
    olen = 4*((len + 2) / 3); /* 3-byte blocks to 4-byte */
    if (olen < len)
        return std::string(); /* integer overflow */
    std::string outStr;
    outStr.resize(olen);
    out = (unsigned char*)&outStr[0];
    end = src + len;
    in = src;
    pos = out;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
    }
    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }
    return outStr;
  }
  static std::string base64Decode(const std::string& in) {
    static const int B64index[256] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };
const void* data=in.c_str();
const size_t len = in.length();
unsigned char* p = (unsigned char*)data;
    int pad = len > 0 && (len % 4 || p[len - 1] == '=');
    const size_t L = ((len + 3) / 4 - pad) * 4;
    std::string str(L / 4 * 3 + pad, '\0');

    for (size_t i = 0, j = 0; i < L; i += 4)
    {
        int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
        str[j++] = n >> 16;
        str[j++] = n >> 8 & 0xFF;
        str[j++] = n & 0xFF;
    }
    if (pad)
    {
        int n = B64index[p[L]] << 18 | B64index[p[L + 1]] << 12;
        str[str.size() - 1] = n >> 16;

        if (len > L + 2 && p[L + 2] != '=')
        {
            n |= B64index[p[L + 2]] << 6;
            str.push_back(n >> 8 & 0xFF);
        }
    }
    return str;
  }
  //  https://github.com/brianloveswords/base64url
  static std::string base64UrlFromBase64(const std::string& base64) {
    return std::regex_replace(std::regex_replace(std::regex_replace(base64, std::regex("="), ""), std::regex("\\+"), "-"), std::regex("\\/"), "_");
  }
  static std::string base64FromBase64Url(const std::string& base64Url) {
    auto segmentLength = 4;
    auto stringLength = base64Url.size();
    auto diff = stringLength % segmentLength;
    if (!diff) {
      return base64Url;
    }
    auto padLength = segmentLength - diff;
    std::string paddedBase64Url(base64Url);
    paddedBase64Url += std::string(padLength, '=');
    return std::regex_replace(std::regex_replace(paddedBase64Url, std::regex("\\-"), "+"), std::regex("_"), "/");
  }
  static std::string base64UrlEncode(const std::string& in) { return base64UrlFromBase64(base64Encode(in)); }
  static std::string base64UrlDecode(const std::string& in) { return base64Decode(base64FromBase64Url(in)); }
  static double exponentialBackoff(double initial, double multiplier, double base, double exponent) { return initial + multiplier * (pow(base, exponent) - 1); }
  template <typename InputIterator>
  static uint_fast32_t crc(InputIterator first, InputIterator last);
};
template <typename InputIterator>
uint_fast32_t UtilAlgorithm::crc(InputIterator first, InputIterator last) {
  static auto const table = []() {
    auto const reversed_polynomial = uint_fast32_t{0xEDB88320uL};
    // This is a function object that calculates the checksum for a value,
    // then increments the value, starting from zero.
    struct byte_checksum {
      uint_fast32_t operator()() noexcept {
        auto checksum = static_cast<uint_fast32_t>(n++);
        for (auto i = 0; i < 8; ++i) checksum = (checksum >> 1) ^ ((checksum & 0x1u) ? reversed_polynomial : 0);
        return checksum;
      }
      unsigned n = 0;
    };
    auto table = std::array<uint_fast32_t, 256>{};
    std::generate(table.begin(), table.end(), byte_checksum{});
    return table;
  }();
  // Calculate the checksum - make sure to clip to 32 bits, for systems that don't
  // have a true (fast) 32-bit type.
  return uint_fast32_t{0xFFFFFFFFuL} &
         ~std::accumulate(first, last, ~uint_fast32_t{0} & uint_fast32_t{0xFFFFFFFFuL},
                          [](uint_fast32_t checksum, std::uint_fast8_t value) { return table[(checksum ^ value) & 0xFFu] ^ (checksum >> 8); });
}
class UtilSystem CCAPI_FINAL {
 public:
  static bool getEnvAsBool(const std::string variableName, const bool defaultValue = false) {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return UtilString::toLower(std::string(env_p)) == "true";
    } else {
      return defaultValue;
    }
  }
  static std::string getEnvAsString(const std::string variableName, const std::string defaultValue = "") {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return std::string(env_p);
    } else {
      return defaultValue;
    }
  }
  static int getEnvAsInt(const std::string variableName, const int defaultValue = 0) {
    const char* env_p = std::getenv(variableName.c_str());
    if (env_p) {
      return std::stoi(std::string(env_p));
    } else {
      return defaultValue;
    }
  }
  static long getEnvAsLong(const std::string variableName, const long defaultValue = 0) {
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
inline std::string size_tToString(const size_t& t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}
template <typename T>
std::string intToHex(T i) {
  std::stringstream stream;
  stream << std::hex << i;
  return stream.str();
}
template <typename T>
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
template <typename T>
struct reversion_wrapper {
  T& iterable;
};
template <typename T>
auto begin(reversion_wrapper<T> w) {
  return std::rbegin(w.iterable);
}
template <typename T>
auto end(reversion_wrapper<T> w) {
  return std::rend(w.iterable);
}
template <typename T>
reversion_wrapper<T> reverse(T&& iterable) {
  return {iterable};
}
template <typename K, typename V>
bool firstNSame(const std::map<K, V>& c1, const std::map<K, V>& c2, size_t n) {
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
template <typename K, typename V>
bool lastNSame(const std::map<K, V>& c1, const std::map<K, V>& c2, size_t n) {
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
template <typename K, typename V>
void keepFirstN(std::map<K, V>& c, size_t n) {
  if (!c.empty()) {
    auto it = c.begin();
    std::advance(it, std::min(n, c.size()));
    c.erase(it, c.end());
  }
}
template <typename K, typename V>
void keepLastN(std::map<K, V>& c, size_t n) {
  if (!c.empty()) {
    auto it = c.end();
    std::advance(it, -std::min(n, c.size()));
    c.erase(c.begin(), it);
  }
}
template <typename T>
typename std::enable_if<std::is_same<decltype(std::declval<const T&>().toString()), std::string>::value, std::string>::type toString(const T& t) {
  return t.toString();
}
template <typename T>
typename std::enable_if<std::is_same<decltype(std::declval<const T&>().toStringPretty()), std::string>::value, std::string>::type toStringPretty(
    const T& t, const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) {
  return t.toStringPretty(space, leftToIndent, indentFirstLine);
}
template <typename T>
typename std::enable_if<std::is_same<decltype(std::to_string(std::declval<T&>())), std::string>::value, std::string>::type toString(const T& t) {
  return std::to_string(t);
}
template <typename T>
typename std::enable_if<std::is_same<decltype(std::to_string(std::declval<T&>())), std::string>::value, std::string>::type toStringPretty(
    const T& t, const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) {
  std::string sl(leftToIndent, ' ');
  std::string output = (indentFirstLine ? sl : "") + std::to_string(t);
  return output;
}
template <typename T>
typename std::enable_if<std::is_same<T, std::string>::value, std::string>::type toString(const T& t) {
  return t;
}
template <typename T>
typename std::enable_if<std::is_same<T, std::string>::value, std::string>::type toStringPretty(const T& t, const int space = 2, const int leftToIndent = 0,
                                                                                               const bool indentFirstLine = true) {
  std::string sl(leftToIndent, ' ');
  std::string output = (indentFirstLine ? sl : "") + t;
  return output;
}
template <typename T>
typename std::enable_if<std::is_same<T, TimePoint>::value, std::string>::type toString(const T& t) {
  auto timePair = UtilTime::divide(t);
  return "(" + std::to_string(timePair.first) + "," + std::to_string(timePair.second) + ")";
}
template <typename T, typename... Args>
std::string toString(const std::unordered_set<T, Args...>& c);
template <typename T, typename... Args>
std::string toString(const std::set<T, Args...>& c);
template <typename K, typename V>
std::string toString(const std::map<K, V>& c);
template <typename K, typename V, typename... Args>
std::string toString(const std::unordered_map<K, V, Args...>& c);
template <typename K, typename V>
std::string firstNToString(const std::map<K, V>& c, const size_t n);
template <typename K, typename V>
std::string lastNToString(const std::map<K, V>& c, const size_t n);
template <typename T>
std::string toString(const std::vector<T>& c);
template <typename T>
std::string firstNToString(const std::vector<T>& c, const size_t n);
template <typename U, typename V>
std::string toString(const std::pair<U, V>& c) {
  std::string output = "(";
  output += toString(c.first);
  output += ",";
  output += toString(c.second);
  output += ")";
  return output;
}
template <typename T, typename... Args>
std::string toString(const std::unordered_set<T, Args...>& c) {
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
template <typename T, typename... Args>
std::string toString(const std::set<T, Args...>& c) {
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
template <typename K, typename V>
std::string toString(const std::map<K, V>& c) {
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
template <typename K, typename V>
std::string toStringPretty(const std::map<K, V>& c, const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) {
  std::string sl(leftToIndent, ' ');
  std::string output = (indentFirstLine ? sl : "") + "{\n";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toStringPretty(elem.first, space, space + leftToIndent, true);
    output += " = ";
    output += toStringPretty(elem.second, space, space + leftToIndent, false);
    if (i < size - 1) {
      output += ",\n";
    }
    ++i;
  }
  output += "\n" + sl + "}";
  return output;
}
template <typename K, typename V, typename... Args>
std::string toString(const std::unordered_map<K, V, Args...>& c) {
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
template <typename K, typename V>
std::string firstNToString(const std::map<K, V>& c, const size_t n) {
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
template <typename K, typename V>
std::string lastNToString(const std::map<K, V>& c, const size_t n) {
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
template <typename T>
std::string toString(const std::vector<T>& c) {
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
template <typename T>
std::string toStringPretty(const std::vector<T>& c, const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) {
  std::string sl(leftToIndent, ' ');
  std::string output = (indentFirstLine ? sl : "") + "[\n";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    output += toStringPretty(elem, space, space + leftToIndent, true);
    if (i < size - 1) {
      output += ",\n";
    }
    ++i;
  }
  output += "\n" + sl + "]";
  return output;
}
template <typename T>
std::string firstNToString(const std::vector<T>& c, const size_t n) {
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
template <typename T>
std::string firstNToStringPretty(const std::vector<T>& c, const size_t n, const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) {
  std::string sl(leftToIndent, ' ');
  std::string output = (indentFirstLine ? sl : "") + "[\n";
  auto size = c.size();
  auto i = 0;
  for (const auto& elem : c) {
    if (i >= n) {
      break;
    }
    output += toStringPretty(elem, space, space + leftToIndent, true);
    if (i < size - 1) {
      output += ",\n";
    }
    ++i;
  }
  if (i < size - 1 && i > 0) {
    output += "...";
  }
  output += "\n" + sl + "]";
  return output;
}
template <typename K, typename V>
std::map<V, std::vector<K> > invertMapMulti(const std::map<K, V>& c) {
  std::map<V, std::vector<K> > output;
  for (const auto& elem : c) {
    output[elem.second].push_back(elem.first);
  }
  return output;
}
template <typename K, typename V>
std::map<V, K> invertMap(const std::map<K, V>& c) {
  std::map<V, K> output;
  for (const auto& elem : c) {
    output.insert(std::make_pair(elem.second, elem.first));
  }
  return output;
}
template <template <class, class, class...> class C, typename K, typename V, typename... Args>
V mapGetWithDefault(const C<K, V, Args...>& m, const K& key, const V defaultValue = {}) {
  typename C<K, V, Args...>::const_iterator it = m.find(key);
  if (it == m.end()) {
    return defaultValue;
  }
  return it->second;
}
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_UTIL_PRIVATE_H_
