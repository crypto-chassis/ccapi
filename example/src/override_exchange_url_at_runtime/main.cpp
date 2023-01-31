#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::SESSION_STATUS) {
      for (const auto& message : event.getMessageList()) {
        if (message.getType() == Message::Type::SESSION_CONNECTION_UP) {
          for (const auto& element : message.getElementList()) {
            const std::map<std::string, std::string>& elementNameValueMap = element.getNameValueMap();
            std::cout << "Connected to " + toString(elementNameValueMap.at("CONNECTION_URL")) << std::endl;
          }
        }
      }
    }
    return true;
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::toString;
int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  std::map<std::string, std::string> urlWebsocketBase = sessionConfigs.getUrlWebsocketBase();
  urlWebsocketBase["okx"] = "wss://wsaws.okx.com:8443";
  sessionConfigs.setUrlWebsocketBase(urlWebsocketBase);
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("okx", "BTC-USDT", "MARKET_DEPTH");
  session.subscribe(subscription);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
