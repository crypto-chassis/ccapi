#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
  class MyLogger final : public Logger {
   public:
    void logMessage(const std::string& severity, const std::string& threadId, const std::string& timeISO, const std::string& fileName,
                    const std::string& lineNumber, const std::string& message) override {
      std::lock_guard<std::mutex> lock(m);
      std::cout << threadId << ": [" << timeISO << "] {" << fileName << ":" << lineNumber << "} " << severity << std::string(8, ' ') << message << std::endl;
    }

   private:
    std::mutex m;
  };
  MyLogger myLogger;
  Logger* Logger::logger = &myLogger;class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    std::cout << "Received an event:\n" + event.toStringPretty(2, 2) << std::endl;
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
using ::ccapi::ExecutionManagementService;
int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("COINBASE_API_KEY").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_SECRET").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("COINBASE_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable COINBASE_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
    Request request(Request::Operation::GENERIC_PRIVATE_REQUEST, "coinbase", "", "Get all fills");
    request.appendParam({
        {"HTTP_METHOD", "GET"},
        {"HTTP_PATH", "/fills"},
        {"HTTP_QUERY_STRING", "product_id=BTC-USD"},
    });
    session.sendRequest(request);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
