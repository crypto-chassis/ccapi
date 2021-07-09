#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
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
int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  {
    Request request(Request::Operation::GENERIC_PUBLIC_REQUEST, "binance", "", "Check Server Time");
    request.appendParam({
        {"HTTP_METHOD", "GET"},
        {"HTTP_PATH", "/api/v3/time"},
    });
    session.sendRequest(request);
  }
  std::this_thread::sleep_for(std::chrono::seconds(10));
  {
    Request request(Request::Operation::GENERIC_PUBLIC_REQUEST, "binance", "", "Old Trade Lookup",
                    {
                        {"BINANCE_API_KEY", UtilSystem::getEnvAsString("BINANCE_API_KEY")},
                    });
    request.appendParam({
        {"HTTP_METHOD", "GET"},
        {"HTTP_PATH", "/api/v3/historicalTrades"},
        {"HTTP_QUERY_STRING", "symbol=BTCUSDT"},
    });
    session.sendRequest(request);
  }
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
