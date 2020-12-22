#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session *session) override {
    std::cout << "Received an event: " + toString(event) << std::endl;
    return true;
  }
};
} /* namespace ccapi */
int main(int argc, char** argv) {
  using namespace ccapi;  // NOLINT(build/namespaces)
  std::vector<std::string> modeList = { "create_order", "cancel_order", "get_order", "get_open_orders",
      "cancel_open_orders" };
  if (argc < 2 || std::find(modeList.begin(), modeList.end(), argv[1]) == modeList.end()) {
    std::cerr << "Please provide the first command line argument from this list: " + toString(modeList) << std::endl;
    return EXIT_FAILURE;
  }
  std::string mode(argv[1]);
  std::string key = UtilSystem::getEnvAsString("COINBASE_API_KEY");
  if (key.empty()) {
    std::cerr << "Please set environment variable COINBASE_API_KEY" << std::endl;
    return EXIT_FAILURE;
  }
  std::string secret = UtilSystem::getEnvAsString("COINBASE_API_SECRET");
  if (secret.empty()) {
    std::cerr << "Please set environment variable COINBASE_API_SECRET" << std::endl;
    return EXIT_FAILURE;
  }
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);
  if (mode == "create_order") {
    if (argc != 6) {
      std::cerr << "Usage: <program name> create_order <symbol> <buy or sell> <order quantity> <limit price>\n"
                << "Example:\n" << "    main create_order BTCUSD buy 0.0005 20000" << std::endl;
      session.stop();
      return EXIT_FAILURE;
    }
    Request request(Request::Operation::CREATE_ORDER, "coinbase", argv[2]);
    request.appendParam({
      {"SIDE", strcmp(argv[3], "buy") == 0 ? "BUY" : "SELL"},
      {"QUANTITY", argv[4]},
      {"LIMIT_PRICE", argv[5]}
    });
    session.sendRequest(request);
  } else if (mode == "cancel_order") {
    if (argc != 4) {
      std::cerr << "Usage: <program name> cancel_order <symbol> <order id>\n" << "Example:\n"
                << "    main cancel_order BTCUSD 4" << std::endl;
      session.stop();
      return EXIT_FAILURE;
    }
    Request request(Request::Operation::CANCEL_ORDER, "coinbase", argv[2]);
    request.appendParam({
      {"ORDER_ID", argv[3]}
    });
    session.sendRequest(request);
  } else if (mode == "get_order") {
    if (argc != 4) {
      std::cerr << "Usage: <program name> get_order <symbol> <order id>\n" << "Example:\n"
                << "    main get_order BTCUSD 4" << std::endl;
      session.stop();
      return EXIT_FAILURE;
    }
    Request request(Request::Operation::GET_ORDER, "coinbase", argv[2]);
    request.appendParam({
      {"ORDER_ID", argv[3]}
    });
    session.sendRequest(request);
  } else if (mode == "get_open_orders") {
    if (argc != 3) {
      std::cerr << "Usage: <program name> get_open_orders <symbol>\n" << "Example:\n"
                << "    main get_open_orders BTCUSD" << std::endl;
      session.stop();
      return EXIT_FAILURE;
    }
    Request request(Request::Operation::GET_OPEN_ORDERS, "coinbase", argv[2]);
    session.sendRequest(request);
  } else if (mode == "cancel_open_orders") {
    if (argc != 3) {
      std::cerr << "Usage: <program name> cancel_open_orders <symbol>\n" << "Example:\n"
                << "    main cancel_open_orders BTCUSD" << std::endl;
      session.stop();
      return EXIT_FAILURE;
    }
    Request request(Request::Operation::CANCEL_OPEN_ORDERS, "coinbase", argv[2]);
    session.sendRequest(request);
  }
  std::this_thread::sleep_for(std::chrono::seconds(10));
  session.stop();
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
