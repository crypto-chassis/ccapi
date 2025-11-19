#include <atomic>

#include "ccapi_cpp/ccapi_session.h"

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  MyEventHandler(const std::string& symbol, const std::string& side, const std::string& quantity, const std::string& price, int clientOrderIdLength,
                 bool cancelByClientOrderId, int numOrders, bool byWebsocket, const std::string& websocketOrderEntrySubscriptionCorrelationId)
      : symbol(symbol),
        side(side),
        quantity(quantity),
        price(price),
        clientOrderIdLength(clientOrderIdLength),
        cancelByClientOrderId(cancelByClientOrderId),
        numOrders(numOrders),
        byWebsocket(byWebsocket),
        websocketOrderEntrySubscriptionCorrelationId(websocketOrderEntrySubscriptionCorrelationId) {}

  void processEvent(const Event& event, Session* sessionPtr) override {
    if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
      const auto& message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::SUBSCRIPTION_STARTED) {
        Request request(Request::Operation::GET_OPEN_ORDERS, "okx", this->symbol);
        sessionPtr->sendRequest(request);
      }
    } else if (event.getType() == Event::Type::RESPONSE) {
      const auto& message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::GET_OPEN_ORDERS) {
        for (int i = 0; i < this->numOrders; ++i) {
          sessionPtr->setTimer(
              std::to_string(i), 100 * i,
              [](const boost::system::error_code&) {
                std::cout << std::string("Timer error handler is triggered at ") + UtilTime::getISOTimestamp(UtilTime::now()) << std::endl;
              },
              [this, sessionPtr]() {
                Request request(Request::Operation::CREATE_ORDER, "okx", this->symbol);
                const auto& clientOrderId = UtilString::generateRandomString(this->clientOrderIdLength);
                request.appendParam({
                    {"SIDE", UtilString::toUpper(this->side)},
                    {"LIMIT_PRICE", this->price},
                    {"QUANTITY", this->quantity},
                    {"CLIENT_ORDER_ID", clientOrderId},
                });
                this->orderCreateTimes.emplace(clientOrderId, UtilTime::now());
                if (this->byWebsocket) {
                  sessionPtr->sendRequestByWebsocket(this->websocketOrderEntrySubscriptionCorrelationId, request);
                } else {
                  sessionPtr->sendRequest(request);
                }
              });
        }
      }
    } else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      const auto& message = event.getMessageList().at(0);
      if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE) {
        const auto& now = UtilTime::now();
        const auto& element = message.getElementList().front();
        const auto& orderId = element.getValue("ORDER_ID");
        const auto& clientOrderId = element.getValue("CLIENT_ORDER_ID");
        const auto& status = element.getValue("STATUS");
        if (status == "live") {
          this->orderCreateLatencies.push_back(now - this->orderCreateTimes.at(clientOrderId));
          this->orderCreateTimes.erase(clientOrderId);
          Request request(Request::Operation::CANCEL_ORDER, "okx", this->symbol);
          if (this->cancelByClientOrderId) {
            request.appendParam({
                {"CLIENT_ORDER_ID", clientOrderId},
            });
          } else {
            request.appendParam({
                {"ORDER_ID", orderId},
            });
          }
          this->orderCancelTimes.emplace(clientOrderId, now);
          if (this->byWebsocket) {
            sessionPtr->sendRequestByWebsocket(this->websocketOrderEntrySubscriptionCorrelationId, request);
          } else {
            sessionPtr->sendRequest(request);
          }
        } else if (status == "canceled") {
          this->orderCancelLatencies.push_back(now - this->orderCancelTimes.at(clientOrderId));
          this->orderCancelTimes.erase(clientOrderId);
          ++this->numCanceledOrders;
          if (this->numCanceledOrders == this->numOrders) {
            done = true;
          }
        }
      }
    }
  }

  std::string symbol;
  std::string side;
  std::string quantity;
  std::string price;
  int clientOrderIdLength{};
  bool cancelByClientOrderId{};
  int numOrders{};
  bool byWebsocket{};

  std::string websocketOrderEntrySubscriptionCorrelationId;

  std::map<std::string, TimePoint> orderCreateTimes;
  std::vector<std::chrono::nanoseconds> orderCreateLatencies;
  std::map<std::string, TimePoint> orderCancelTimes;
  std::vector<std::chrono::nanoseconds> orderCancelLatencies;

  int numCanceledOrders{};
  std::atomic<bool> done{};
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;

