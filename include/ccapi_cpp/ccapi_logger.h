#ifndef INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
#define INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
#ifdef _WIN32
#define CCAPI_LOGGER_FILE_SEPARATOR '\\'
#else
#define CCAPI_LOGGER_FILE_SEPARATOR '/'
#endif
#define CCAPI_LOGGER_FILE_NAME (strrchr(__FILE__, CCAPI_LOGGER_FILE_SEPARATOR) ? strrchr(__FILE__, CCAPI_LOGGER_FILE_SEPARATOR) + 1 : __FILE__)
#define CCAPI_LOGGER_LINE_NUMBER std::to_string(__LINE__)
#define CCAPI_LOGGER_THREAD_ID std::this_thread::get_id()
#define CCAPI_LOGGER_NOW std::chrono::system_clock::now()
#if defined(CCAPI_ENABLE_LOG_FATAL) || defined(CCAPI_ENABLE_LOG_ERROR) || defined(CCAPI_ENABLE_LOG_WARN) || defined(CCAPI_ENABLE_LOG_INFO) || \
    defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_FATAL(message)                                                                                                      \
  if (::ccapi::Logger::logger) {                                                                                                         \
    ::ccapi::Logger::logger->fatal(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, message); \
    throw std::runtime_error(message);                                                                                                   \
  }
#else
#define CCAPI_LOGGER_FATAL(message) throw std::runtime_error(message)
#endif
#if defined(CCAPI_ENABLE_LOG_ERROR) || defined(CCAPI_ENABLE_LOG_WARN) || defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || \
    defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_ERROR(message)                                                                                                      \
  if (::ccapi::Logger::logger) {                                                                                                         \
    ::ccapi::Logger::logger->error(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, message); \
  }
#else
#define CCAPI_LOGGER_ERROR(message)
#endif
#if defined(CCAPI_ENABLE_LOG_WARN) || defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_WARN(message)                                                                                                      \
  if (::ccapi::Logger::logger) {                                                                                                        \
    ::ccapi::Logger::logger->warn(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, message); \
  }
#else
#define CCAPI_LOGGER_WARN(message)
#endif
#if defined(CCAPI_ENABLE_LOG_INFO) || defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_INFO(message)                                                                                                      \
  if (::ccapi::Logger::logger) {                                                                                                        \
    ::ccapi::Logger::logger->info(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, message); \
  }
#else
#define CCAPI_LOGGER_INFO(message)
#endif
#if defined(CCAPI_ENABLE_LOG_DEBUG) || defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_DEBUG(message)                                                                                                      \
  if (::ccapi::Logger::logger) {                                                                                                         \
    ::ccapi::Logger::logger->debug(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, message); \
  }
#else
#define CCAPI_LOGGER_DEBUG(message)
#endif
#if defined(CCAPI_ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_TRACE(message)                                                                                                      \
  if (::ccapi::Logger::logger) {                                                                                                         \
    ::ccapi::Logger::logger->trace(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILE_NAME, CCAPI_LOGGER_LINE_NUMBER, message); \
  }
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
#include <cstring>
#include <string>
#include <thread>

#include "ccapi_cpp/ccapi_date.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
/**
 * This class is used for library logging.
 */
class Logger {
 public:
  std::string LOG_SEVERITY_FATAL = "FATAL";
  std::string LOG_SEVERITY_ERROR = "ERROR";
  std::string LOG_SEVERITY_WARN = "WARN";
  std::string LOG_SEVERITY_INFO = "INFO";
  std::string LOG_SEVERITY_DEBUG = "DEBUG";
  std::string LOG_SEVERITY_TRACE = "TRACE";
  virtual ~Logger() {}
  void fatal(const std::thread::id& threadId, const std::chrono::system_clock::time_point& time, const std::string& fileName, const std::string& lineNumber,
             const std::string& message) {
    this->logMessagePrivate(LOG_SEVERITY_FATAL, threadId, time, fileName, lineNumber, message);
  }
  void error(const std::thread::id& threadId, const std::chrono::system_clock::time_point& time, const std::string& fileName, const std::string& lineNumber,
             const std::string& message) {
    this->logMessagePrivate(LOG_SEVERITY_ERROR, threadId, time, fileName, lineNumber, message);
  }
  void warn(const std::thread::id& threadId, const std::chrono::system_clock::time_point& time, const std::string& fileName, const std::string& lineNumber,
            const std::string& message) {
    this->logMessagePrivate(LOG_SEVERITY_WARN, threadId, time, fileName, lineNumber, message);
  }
  void info(const std::thread::id& threadId, const std::chrono::system_clock::time_point& time, const std::string& fileName, const std::string& lineNumber,
            const std::string& message) {
    this->logMessagePrivate(LOG_SEVERITY_INFO, threadId, time, fileName, lineNumber, message);
  }
  void debug(const std::thread::id& threadId, const std::chrono::system_clock::time_point& time, const std::string& fileName, const std::string& lineNumber,
             const std::string& message) {
    this->logMessagePrivate(LOG_SEVERITY_DEBUG, threadId, time, fileName, lineNumber, message);
  }
  void trace(const std::thread::id& threadId, const std::chrono::system_clock::time_point& time, const std::string& fileName, const std::string& lineNumber,
             const std::string& message) {
    this->logMessagePrivate(LOG_SEVERITY_TRACE, threadId, time, fileName, lineNumber, message);
  }
  static Logger* logger;
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  virtual void logMessage(const std::string& severity, const std::string& threadId, const std::string& timeISO, const std::string& fileName,
                          const std::string& lineNumber, const std::string& message) {}
  void logMessagePrivate(const std::string& severity, const std::thread::id& threadId, const std::chrono::system_clock::time_point& time,
                         const std::string& fileName, const std::string& lineNumber, const std::string& message) {
    std::stringstream ss;
    ss << threadId;
    this->logMessage(severity, ss.str(), getISOTimestamp(time), fileName, lineNumber, message);
  }
  template <typename T = std::chrono::nanoseconds>
  static std::string getISOTimestamp(const TimePoint& tp, const std::string& fmt = "%FT%TZ") {
    return date::format(fmt.c_str(), date::floor<T>(tp));
  }
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
