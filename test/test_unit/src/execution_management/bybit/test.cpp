#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_bybit.h"

// clang-format on

namespace ccapi {

class ExecutionManagementServiceBybitTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;

  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceBybit>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), &this->serviceContext);
    this->credential = {
        {CCAPI_BYBIT_API_KEY, "xxxxxxxxxxxxxxxxxx"},
        {CCAPI_BYBIT_API_SECRET, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }

  ServiceContext serviceContext;
  std::shared_ptr<ExecutionManagementServiceBybit> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = std::string(req.base().at("X-BAPI-TIMESTAMP"));
  preSignedText += std::string(req.base().at("X-BAPI-API-KEY"));
  preSignedText += std::string(req.base().at("X-BAPI-RECV-WINDOW"));
  preSignedText += req.body();
  EXPECT_EQ(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true), std::string(req.base().at("X-BAPI-SIGN")));
}

TEST_F(ExecutionManagementServiceBybitTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BYBIT, "BTCUSDT", "foo", this->credential);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/v5/order/create");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["category"].GetString()), "spot");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTCUSDT");
  EXPECT_FALSE(document.HasMember("isLeverage"));
  verifySignature(req, this->credential.at(CCAPI_BYBIT_API_SECRET));
}

TEST_F(ExecutionManagementServiceBybitTest, convertRequestCreateOrderSpotMargin) {
  for (const auto& marginType : {CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN, CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN}) {
    Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BYBIT, "BTCUSDT", "foo", this->credential);
    request.setMarginType(marginType);
    request.appendParam({
        {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
        {CCAPI_EM_ORDER_QUANTITY, "0.1"},
        {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
    });
    auto req = this->service->convertRequest(request, this->now);
    EXPECT_EQ(req.method(), http::verb::post);
    EXPECT_EQ(req.target(), "/v5/order/create");
    rj::Document document;
    document.Parse(req.body().c_str());
    EXPECT_EQ(std::string(document["category"].GetString()), "spot");
    // Bybit enables spot margin through isLeverage; both cross and isolated margin use the same flag
    EXPECT_EQ(document["isLeverage"].GetInt(), 1);
    verifySignature(req, this->credential.at(CCAPI_BYBIT_API_SECRET));
  }
}

TEST_F(ExecutionManagementServiceBybitTest, convertRequestCreateOrderSpotMarginDerivativesCategoryUnaffected) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BYBIT, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
      {"category", "linear"},
  });
  auto req = this->service->convertRequest(request, this->now);
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["category"].GetString()), "linear");
  EXPECT_FALSE(document.HasMember("isLeverage"));
}

TEST_F(ExecutionManagementServiceBybitTest, convertRequestCreateOrderExplicitIsLeverageWins) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BYBIT, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
      {"isLeverage", "0"},
  });
  auto req = this->service->convertRequest(request, this->now);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(document["isLeverage"].GetInt(), 0);
}

} /* namespace ccapi */
#endif
#endif
