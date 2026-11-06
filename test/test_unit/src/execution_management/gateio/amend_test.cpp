// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio_perpetual_futures.h"
// clang-format on

namespace ccapi {

class ExecutionManagementServiceGateioAmendTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      this->service = std::make_shared<ExecutionManagementServiceGateioPerpetualFutures>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                                         &this->serviceContext);
      this->exchange = CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES;
      this->credential = {{CCAPI_GATEIO_PERPETUAL_FUTURES_API_KEY, "test-key"}, {CCAPI_GATEIO_PERPETUAL_FUTURES_API_SECRET, "test-secret"}};
    } else {
      this->service =
          std::make_shared<ExecutionManagementServiceGateio>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), &this->serviceContext);
      this->exchange = CCAPI_EXCHANGE_NAME_GATEIO;
      this->credential = {{CCAPI_GATEIO_API_KEY, "test-key"}, {CCAPI_GATEIO_API_SECRET, "test-secret"}};
    }
    this->now = UtilTime::makeTimePointFromMilliseconds(1499827319000LL);
  }

  void verifyRequest(const http::request<http::string_body>& req, const std::string& id, const std::string& settle = "usdt") {
    EXPECT_EQ(req.method(), GetParam() ? http::verb::put : http::verb::patch);
    const std::string path = GetParam() ? "/api/v4/futures/" + settle + "/orders/" + id : "/api/v4/spot/orders/" + id;
    EXPECT_EQ(req.target(), path);
    EXPECT_EQ(req.base().at("KEY"), "test-key");
    EXPECT_EQ(req.base().at("TIMESTAMP"), "1499827319");
    EXPECT_EQ(req.base().at(beast::http::field::content_type), "application/json");
    const std::string preSignedText = std::string(req.method_string()) + "\n" + path + "\n\n" +
                                      UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, req.body(), true) + "\n1499827319";
    EXPECT_EQ(req.base().at("SIGN"), Hmac::hmac(Hmac::ShaVersion::SHA512, "test-secret", preSignedText, true));
    EXPECT_EQ(req.base().at(beast::http::field::content_length), std::to_string(req.body().size()));
  }

  ServiceContext serviceContext;
  std::shared_ptr<ExecutionManagementServiceGateioBase> service;
  std::string exchange;
  std::map<std::string, std::string> credential;
  TimePoint now;
};

TEST_P(ExecutionManagementServiceGateioAmendTest, orderIdAndStandardParams) {
  Request request(Request::Operation::AMEND_ORDER, this->exchange, "BTC_USDT", "foo", this->credential);
  request.appendParam({{CCAPI_EM_ORDER_ID, "123456"}, {CCAPI_EM_ORDER_QUANTITY, "10"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"}});
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req, "123456");
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), GetParam() ? 2 : 3);
  EXPECT_EQ(std::string(document["price"].GetString()), "25000");
  if (GetParam()) {
    ASSERT_TRUE(document["size"].IsInt64());
    EXPECT_EQ(document["size"].GetInt64(), 10);
  } else {
    EXPECT_EQ(std::string(document["amount"].GetString()), "10");
    EXPECT_EQ(std::string(document["currency_pair"].GetString()), "BTC_USDT");
  }
}

TEST_P(ExecutionManagementServiceGateioAmendTest, clientOrderIdAndPriceOnly) {
  Request request(Request::Operation::AMEND_ORDER, this->exchange, "BTC_USDT", "foo", this->credential);
  request.appendParam({{CCAPI_EM_CLIENT_ORDER_ID, "t-client1"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"}, {"amend_text", "amend1"}});
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req, "t-client1");
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), GetParam() ? 2 : 3);
  EXPECT_EQ(std::string(document["price"].GetString()), "25000");
  EXPECT_EQ(std::string(document["amend_text"].GetString()), "amend1");
}

TEST_P(ExecutionManagementServiceGateioAmendTest, nativeParamsAndExplicitSettlement) {
  Request request(Request::Operation::AMEND_ORDER, this->exchange, "", "foo", this->credential);
  std::map<std::string, std::string> param{{"order_id", "123456"}, {"price", "25000"}};
  if (GetParam()) {
    param["settle"] = "btc";
    param["size"] = "-5000000000";
  } else {
    param["currency_pair"] = "BTC_USDT";
    param["amount"] = "0.02";
    param["account"] = "unified";
    param["action_mode"] = "FULL";
  }
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req, "123456", "btc");
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(std::string(document["price"].GetString()), "25000");
  if (GetParam()) {
    EXPECT_EQ(document.MemberCount(), 2);
    ASSERT_TRUE(document["size"].IsInt64());
    EXPECT_EQ(document["size"].GetInt64(), -5000000000LL);
  } else {
    EXPECT_EQ(document.MemberCount(), 5);
    EXPECT_EQ(std::string(document["amount"].GetString()), "0.02");
    EXPECT_EQ(std::string(document["currency_pair"].GetString()), "BTC_USDT");
    EXPECT_EQ(std::string(document["account"].GetString()), "unified");
    EXPECT_EQ(std::string(document["action_mode"].GetString()), "FULL");
  }
}

TEST_P(ExecutionManagementServiceGateioAmendTest, quantityOnlyAndOrderIdPriority) {
  Request request(Request::Operation::AMEND_ORDER, this->exchange, "BTC_USD", "foo", this->credential);
  request.appendParam({{CCAPI_EM_ORDER_ID, "123456"}, {CCAPI_EM_CLIENT_ORDER_ID, "t-client1"}, {CCAPI_EM_ORDER_QUANTITY, "2"}});
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req, "123456", "btc");
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), GetParam() ? 1 : 2);
  if (GetParam()) {
    EXPECT_EQ(document["size"].GetInt64(), 2);
  } else {
    EXPECT_EQ(std::string(document["amount"].GetString()), "2");
  }
}

TEST_P(ExecutionManagementServiceGateioAmendTest, standardizedAccountOrSettlement) {
  Request request(Request::Operation::AMEND_ORDER, this->exchange, "BTC_USDT", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "123456"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"}};
  param[GetParam() ? CCAPI_SETTLE_ASSET : CCAPI_EM_ACCOUNT_TYPE] = GetParam() ? "btc" : "unified";
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req, "123456", "btc");
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), GetParam() ? 1 : 3);
  if (!GetParam()) {
    EXPECT_EQ(std::string(document["account"].GetString()), "unified");
  }
}

TEST_P(ExecutionManagementServiceGateioAmendTest, response) {
  Request request(Request::Operation::AMEND_ORDER, this->exchange, "BTC_USDT", "foo", this->credential);
  const std::string textMessage = GetParam()
                                      ? R"({"id":123456,"text":"t-client1","contract":"BTC_USDT","size":-10,"price":"25000","status":"open"})"
                                      : R"({"id":"123456","text":"t-client1","currency_pair":"BTC_USDT","amount":"0.02","price":"25000","status":"open"})";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  ASSERT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, "foo");
  EXPECT_EQ(messageList.at(0).getType(), Message::Type::AMEND_ORDER);
  const auto& elementList = messageList.at(0).getElementList();
  ASSERT_EQ(elementList.size(), 1);
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_ORDER_ID), "123456");
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_CLIENT_ORDER_ID), "t-client1");
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "25000");
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_ORDER_QUANTITY), GetParam() ? "-10" : "0.02");
}

INSTANTIATE_TEST_SUITE_P(SpotAndFutures, ExecutionManagementServiceGateioAmendTest, ::testing::Bool());

}  // namespace ccapi
