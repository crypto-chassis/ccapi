#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* sessionPtr) override {
    if (event.getType() == Event::Type::HEARTBEAT) {
      std::cout << "Received an event of type HEARTBEAT:\n" + event.toPrettyString(2, 2) << std::endl;
    }
  }
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;

int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("", "", "HEARTBEAT", "HEARTBEAT_INTERVAL_MILLISECONDS=1000");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
