#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_bitget.h"

// clang-format on

namespace ccapi {

class ExecutionManagementServiceBitgetTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;

  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceBitget>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), &this->serviceContext);
    this->credential = {
        {CCAPI_BITGET_API_KEY, "bg_xxxxxxxxxxxxxxxxxxx"},
        {CCAPI_BITGET_API_SECRET, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
        {CCAPI_BITGET_API_PASSPHRASE, "xxxxxxxxxxxx"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }

  ServiceContext serviceContext;
  std::shared_ptr<ExecutionManagementServiceBitget> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = std::string(req.base().at("ACCESS-TIMESTAMP"));
  preSignedText += std::string(req.method_string());
  preSignedText += std::string(req.target());
  preSignedText += req.body();
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText)), std::string(req.base().at("ACCESS-SIGN")));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/api/v2/spot/trade/place-order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTCUSDT");
  EXPECT_EQ(std::string(document["size"].GetString()), "0.1");
  EXPECT_FALSE(document.HasMember("loanType"));
  verifySignature(req, this->credential.at(CCAPI_BITGET_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertRequestCreateOrderCrossMargin) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/api/v2/margin/crossed/place-order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTCUSDT");
  // Margin orders express the base asset quantity as baseSize
  EXPECT_EQ(std::string(document["baseSize"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["loanType"].GetString()), "normal");
  EXPECT_EQ(std::string(document["price"].GetString()), "15600");
  EXPECT_EQ(std::string(document["force"].GetString()), "gtc");
  verifySignature(req, this->credential.at(CCAPI_BITGET_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertRequestCreateOrderIsolatedMargin) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "0.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "15600"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/api/v2/margin/isolated/place-order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["baseSize"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["loanType"].GetString()), "normal");
  verifySignature(req, this->credential.at(CCAPI_BITGET_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertRequestCancelOrderCrossMargin) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  request.appendParam({
      {CCAPI_EM_ORDER_ID, "121211212122"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/api/v2/margin/crossed/cancel-order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["orderId"].GetString()), "121211212122");
  verifySignature(req, this->credential.at(CCAPI_BITGET_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertTextMessageToMessageRestCreateOrderCrossMargin) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  std::string textMessage =
      R"(
        {
          "code": "00000",
          "msg": "success",
          "requestTime": 1627293504612,
          "data": {
            "orderId": "121211212122",
            "clientOid": "myClientOid001"
          }
        }
      )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CREATE_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_ORDER_ID), "121211212122");
  EXPECT_EQ(elementList.at(0).getValue(CCAPI_EM_CLIENT_ORDER_ID), "myClientOid001");
}

TEST_F(ExecutionManagementServiceBitgetTest, convertRequestGetOpenOrdersCrossMargin) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(std::string(req.target()), "?");
  EXPECT_EQ(splitted.at(0), "/api/v2/margin/crossed/open-orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSDT");
  verifySignature(req, this->credential.at(CCAPI_BITGET_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertRequestGetAccountBalancesCrossMargin) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BITGET, "", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  EXPECT_EQ(req.target(), "/api/v2/margin/crossed/account/assets");
  verifySignature(req, this->credential.at(CCAPI_BITGET_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitgetTest, convertTextMessageToMessageRestGetAccountBalancesCrossMargin) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BITGET, "", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  std::string textMessage =
      R"(
        {
          "code": "00000",
          "msg": "success",
          "requestTime": 1695636829522,
          "data": [
            {
              "coin": "USDT",
              "totalAmount": "1000",
              "available": "900",
              "frozen": "100",
              "borrow": "200",
              "interest": "1.5",
              "net": "800"
            }
          ]
        }
      )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_BALANCES);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_TOTAL), "1000");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "900");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_LIABILITY), "201.5");
}

TEST_F(ExecutionManagementServiceBitgetTest, convertTextMessageToMessageRestGetOpenOrdersCrossMargin) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITGET, "BTCUSDT", "foo", this->credential);
  request.setMarginType(CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
  std::string textMessage =
      R"(
        {
          "code": "00000",
          "msg": "success",
          "requestTime": 1695636829522,
          "data": {
            "resultList": [
              {
                "orderId": "121211212122",
                "symbol": "BTCUSDT",
                "orderType": "limit",
                "clientOid": "myClientOid001",
                "loanType": "normal",
                "price": "32111",
                "side": "buy",
                "status": "live",
                "baseSize": "0.01",
                "quoteSize": "1000",
                "priceAvg": "32111",
                "size": "0.005",
                "amount": "160",
                "force": "gtc",
                "cTime": "1695629859821",
                "uTime": "1695629890839"
              }
            ],
            "maxId": "1",
            "minId": "1"
          }
        }
      )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "121211212122");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTCUSDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.01");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "32111");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.005");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY), "160");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "live");
}

} /* namespace ccapi */
#endif
#endif
