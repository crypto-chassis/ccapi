#include "ccapi_cpp/ccapi_logger.h"
#include <iostream>
namespace ccapi {
class MyLogger final : public Logger {
 public:
  void logMessage(std::string severity, std::string threadId, std::string timeISO, std::string fileName, std::string lineNumber, std::string message) override {
    std::lock_guard<std::mutex> lock(m);
    std::cout << threadId << ": [" << timeISO << "] {" << fileName << ":" << lineNumber << "} " << severity << std::string(8, ' ') << message << std::endl;
  }

 private:
  std::mutex m;
};
MyLogger myLogger;
Logger* Logger::logger = &myLogger;
}  // namespace ccapi
