#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {

    class MyLogger final: public Logger {
    public:
        void logMessage(std::string severity,
                        std::string threadId,
                        std::string timeISO,
                        std::string fileName,
                        std::string lineNumber,
                        std::string message) override {
            std::lock_guard<std::mutex> lock(m);
            std::cout << threadId << ": [" << timeISO << "] {"
                      << fileName << ":" << lineNumber << "} "
                      << severity << std::string(8, ' ') << message
                      << std::endl;
        }
    private:
        std::mutex m;
    };
    MyLogger myLogger;
    Logger* Logger::logger = &myLogger;

    class MyEventHandler : public EventHandler {
    public:
        bool processEvent(const Event& event, Session *session) override {
            std::cout << event.toString() << std::endl;
            return true;
        }
    };


}
using ::ccapi::SessionOptions;
using ::ccapi::SessionConfigs;
using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::Request;

int main(int argc, char **argv) {

    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;

    Session session(sessionOptions, sessionConfigs, &eventHandler);

    std::map<std::string, std::string> myCredentials = {
            { CCAPI_FTX_API_KEY, "" },
            { CCAPI_FTX_API_SECRET, "" }
    };


    Request requestBuy(Request::Operation::CREATE_ORDER, "ftx", "DOGE-PERP", "", myCredentials);
    requestBuy.appendParam({
                                   {"SIDE", "BUY"},
                                   {"QUANTITY", "5"},
                                   {"LIMIT_PRICE", "0.03"}
                           });
    session.sendRequest(requestBuy);
    std::cout << "Creating orders" << std::endl;
    
    // timeout is 10 seconds, lets wait for 13
    std::this_thread::sleep_for(std::chrono::seconds(13));

    Request requestBuyTwo(Request::Operation::CREATE_ORDER, "ftx", "DOGE-PERP", "", myCredentials);
    requestBuyTwo.appendParam({
                                   {"SIDE", "BUY"},
                                   {"QUANTITY", "5"},
                                   {"LIMIT_PRICE", "0.03"}
                           });
    session.sendRequest(requestBuyTwo);
    std::cout << "Creating orders" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.stop();

    std::cout << "Bye" << std::endl;
    return EXIT_SUCCESS;
}
