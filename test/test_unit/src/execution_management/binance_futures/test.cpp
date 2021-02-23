#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES
#include "gtest/gtest.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_futures.h"
namespace ccapi {
class ExecutionManagementServiceBinanceFuturesTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceBinanceFutures>([](Event& event){}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {
       { CCAPI_BINANCE_FUTURES_API_KEY, "vmPUZE6mv9SD5VNHk4HlWFsOr6aKE2zvsw0MuIgwCIPy6utIco14y7Ju91duEh8A" },
       { CCAPI_BINANCE_FUTURES_API_SECRET, "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j" }
    };
    this->timestamp = 1499827319559;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp);
  }
  std::shared_ptr<ExecutionManagementServiceBinanceFutures> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

TEST_F(ExecutionManagementServiceBinanceFuturesTest, processSuccessfulTextMessageGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_FUTURES, "BTCUSDT", "foo", this->credential);
  std::string textMessage =
  R"(
  {
    "avgPrice": "0.00000",
    "clientOrderId": "abc",
    "cumQuote": "0.01",
    "executedQty": "0",
    "orderId": 1917641,
    "origQty": "0.40",
    "origType": "TRAILING_STOP_MARKET",
    "price": "0",
    "reduceOnly": false,
    "side": "BUY",
    "positionSide": "SHORT",
    "status": "NEW",
    "stopPrice": "9300",
    "closePosition": false,
    "symbol": "BTCUSDT",
    "time": 1579276756075,
    "timeInForce": "GTC",
    "type": "TRAILING_STOP_MARKET",
    "activatePrice": "9020",
    "priceRate": "0.3",
    "updateTime": 1579276756075,
    "workingType": "CONTRACT_PRICE",
    "priceProtect": false
  }
  )";
  auto messageList = this->service->processSuccessfulTextMessage(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  auto message = messageList.at(0);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "0.01");
}

} /* namespace ccapi */
#endif
#endif
