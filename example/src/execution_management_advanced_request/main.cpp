#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* sessionPtr) override { std::cout << "Received an event:\n" + event.toPrettyString(2, 2) << std::endl; }
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::UtilSystem;

int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("OKX_API_KEY").empty()) {
    std::cerr << "Please set environment variable OKX_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("OKX_API_SECRET").empty()) {
    std::cerr << "Please set environment variable OKX_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("OKX_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable OKX_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Request request(Request::Operation::CREATE_ORDER, "okx", "BTC-USDT");
  request.appendParam({
      {"tdMode", "cross"},
      {"ccy", "USDT"},
  });
  session.sendRequest(request);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
