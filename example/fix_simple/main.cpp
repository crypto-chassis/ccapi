#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  MyEventHandler(const std::string& fixSubscriptionCorrelationId) : fixSubscriptionCorrelationId(fixSubscriptionCorrelationId) {}

  void processEvent(const Event& event, Session* sessionPtr) override {
    std::cout << "Received an event:\n" + event.toPrettyString(2, 2) << std::endl;
    if (!willSendRequest) {
      sessionPtr->setTimer("id", 1000, nullptr, [this, sessionPtr]() {
        Request request(Request::Operation::FIX, "binance");
        request.appendFixParam({
            {35, "D"},
            {11, request.generateNextClientOrderId()},
            {55, "BTCUSDT"},
            {54, "1"},
            {44, "100000"},
            {38, "0.0001"},
            {40, "2"},
            {59, "1"},
        });
        sessionPtr->sendRequestByFix(this->fixSubscriptionCorrelationId, request);
      });
      willSendRequest = true;
    }
  }

 private:
  std::string fixSubscriptionCorrelationId;
  bool willSendRequest{};
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;

int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("BINANCE_FIX_API_KEY").empty()) {
    std::cerr << "Please set environment variable BINANCE_FIX_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("BINANCE_FIX_API_PRIVATE_KEY_PATH").empty()) {
    std::cerr << "Please set environment variable BINANCE_FIX_API_PRIVATE_KEY_PATH" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  std::string fixSubscriptionCorrelationId("any");
  MyEventHandler eventHandler(fixSubscriptionCorrelationId);
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("binance", "", "FIX", "", fixSubscriptionCorrelationId);
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