int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("OKX_API_KEY").empty()) {
    std::cerr << "Please set environment variable OKX_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("OKX_API_SECRET").empty()) {
    std::cerr << "Please set environment variable OKX_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("OKX_API_PASSPHRASE").empty()) {
    std::cerr << "Please set environment variable OKX_API_PASSPHRASE" << std::endl;
    return EXIT_FAILURE;
  }
  const auto& symbol = UtilSystem::getEnvAsString("SYMBOL");
  const auto& side = UtilSystem::getEnvAsString("SIDE");
  const auto& quantity = UtilSystem::getEnvAsString("QUANTITY");
  const auto& price = UtilSystem::getEnvAsString("PRICE");
  const auto& clientOrderIdLength = UtilSystem::getEnvAsInt("CLIENT_ORDER_ID_LENGTH", 4);
  const auto& cancelByClientOrderId = UtilSystem::getEnvAsBool("CANCEL_BY_CLIENT_ORDER_ID");
  const auto& numOrders = UtilSystem::getEnvAsInt("NUM_ORDERS", 10);
  const auto& byWebsocket = UtilSystem::getEnvAsBool("BY_WEBSOCKET");
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;

  const auto& okx_rest_fast_url = ccapi::UtilSystem::getEnvAsString("OKX_REST_FAST_URL");
  const auto& okx_websocket_fast_url = ccapi::UtilSystem::getEnvAsString("OKX_WEBSOCKET_FAST_URL");
  auto url_rest_base = sessionConfigs.getUrlRestBase();
  if (!okx_rest_fast_url.empty()) {
    url_rest_base.at("okx") = okx_rest_fast_url;
  }
  sessionConfigs.setUrlRestBase(url_rest_base);
  std::map<std::string, std::string> url_websocket_base = sessionConfigs.getUrlWebsocketBase();
  if (!okx_websocket_fast_url.empty()) {
    url_websocket_base.at("okx") = okx_websocket_fast_url;
  }
  sessionConfigs.setUrlWebsocketBase(url_websocket_base);

  std::string websocketOrderEntrySubscriptionCorrelationId("any");
  MyEventHandler eventHandler(symbol, side, quantity, price, clientOrderIdLength, cancelByClientOrderId, numOrders, byWebsocket,
                              websocketOrderEntrySubscriptionCorrelationId);
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  Subscription subscription("okx", symbol, "ORDER_UPDATE", "", websocketOrderEntrySubscriptionCorrelationId);
  session.subscribe(subscription);
  while (!eventHandler.done) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  session.stop();
  double avgCreateLatencyMs =
      std::accumulate(eventHandler.orderCreateLatencies.begin(), eventHandler.orderCreateLatencies.end(), std::chrono::nanoseconds{0}).count() / 1e6 /
      eventHandler.orderCreateLatencies.size();
  std::cout << "avgCreateLatencyMs = " << avgCreateLatencyMs << " for " << eventHandler.orderCreateLatencies.size() << " orders" << std::endl;
  double avgCancelLatencyMs =
      std::accumulate(eventHandler.orderCancelLatencies.begin(), eventHandler.orderCancelLatencies.end(), std::chrono::nanoseconds{0}).count() / 1e6 /
      eventHandler.orderCancelLatencies.size();
  std::cout << "avgCancelLatencyMs = " << avgCancelLatencyMs << " for " << eventHandler.orderCancelLatencies.size() << " orders" << std::endl;
  return EXIT_SUCCESS;
}
