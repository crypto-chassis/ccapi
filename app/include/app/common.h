#ifndef APP_INCLUDE_APP_COMMON_H_
#define APP_INCLUDE_APP_COMMON_H_
#ifndef PUBLIC_SUBSCRIPTION_DATA_MARKET_DEPTH_CORRELATION_ID
#define PUBLIC_SUBSCRIPTION_DATA_MARKET_DEPTH_CORRELATION_ID "MARKET_DEPTH"
#endif
#ifndef PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID
#define PUBLIC_SUBSCRIPTION_DATA_TRADE_CORRELATION_ID "TRADE"
#endif
#include <cmath>
#include <fstream>
#include <mutex>
#include <string>
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class AppUtil {
 public:
  static std::string generateUuidV4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
      ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
      ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
      ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
      ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
      ss << dis(gen);
    };
    return ss.str();
  }
  static std::string printDoubleScientific(double number) {
    std::stringstream ss;
    ss << std::scientific;
    ss << number;
    return ss.str();
  }
  static double linearInterpolate(double x1, double y1, double x2, double y2, double x) { return y1 + (y2 - y1) / (x2 - x1) * (x - x1); }
  static std::string roundInput(double input, const std::string& inputIncrement, bool roundUp) {
    int64_t x = std::floor(input / std::stod(inputIncrement));
    if (roundUp) {
      x += 1;
    }
    std::string output;
    if (inputIncrement.find('.') != std::string::npos) {
      auto splitted = UtilString::split(inputIncrement, ".");
      auto splitted_0 = splitted.at(0);
      auto splitted_1 = splitted.at(1);
      if (splitted_0 == "0") {
        output = std::to_string(x * std::stoll(splitted_1));
      } else {
        output = std::to_string(x * std::stoll(splitted_0 + splitted_1));
      }
      if (output.length() > splitted_1.length()) {
        output.insert(output.length() - splitted_1.length(), 1, '.');
      } else {
        output = "0." + UtilString::leftPadTo(output, splitted_1.length(), '0');
      }
    } else {
      output = std::to_string(x * std::stoll(inputIncrement));
    }
    return output;
  }
};
class AppLogger {
 public:
  void log(const std::string& message) { this->log(std::chrono::system_clock::now(), message); }
  void log(const std::string& filename, const std::string& lineNumber, const std::string& message) {
    this->log(std::chrono::system_clock::now(), filename, lineNumber, message);
  }
  void log(const TimePoint& now, const std::string& message) {
    std::lock_guard<std::mutex> lock(m);
    std::cout << "[" << UtilTime::getISOTimestamp(now) << "] " << message << std::endl;
  }
  void log(const TimePoint& now, const std::string& filename, const std::string& lineNumber, const std::string& message) {
    std::lock_guard<std::mutex> lock(m);
    std::cout << "[" << UtilTime::getISOTimestamp(now) << "] {" << filename << ":" << lineNumber << "} " << message << std::endl;
  }
  void logDebug(const std::string& message, bool printDebug) { this->logDebug(std::chrono::system_clock::now(), message, printDebug); }
  void logDebug(const std::string& filename, const std::string& lineNumber, const std::string& message, bool printDebug) {
    this->logDebug(std::chrono::system_clock::now(), filename, lineNumber, message, printDebug);
  }
  void logDebug(const TimePoint& now, const std::string& message, bool printDebug) {
    if (printDebug) {
      std::lock_guard<std::mutex> lock(m);
      std::cout << "[" << UtilTime::getISOTimestamp(now) << "] " << message << std::endl;
    }
  }
  void logDebug(const TimePoint& now, const std::string& filename, const std::string& lineNumber, const std::string& message, bool printDebug) {
    if (printDebug) {
      std::lock_guard<std::mutex> lock(m);
      std::cout << "[" << UtilTime::getISOTimestamp(now) << "] {" << filename << ":" << lineNumber << "} " << message << std::endl;
    }
  }

 private:
  std::mutex m;
};
class CcapiLogger : public Logger {
 public:
  explicit CcapiLogger(AppLogger* appLogger) : Logger(), appLogger(appLogger) {}
  void logMessage(const std::string& severity, const std::string& threadId, const std::string& timeISO, const std::string& filename,
                  const std::string& lineNumber, const std::string& message) override {
    std::ostringstream oss;
    oss << threadId << ": {" << filename << ":" << lineNumber << "} " << severity << std::string(8, ' ') << message;
    this->appLogger->log(filename, lineNumber, oss.str());
  }

 private:
  AppLogger* appLogger;
};
class CsvWriter {
 public:
  CsvWriter() {}
  void open(const string& filename) { this->f.open(filename); }
  void close() {
    std::lock_guard<std::mutex> lock(m);
    this->f.close();
  }
  void writeRow(const std::vector<std::string>& row) {
    std::lock_guard<std::mutex> lock(m);
    size_t numCol = row.size();
    int i = 0;
    for (const auto& column : row) {
      this->f << column;
      if (i < numCol - 1) {
        this->f << ",";
      }
      ++i;
    }
    this->f << "\n";
  }
  void writeRows(const std::vector<std::vector<std::string>>& rows) {
    std::lock_guard<std::mutex> lock(m);
    for (const auto& row : rows) {
      size_t numCol = row.size();
      int i = 0;
      for (const auto& column : row) {
        this->f << column;
        if (i < numCol - 1) {
          this->f << ",";
        }
        ++i;
      }
      this->f << "\n";
    }
  }
  void flush() {
    std::lock_guard<std::mutex> lock(m);
    this->f.flush();
  }

 private:
  std::ofstream f;
  std::mutex m;
};
} /* namespace ccapi */
#endif  // APP_INCLUDE_APP_COMMON_H_
