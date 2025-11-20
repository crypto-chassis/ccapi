#include "ccapi_cpp/ccapi_event_handler.h"
#include "my_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session*) override { std::cout << "Received an event:\n" + event.toPrettyString(2, 2) << std::endl; }

  void setMySessionPtr(MySession* mySessionPtr) { this->mySessionPtr = mySessionPtr; }

 private:
  MySession* mySessionPtr{nullptr};
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::MySession;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;

int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  MySession mySession(sessionOptions, sessionConfigs, &eventHandler);
  eventHandler.setMySessionPtr(&mySession);
  Subscription subscription("okx", "BTC-USDT", "MARKET_DEPTH");
  mySession.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  mySession.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
