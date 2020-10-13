#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
class ExampleLogger final: public Logger {
 public:
  virtual void logMessage(Logger::Severity severity, std::thread::id threadId,
                          std::chrono::system_clock::time_point time,
                          std::string fileName, int lineNumber,
                          std::string message) override {
    lock.lock();
    std::cout << threadId << ": [" << UtilTime::getISOTimestamp(time) << "] {"
        << fileName << ":" << lineNumber << "} "
        << Logger::severityToString(severity) << std::string(8, ' ') << message
        << std::endl;
    lock.unlock();
  }
 private:
  std::mutex lock;
};
Logger* Logger::logger = 0;  // This line is needed.
class MyEventHandler : public EventHandler {
  bool processEvent(const Event& event, Session *session) override {
    CCAPI_LOGGER_TRACE(toString(event));
    return true;
  }
};
} /* namespace ccapi */
int main(int argc, char** argv)
{
  using namespace ccapi;  // NOLINT(build/namespaces)
  ExampleLogger exampleLogger;
  Logger::logger = &exampleLogger;
  SessionOptions sessionOptions;
  sessionOptions.enableOneHttpConnectionPerRequest = false;
  std::string instrument = "my cool naming";
  std::string symbol = "BTC-USD";
  // Coinbase names a trading pair using upper case concatenated by dash
  // Since symbol normalization is a tedious task, you can choose to use a reference file at https://marketdata-e0323a9039add2978bf5b49550572c7c-public.s3.amazonaws.com/supported_exchange_instrument_subscription_data.csv.gz which we frequently update.
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  CCAPI_LOGGER_TRACE("before session created");
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  CCAPI_LOGGER_TRACE("session created");
  Request::Operation operation = Request::Operation::CREATE_ORDER;
  std::map<std::string, std::string> credential = {
      {BINANCE_US_API_KEY, UtilSystem::getEnvAsString(BINANCE_US_API_KEY)},
      {BINANCE_US_API_SECRET, UtilSystem::getEnvAsString(BINANCE_US_API_SECRET)}};
  CorrelationId correlationId("this is my correlation id");
//  std::string topic = std::string("/") + CCAPI_EXCHANGE_NAME_COINBASE + "/" + instrument;
  Request request(operation, credential,
                  CCAPI_EXCHANGE_NAME_BINANCE_US,"ETHUSD",
                  correlationId);
//  request.setParam(CCAPI_EM_INSTRUMENT, "ethusdt");
  request.setParam(CCAPI_EM_SIDE, CCAPI_EM_SIDE_BUY);
  request.setParam(CCAPI_EM_QUANTITY, "0.04");
  request.setParam(CCAPI_EM_LIMIT_PRICE, "300");
  Queue<Event> eventQueue;
  for(int i=1; i<=1; i++){

    session.sendRequest(request);
  }


//  CCAPI_LOGGER_TRACE("about to sleep");
//  std::this_thread::sleep_for (std::chrono::seconds(5));
  return EXIT_SUCCESS;
}
