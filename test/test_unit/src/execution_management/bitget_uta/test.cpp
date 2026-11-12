#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_UTA
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_bitget_uta.h"
// clang-format on

namespace ccapi {

class ExecutionManagementServiceBitgetUtaTest : public ::testing::Test {
 public:
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceBitgetUta>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), &this->serviceContext);
    this->credential = {{CCAPI_BITGET_UTA_API_KEY, "test-key"},
                        {CCAPI_BITGET_UTA_API_SECRET, "test-secret"},
                        {CCAPI_BITGET_UTA_API_PASSPHRASE, "test-passphrase"},
                        {CCAPI_BITGET_UTA_API_DEMO_TRADING, "1"}};
    this->now = UtilTime::makeTimePointFromMilliseconds(1499827319000LL);
  }

  void verifyRequest(const http::request<http::string_body>& req) {
    EXPECT_EQ(req.method(), http::verb::post);
    EXPECT_EQ(req.target(), "/api/v3/trade/modify-order");
    EXPECT_EQ(req.base().at(beast::http::field::content_type), "application/json");
    EXPECT_EQ(req.base().at("ACCESS-KEY"), this->credential.at(CCAPI_BITGET_UTA_API_KEY));
    EXPECT_EQ(req.base().at("ACCESS-PASSPHRASE"), this->credential.at(CCAPI_BITGET_UTA_API_PASSPHRASE));
    EXPECT_EQ(req.base().at("ACCESS-TIMESTAMP"), "1499827319000");
    EXPECT_EQ(req.base().at("paptrading"), "1");
    const auto preSignedText = std::string(req.base().at("ACCESS-TIMESTAMP")) + "POST" + std::string(req.target()) + req.body();
    EXPECT_EQ(req.base().at("ACCESS-SIGN"),
              UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, this->credential.at(CCAPI_BITGET_UTA_API_SECRET), preSignedText)));
    EXPECT_EQ(req.base().at(beast::http::field::content_length), std::to_string(req.body().size()));
  }

  ServiceContext serviceContext;
  std::shared_ptr<ExecutionManagementServiceBitgetUta> service;
  std::map<std::string, std::string> credential;
  TimePoint now;
};

TEST_F(ExecutionManagementServiceBitgetUtaTest, convertRequestAmendOrderByOrderId) {
  Request request(Request::Operation::AMEND_ORDER, CCAPI_EXCHANGE_NAME_BITGET_UTA, "BTCUSDT", "foo", this->credential);
  request.appendParam({{CCAPI_EM_ORDER_ID, "123456789"}, {CCAPI_EM_ORDER_QUANTITY, "0.02"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"}});
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req);
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), 5);
  EXPECT_EQ(std::string(document["orderId"].GetString()), "123456789");
  EXPECT_EQ(std::string(document["qty"].GetString()), "0.02");
  EXPECT_EQ(std::string(document["price"].GetString()), "25000");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTCUSDT");
  EXPECT_EQ(std::string(document["category"].GetString()), "SPOT");
}

TEST_F(ExecutionManagementServiceBitgetUtaTest, convertRequestAmendOrderByClientOrderId) {
  for (const auto& field : {CCAPI_EM_ORDER_QUANTITY, CCAPI_EM_ORDER_LIMIT_PRICE}) {
    Request request(Request::Operation::AMEND_ORDER, CCAPI_EXCHANGE_NAME_BITGET_UTA, "BTCUSDT", "foo", this->credential);
    request.appendParam({{CCAPI_EM_CLIENT_ORDER_ID, "client1"}, {CCAPI_INSTRUMENT_TYPE, "USDT-FUTURES"}, {field, "1"}});
    auto req = this->service->convertRequest(request, this->now);
    this->verifyRequest(req);
    rj::Document document;
    document.Parse(req.body().c_str());
    ASSERT_TRUE(document.IsObject());
    EXPECT_EQ(document.MemberCount(), 4);
    EXPECT_EQ(std::string(document["clientOid"].GetString()), "client1");
    EXPECT_EQ(std::string(document["category"].GetString()), "USDT-FUTURES");
    EXPECT_EQ(std::string(document["symbol"].GetString()), "BTCUSDT");
    auto key = std::string(field) == CCAPI_EM_ORDER_QUANTITY ? "qty" : "price";
    EXPECT_EQ(std::string(document[key].GetString()), "1");
  }
}

