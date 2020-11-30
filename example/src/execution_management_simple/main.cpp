// #include "ccapi_cpp/ccapi_session.h"
// namespace ccapi {
// class ExampleLogger final: public Logger {
//  public:
//   void logMessage(Logger::Severity severity, std::thread::id threadId,
//                           std::chrono::system_clock::time_point time,
//                           std::string fileName, int lineNumber,
//                           std::string message) override {
//     lock.lock();
//     std::cout << threadId << ": [" << UtilTime::getISOTimestamp(time) << "] {"
//         << fileName << ":" << lineNumber << "} "
//         << Logger::severityToString(severity) << std::string(8, ' ') << message
//         << std::endl;
//     lock.unlock();
//   }
//  private:
//   std::mutex lock;
// };
// Logger* Logger::logger = nullptr;  // This line is needed.
// class MyEventHandler : public EventHandler {
//   bool processEvent(const Event& event, Session *session) override {
//     CCAPI_LOGGER_TRACE(toString(event));
//     return true;
//   }
// };
// } /* namespace ccapi */
// int main(int argc, char** argv) {
//   using namespace ccapi;  // NOLINT(build/namespaces)
//   ExampleLogger exampleLogger;
//   Logger::logger = &exampleLogger;
//   SessionOptions sessionOptions;
//   SessionConfigs sessionConfigs;
//   MyEventHandler eventHandler;
//   Session session(sessionOptions, sessionConfigs, &eventHandler);
//   Request::Operation operation = Request::Operation::CREATE_ORDER;
//   std::map<std::string, std::string> credential = {
//       {BINANCE_US_API_KEY, UtilSystem::getEnvAsString(BINANCE_US_API_KEY)},
//       {BINANCE_US_API_SECRET, UtilSystem::getEnvAsString(BINANCE_US_API_SECRET)}
//   };
//   CorrelationId correlationId("this is my correlation id");
//   Request request(operation, credential, CCAPI_EXCHANGE_NAME_BINANCE_US, "ETHUSD", correlationId);
//   request.setParam(CCAPI_EM_SIDE, CCAPI_EM_SIDE_BUY);
//   request.setParam(CCAPI_EM_QUANTITY, "0.04");
//   request.setParam(CCAPI_EM_LIMIT_PRICE, "300");
//   Queue<Event> eventQueue;
//   session.sendRequest(request, &eventQueue);
//   std::vector<Event> eventList = eventQueue.purge();
//   CCAPI_LOGGER_TRACE("eventList = "+toString(eventList));
//   return EXIT_SUCCESS;
// }
