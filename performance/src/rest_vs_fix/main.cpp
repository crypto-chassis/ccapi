#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::AUTHORIZATION_STATUS) {
      auto message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::AUTHORIZATION_SUCCESS) {
        this->fixReady = true;
      }
    }
    return true;
  }
  std::atomic<bool> fixReady{};
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::toString;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
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
  if (UtilSystem::getEnvAsString("MODE").empty()) {
    std::cerr << "Please set environment variable MODE. Allowed values: rest, fix." << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("NUM_REQUESTS").empty()) {
    std::cerr << "Please set environment variable NUM_REQUESTS. Allowed values: any positive integer." << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("TIME_SLEEP_MILLISECONDS_BETWEEN_REQUESTS").empty()) {
    std::cerr << "Please set environment variable TIME_SLEEP_MILLISECONDS_BETWEEN_REQUESTS. Allowed values: any positive "
                 "integer to avoid exchange request "
                 "rate limit."
              << std::endl;
    return EXIT_FAILURE;
  }
  const std::string mode = UtilSystem::getEnvAsString("MODE");
  std::cout << "Mode is " + mode << std::endl;
  const int numRequests = UtilSystem::getEnvAsInt("NUM_REQUESTS");
  std::cout << "Number of requests is " + std::to_string(numRequests) << std::endl;
  const int timeSleepMilliSeconds = UtilSystem::getEnvAsInt("TIME_SLEEP_MILLISECONDS_BETWEEN_REQUESTS");
  std::cout << "Time to sleep between requests is " + std::to_string(timeSleepMilliSeconds) + " milliseconds" << std::endl;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  double cumulativeRequestTimeMicroSeconds = 0;
  if (mode == "fix") {
    Subscription subscription("coinbase", "", "FIX", "", "same correlation id for subscription and request");
    session.subscribeByFix(subscription);
    while (!eventHandler.fixReady.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  for (int i = 0; i < numRequests + 1; i++) {
    auto start = UtilTime::now();
    if (mode == "rest") {
      Request request(Request::Operation::CREATE_ORDER, "coinbase", "BTC-USD");
      request.appendParam({
          {"SIDE", "BUY"},
          {"QUANTITY", "0.001"},
          {"LIMIT_PRICE", "20000"},
      });
      session.sendRequest(request);
    } else if (mode == "fix") {
      Request request(Request::Operation::FIX, "coinbase", "", "same correlation id for subscription and request");
      request.appendParamFix({
          {35, "D"},
          {11, "6d4eb0fb-2229-469f-873e-557dd78ac11e"},
          {55, "BTC-USD"},
          {54, "1"},
          {44, "20000"},
          {38, "0.001"},
          {40, "2"},
          {59, "1"},
      });
      session.sendRequestByFix(request);
    }
    auto end = UtilTime::now();
    // skip the first one because REST API needs to create a tcp connection for the first time
    if (i > 0) {
      std::cout << "Sent " + std::to_string(i) + "th request at time " + UtilTime::getISOTimestamp(end) << std::endl;
      cumulativeRequestTimeMicroSeconds += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(timeSleepMilliSeconds));
  }
  std::cout << "Average request time is " + std::to_string(cumulativeRequestTimeMicroSeconds / numRequests) + " microseconds" << std::endl;
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
