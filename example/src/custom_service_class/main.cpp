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
class ExecutionManagementServiceCoinbaseCustom : public ExecutionManagementServiceCoinbase {
 public:
  ExecutionManagementServiceCoinbaseCustom(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                           ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceCoinbase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}

 protected:
  void convertReqCustom(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                        const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::CUSTOM: {
        if (request.getParamList().at(0).at("CUSTOM_OPERATION") == "LIST_PROFILES") {
          req.method(http::verb::get);
          req.target("/profiles");
          this->signRequest(req, "", credential);
        }
      } break;
      default:
        ExecutionManagementServiceCoinbase::convertReqCustom(req, request, now, symbolId, credential);
    }
  }
  void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    switch (request.getOperation()) {
      case Request::Operation::CUSTOM: {
        if (request.getParamList().at(0).at("CUSTOM_OPERATION") == "LIST_PROFILES") {
          rj::Document document;
          document.Parse(textMessage.c_str());
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({request.getCorrelationId()});
          message.setType(Message::Type::CUSTOM);
          std::vector<Element> elementList;
          for (const auto& x : document.GetArray()) {
            Element element;
            element.insert("PROFILE_ID", std::string(x["id"].GetString()));
            elementList.push_back(element);
          }
          message.setElementList(elementList);
          std::vector<Message> messageList;
          messageList.push_back(std::move(message));
          Event event;
          event.setType(Event::Type::RESPONSE);
          event.addMessages(messageList);
          this->eventHandler(event);
        }
      } break;
      default:
        ExecutionManagementServiceCoinbase::processSuccessfulTextMessage(request, textMessage, timeReceived);
    }
  }
};
} /* namespace ccapi */
using ::ccapi::ExecutionManagementServiceCoinbaseCustom;
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::UtilSystem;
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
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  session.serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_COINBASE] =
      std::make_shared<ExecutionManagementServiceCoinbaseCustom>(session.internalEventHandler, session.sessionOptions, session.sessionConfigs,
                                                                 session.serviceContextPtr);
  Request request(Request::Operation::CUSTOM, "coinbase");
  request.serviceName = CCAPI_EXECUTION_MANAGEMENT;
  request.appendParam({
      {"CUSTOM_OPERATION", "LIST_PROFILES"},
  });
  session.sendRequest(request);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
