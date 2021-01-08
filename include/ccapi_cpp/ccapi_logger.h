#ifndef INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
#define INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
#ifdef _WIN32
#define CCAPI_LOGGER_FILE_SEPARATOR '\\'
#else
#define CCAPI_LOGGER_FILE_SEPARATOR '/'
#endif
#define CCAPI_LOGGER_FILENAME (strrchr(__FILE__, CCAPI_LOGGER_FILE_SEPARATOR) ? strrchr(__FILE__, CCAPI_LOGGER_FILE_SEPARATOR) + 1 : __FILE__)
#define CCAPI_LOGGER_THREAD_ID std::this_thread::get_id()
#define CCAPI_LOGGER_NOW std::chrono::system_clock::now()
#if defined(CCAPI_ENABLE_LOG_FATAL) || defined(CCAPI_ENABLE_LOG_ERROR) || defined(CCAPI_ENABLE_LOG_WARN) || defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_FATAL(message) if (Logger::logger) { Logger::logger->fatal(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); throw std::runtime_error(message); }
#else
#define CCAPI_LOGGER_FATAL(message) throw std::runtime_error(message)
#endif
#if defined(CCAPI_ENABLE_LOG_ERROR) || defined(CCAPI_ENABLE_LOG_WARN) || defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_ERROR(message) if (Logger::logger) { Logger::logger->error(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); }
#else
#define CCAPI_LOGGER_ERROR(message)
#endif
#if defined(CCAPI_ENABLE_LOG_WARN) || defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_WARN(message) if (Logger::logger) { Logger::logger->warn(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); }
#else
#define CCAPI_LOGGER_WARN(message)
#endif
#if defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_INFO(message) if (Logger::logger) { Logger::logger->info(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); }
#else
#define CCAPI_LOGGER_INFO(message)
#endif
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_DEBUG(message) if (Logger::logger) { Logger::logger->debug(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); }
#else
#define CCAPI_LOGGER_DEBUG(message)
#endif
#if defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_TRACE(message) if (Logger::logger) { Logger::logger->trace(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); }
#else
#define CCAPI_LOGGER_TRACE(message)
#endif
#ifdef __GNUC__
#define __ccapi_cpp_func__ __PRETTY_FUNCTION__
#else
#define __ccapi_cpp_func__ __func__
#endif
#define CCAPI_LOGGER_FUNCTION_ENTER CCAPI_LOGGER_TRACE(std::string("enter ") + std::string(__ccapi_cpp_func__))
#define CCAPI_LOGGER_FUNCTION_EXIT CCAPI_LOGGER_TRACE(std::string("exit ") + std::string(__ccapi_cpp_func__))
#include <thread>
#include <string>
#include <cstring>
#include "ccapi_cpp/ccapi_macro.h"
#include "date/date.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class Logger {
 public:
  std::string SEVERITY_FATAL = "FATAL";
  std::string SEVERITY_ERROR = "ERROR";
  std::string SEVERITY_WARN = "WARN";
  std::string SEVERITY_INFO = "INFO";
  std::string SEVERITY_DEBUG = "DEBUG";
  std::string SEVERITY_TRACE = "TRACE";
  virtual ~Logger() {
  }
  void fatal(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessagePrivate(SEVERITY_FATAL, threadId, time, fileName, lineNumber, message);
  }
  void error(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessagePrivate(SEVERITY_ERROR, threadId, time, fileName, lineNumber, message);
  }
  void warn(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
            std::string message) {
    this->logMessagePrivate(SEVERITY_WARN, threadId, time, fileName, lineNumber, message);
  }
  void info(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
            std::string message) {
    this->logMessagePrivate(SEVERITY_INFO, threadId, time, fileName, lineNumber, message);
  }
  void debug(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessagePrivate(SEVERITY_DEBUG, threadId, time, fileName, lineNumber, message);
  }
  void trace(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessagePrivate(SEVERITY_TRACE, threadId, time, fileName, lineNumber, message);
  }
  static Logger* logger;

 protected:
  virtual void logMessage(std::string severity, std::string threadId, std::string timeISO,
                          std::string fileName, std::string lineNumber, std::string message) {
  }
  void logMessagePrivate(std::string severity, std::thread::id threadId, std::chrono::system_clock::time_point time,
                          std::string fileName, int lineNumber, std::string message) {
    std::stringstream ss;
    ss << threadId;
    this->logMessage(severity, ss.str(), getISOTimestamp(time), fileName, std::to_string(lineNumber), message);
  }
  template<typename T = std::chrono::nanoseconds>
  static std::string getISOTimestamp(TimePoint tp, std::string fmt = "%FT%TZ") {
    return date::format(fmt.c_str(), date::floor<T>(tp));
  }
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