TEST_F(ExecutionManagementServiceBitgetUtaTest, convertRequestAmendOrderNativeParams) {
  Request request(Request::Operation::AMEND_ORDER, CCAPI_EXCHANGE_NAME_BITGET_UTA, "", "foo", this->credential);
  request.appendParam({{"orderId", "123456789"},
                       {"clientOid", "client1"},
                       {"symbol", "BTCUSDT"},
                       {"category", "USDT-FUTURES"},
                       {"qty", "0.02"},
                       {"price", "25000"},
                       {"autoCancel", "yes"},
                       {"takeProfit", "30000"}});
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req);
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), 8);
  EXPECT_EQ(std::string(document["orderId"].GetString()), "123456789");
  EXPECT_EQ(std::string(document["clientOid"].GetString()), "client1");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTCUSDT");
  EXPECT_EQ(std::string(document["category"].GetString()), "USDT-FUTURES");
  EXPECT_EQ(std::string(document["qty"].GetString()), "0.02");
  EXPECT_EQ(std::string(document["price"].GetString()), "25000");
  EXPECT_EQ(std::string(document["autoCancel"].GetString()), "yes");
  EXPECT_EQ(std::string(document["takeProfit"].GetString()), "30000");
}

TEST_F(ExecutionManagementServiceBitgetUtaTest, convertRequestAmendOrderWithoutSymbol) {
  Request request(Request::Operation::AMEND_ORDER, CCAPI_EXCHANGE_NAME_BITGET_UTA, "", "foo", this->credential);
  request.appendParam({{CCAPI_EM_ORDER_ID, "123456789"}, {CCAPI_EM_ORDER_QUANTITY, "0.02"}});
  auto req = this->service->convertRequest(request, this->now);
  this->verifyRequest(req);
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document.MemberCount(), 3);
  EXPECT_FALSE(document.HasMember("symbol"));
  EXPECT_EQ(std::string(document["orderId"].GetString()), "123456789");
  EXPECT_EQ(std::string(document["qty"].GetString()), "0.02");
  EXPECT_EQ(std::string(document["category"].GetString()), "SPOT");
}

TEST_F(ExecutionManagementServiceBitgetUtaTest, convertTextMessageToMessageRestAmendOrder) {
  Request request(Request::Operation::AMEND_ORDER, CCAPI_EXCHANGE_NAME_BITGET_UTA, "BTCUSDT", "foo", this->credential);
  const std::string textMessage = R"({"code":"00000","msg":"success","requestTime":1499827319000,"data":{"orderId":"123456789","clientOid":"client1"}})";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  ASSERT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, "foo");
  EXPECT_EQ(messageList.at(0).getType(), Message::Type::AMEND_ORDER);
  const auto& elementList = messageList.at(0).getElementList();
  ASSERT_EQ(elementList.size(), 1);
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_ORDER_ID), "123456789");
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_CLIENT_ORDER_ID), "client1");
}

TEST_F(ExecutionManagementServiceBitgetUtaTest, convertRequestCreateOrderUnchanged) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITGET_UTA, "BTCUSDT", "foo", this->credential);
  request.appendParam({{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY}, {CCAPI_EM_ORDER_QUANTITY, "0.02"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"}});
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.target(), "/api/v3/trade/place-order");
  rj::Document document;
  document.Parse(req.body().c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  EXPECT_EQ(std::string(document["qty"].GetString()), "0.02");
  EXPECT_EQ(std::string(document["price"].GetString()), "25000");
  EXPECT_EQ(std::string(document["orderType"].GetString()), "limit");
}

}  // namespace ccapi
#endif
#endif
