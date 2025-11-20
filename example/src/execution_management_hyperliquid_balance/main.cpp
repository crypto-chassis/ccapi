#include <array>
#include <iomanip>
#include <random>
#include <sstream>

#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {

class MyLogger final : public Logger {
 public:
  void logMessage(const std::string& severity, const std::string& threadId, const std::string& timeISO, const std::string& fileName,
                  const std::string& lineNumber, const std::string& message) override {
    std::lock_guard<std::mutex> lock(m);
    std::cout << threadId << ": [" << timeISO << "] {" << fileName << ":" << lineNumber << "} " << severity << std::string(8, ' ') << message << std::endl;
  }

 private:
  std::mutex m;
};

MyLogger myLogger;
Logger* Logger::logger = &myLogger;
class MyEventHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* session) override {
    if (event.getType() == Event::Type::RESPONSE) {
      std::cout << "Received an event of type RESPONSE:\n" + event.toPrettyString(2, 2) << std::endl;
    }
  }
};
} /* namespace ccapi */
using ::ccapi::MyEventHandler;
using ::ccapi::Request;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::UtilSystem;
using ::ccapi::UtilTime;
int main(int argc, char** argv) {
  if (UtilSystem::getEnvAsString("HYPERLIQUID_API_WALLET_ADDRESS").empty()) {
    std::cerr << "Please set environment variable HYPERLIQUID_API_WALLET_ADDRESS" << std::endl;
    return EXIT_FAILURE;
  }
  if (UtilSystem::getEnvAsString("HYPERLIQUID_API_PRIVATE_KEY").empty()) {
    std::cerr << "Please set environment variable HYPERLIQUID_API_PRIVATE_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  auto waitForResponses = [](int seconds = 2) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
  };
  auto generateCloid = []() {
    std::array<unsigned char, 16> buffer{};
    std::random_device rd;
    for (auto& byte : buffer) {
      byte = static_cast<unsigned char>(rd());
    }
    std::ostringstream oss;
    oss << "0x";
    for (const auto& byte : buffer) {
      oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
  };

  std::cout << "Requesting Hyperliquid account balances..." << std::endl;
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_HYPERLIQUID);
  session.sendRequest(request);
  waitForResponses();

  std::cout << "Submitting Hyperliquid noop action (signing smoke test)..." << std::endl;
  Request noopRequest(Request::Operation::GENERIC_PRIVATE_REQUEST, CCAPI_EXCHANGE_NAME_HYPERLIQUID);
  noopRequest.appendParam({{"action", "noop"}});
  session.sendRequest(noopRequest);
  waitForResponses();

  // Submit a small test order so we can exercise Hyperliquid signing end-to-end.
  std::string assetId = UtilSystem::getEnvAsString("HYPERLIQUID_TEST_ASSET_ID");
  if (assetId.empty()) {
    assetId = "0";  // asset id 0 corresponds to BTC on Hyperliquid
  }

  std::string clientOrderId = generateCloid();

  std::cout << "Creating limit order on assetId=" << assetId << " with cloid=" << clientOrderId << std::endl;
  Request orderRequest(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_HYPERLIQUID, assetId);
  orderRequest.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.00052"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "90000"},
      {CCAPI_EM_CLIENT_ORDER_ID, clientOrderId},
  });
  session.sendRequest(orderRequest);
  waitForResponses();

  std::cout << "Fetching order status via client order id..." << std::endl;
  Request getOrderRequest(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HYPERLIQUID);
  getOrderRequest.appendParam({{CCAPI_EM_CLIENT_ORDER_ID, clientOrderId}});
  session.sendRequest(getOrderRequest);
  waitForResponses();

  std::cout << "Fetching open orders snapshot..." << std::endl;
  Request getOpenOrdersRequest(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HYPERLIQUID);
  session.sendRequest(getOpenOrdersRequest);
  waitForResponses();

  std::cout << "Cancelling order with cloid=" << clientOrderId << std::endl;
  Request cancelOrderRequest(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HYPERLIQUID, assetId);
  cancelOrderRequest.appendParam({{CCAPI_EM_CLIENT_ORDER_ID, clientOrderId}});
  session.sendRequest(cancelOrderRequest);
  waitForResponses();

  std::cout << "Requesting account positions..." << std::endl;
  Request getPositionsRequest(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_HYPERLIQUID);
  session.sendRequest(getPositionsRequest);

  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}

