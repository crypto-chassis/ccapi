#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_binance.h"

// clang-format on

namespace ccapi {

class ExecutionManagementServiceBinanceTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;

  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceBinance>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), &this->serviceContext);
    this->credential = {
        {CCAPI_BINANCE_API_KEY, "vmPUZE6mv9SD5VNHk4HlWFsOr6aKE2zvsw0MuIgwCIPy6utIco14y7Ju91duEh8A"},
        {CCAPI_BINANCE_API_SECRET, "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j"},
    };
    this->timestamp = 1499827319559;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp);
  }

  ServiceContext serviceContext;
  std::shared_ptr<ExecutionManagementServiceBinance> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifySignature(const std::string& paramString, const std::string& apiSecret) {
  auto pos = paramString.find_last_of("&");
  auto paramStringWithoutSignature = paramString.substr(0, pos);
  auto signature = paramString.substr(pos + 11, paramString.length() - pos - 1);
  EXPECT_EQ(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, paramStringWithoutSignature, true), signature);
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BINANCE, "BTCUSDT", "foo", this->credential);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.001"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "20000"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_FALSE(paramMap.find("isIsolated") != paramMap.end());
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestCreateOrderIsolatedMargin) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BINANCE, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.001"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "20000"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/sapi/v1/margin/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("isIsolated"), "TRUE");
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSDT");
  EXPECT_EQ(paramMap.at("type"), "LIMIT");
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestCreateOrderCrossMargin) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BINANCE, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_SELL},
      {CCAPI_EM_ORDER_QUANTITY, "0.001"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "20000"},
  });
  auto req = this->service->convertRequest(request, this->now);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/sapi/v1/margin/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  // cross margin must not send the isolated-only flag
  EXPECT_TRUE(paramMap.find("isIsolated") == paramMap.end());
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestCancelOrderIsolatedMargin) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BINANCE, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_ID, "28"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/sapi/v1/margin/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("isIsolated"), "TRUE");
  EXPECT_EQ(paramMap.at("orderId"), "28");
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestGetOpenOrdersIsolatedMargin) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BINANCE, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/sapi/v1/margin/openOrders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("isIsolated"), "TRUE");
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestGetAccountBalancesCrossMargin) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BINANCE, "", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  auto req = this->service->convertRequest(request, this->now);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/sapi/v1/margin/account");
}

TEST_F(ExecutionManagementServiceBinanceTest, convertRequestAmendOrderMarginRejected) {
  for (const auto& operation : {Request::Operation::AMEND_ORDER, Request::Operation::AMEND_ORDER_KEEP_PRIORITY}) {
    Request request(operation, CCAPI_EXCHANGE_NAME_BINANCE, "BTCUSDT", "foo", this->credential);
    request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
    request.appendParam({{CCAPI_EM_ORDER_ID, "28"}, {CCAPI_EM_ORDER_QUANTITY, "0.002"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "20001"}});
    EXPECT_THROW(this->service->convertRequest(request, this->now), std::runtime_error);
  }
}

TEST_F(ExecutionManagementServiceBinanceTest, convertTextMessageToMessageRestGetAccountBalancesCrossMargin) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BINANCE, "", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  std::string textMessage =
      R"(
        {
          "borrowEnabled": true,
          "marginLevel": "11.64405625",
          "totalAssetOfBtc": "6.82728457",
          "totalLiabilityOfBtc": "0.58633215",
          "totalNetAssetOfBtc": "6.24095242",
          "tradeEnabled": true,
          "transferEnabled": true,
          "userAssets": [
            {
              "asset": "BTC",
              "borrowed": "0.00000000",
              "free": "0.00499500",
              "interest": "0.00000000",
              "locked": "0.00000000",
              "netAsset": "0.00499500"
            }
          ]
        }
      )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  auto message = messageList.at(0);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_ASSET), "BTC");
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_QUANTITY_TOTAL), "0.004995");
}

} /* namespace ccapi */
#endif
#endif
