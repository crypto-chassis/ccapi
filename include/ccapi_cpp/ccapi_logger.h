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
#if defined(ENABLE_LOG_FATAL) || defined(ENABLE_LOG_ERROR) || defined(ENABLE_LOG_WARN) || defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_FATAL(message) Logger::logger->fatal(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message); throw std::runtime_error(message)
#else
#define CCAPI_LOGGER_FATAL(message) throw std::runtime_error(message);
#endif
#if defined(ENABLE_LOG_ERROR) || defined(ENABLE_LOG_WARN) || defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_ERROR(message) Logger::logger->error(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message)
#else
#define CCAPI_LOGGER_ERROR(message)
#endif
#if defined(ENABLE_LOG_WARN) || defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_WARN(message) Logger::logger->warn(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message)
#else
#define CCAPI_LOGGER_WARN(message)
#endif
#if defined(ENABLE_LOG_INFO) || defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_INFO(message) Logger::logger->info(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message)
#else
#define CCAPI_LOGGER_INFO(message)
#endif
#if defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_DEBUG(message) Logger::logger->debug(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message)
#else
#define CCAPI_LOGGER_DEBUG(message)
#endif
#if defined(ENABLE_LOG_TRACE)
#define CCAPI_LOGGER_TRACE(message) Logger::logger->trace(CCAPI_LOGGER_THREAD_ID, CCAPI_LOGGER_NOW, CCAPI_LOGGER_FILENAME, __LINE__, message)
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
namespace ccapi {
class Logger {
 public:
  enum class Severity {
    FATAL = 1,
    ERROR = 2,
    WARN = 3,
    INFO = 4,
    DEBUG = 5,
    TRACE = 6
  };
  static std::string severityToString(Severity severity) {
    std::string output;
    switch (severity) {
      case Severity::FATAL:
        output = "FATAL";
        break;
      case Severity::ERROR:
        output = "ERROR";
        break;
      case Severity::WARN:
        output = "WARN";
        break;
      case Severity::INFO:
        output = "INFO";
        break;
      case Severity::DEBUG:
        output = "DEBUG";
        break;
      case Severity::TRACE:
        output = "TRACE";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  virtual ~Logger() {
  }
  virtual void logMessage(Severity severity, std::thread::id threadId, std::chrono::system_clock::time_point time,
                          std::string fileName, int lineNumber, std::string message) {
  }
  void fatal(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessage(Severity::FATAL, threadId, time, fileName, lineNumber, message);
  }
  void error(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessage(Severity::ERROR, threadId, time, fileName, lineNumber, message);
  }
  void warn(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
            std::string message) {
    this->logMessage(Severity::WARN, threadId, time, fileName, lineNumber, message);
  }
  void info(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
            std::string message) {
    this->logMessage(Severity::INFO, threadId, time, fileName, lineNumber, message);
  }
  void debug(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessage(Severity::DEBUG, threadId, time, fileName, lineNumber, message);
  }
  void trace(std::thread::id threadId, std::chrono::system_clock::time_point time, std::string fileName, int lineNumber,
             std::string message) {
    this->logMessage(Severity::TRACE, threadId, time, fileName, lineNumber, message);
  }
  static Logger* logger;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_LOGGER_H_
