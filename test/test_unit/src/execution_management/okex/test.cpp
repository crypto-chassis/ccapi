#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_okex.h"
namespace ccapi {
class ExecutionManagementServiceOkexTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceOkex>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {{CCAPI_OKEX_API_KEY, "a53c4a1d047bddd07e6d4b5783ae18b0"},
                        {CCAPI_OKEX_API_SECRET, "+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ=="},
                        {CCAPI_OKEX_API_PASSPHRASE, "0x1a5y8koaa9"}};
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
    this->timestampStr = "2017-07-12T02:41:59.000Z";
  }
  std::shared_ptr<ExecutionManagementServiceOkex> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
  std::string timestampStr;
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, const std::string& apiPassphrase, std::string timestampStr) {
  EXPECT_EQ(req.base().at("OK-ACCESS-KEY").to_string(), apiKey);
  EXPECT_EQ(req.base().at("OK-ACCESS-PASSPHRASE").to_string(), apiPassphrase);
  EXPECT_EQ(req.base().at("OK-ACCESS-TIMESTAMP").to_string(), timestampStr);
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = req.base().at("OK-ACCESS-TIMESTAMP").to_string();
  preSignedText += UtilString::toUpper(std::string(req.method_string()));
  preSignedText += req.target().to_string();
  preSignedText += req.body();
  auto signature = req.base().at("OK-ACCESS-SIGN").to_string();
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText)), signature);
}

TEST_F(ExecutionManagementServiceOkexTest, signRequest) {
  http::request<http::string_body> req;
  req.set("OK-ACCESS-TIMESTAMP", "2021-04-01T18:23:16.027Z");
  req.method(http::verb::post);
  req.target("/api/v5/trade/order");
  std::string body("{\"px\":\"2.15\",\"ordType\":\"limit\",\"sz\":\"2\",\"side\":\"buy\",\"tdMode\":\"cash\",\"instId\":\"BTC-USDT\"}");
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("OK-ACCESS-SIGN").to_string(), "vnOpLd3yPc2Ojwm8w0TafZqnujwm3qfjyIpNrmhUrsk=");
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY}, {CCAPI_EM_ORDER_QUANTITY, "2"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "2.15"}, {"tdMode", "cash"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target(), "/api/v5/trade/order");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["instId"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  EXPECT_EQ(std::string(document["sz"].GetString()), "2");
  EXPECT_EQ(std::string(document["px"].GetString()), "2.15");
  EXPECT_EQ(std::string(document["ordType"].GetString()), "limit");
  EXPECT_EQ(std::string(document["tdMode"].GetString()), "cash");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "code": "0",
      "msg": "",
      "data": [
        {
          "clOrdId": "oktswap6",
          "ordId": "12345689",
          "tag": "",
          "sCode": "0",
          "sMsg": ""
        }
      ]
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CREATE_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "12345689");
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "2510789768709120"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target(), "/api/v5/trade/cancel-order");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["instId"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["ordId"].GetString()), "2510789768709120");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "oktswap6"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target(), "/api/v5/trade/cancel-order");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["instId"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["clOrdId"].GetString()), "oktswap6");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::string textMessage =
      R"(
    {
        "code":"0",
        "msg":"",
        "data":[
            {
                "clOrdId":"oktswap6",
                "ordId":"12345689",
                "sCode":"0",
                "sMsg":""
            }
        ]
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "2510789768709120"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("ordId"), "2510789768709120");
  EXPECT_EQ(paramMap.at("instId"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "b1"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("clOrdId"), "b1");
  EXPECT_EQ(paramMap.at("instId"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "code": "0",
      "msg": "",
      "data": [
        {
          "instType": "FUTURES",
          "instId": "BTC-USD-200329",
          "ccy": "",
          "ordId": "123445",
          "clOrdId": "b1",
          "tag": "",
          "px": "999",
          "sz": "323",
          "pnl": "5",
          "ordType": "limit",
          "side": "buy",
          "posSide": "long",
          "tdMode": "isolated",
          "accFillSz": "3",
          "fillPx": "0",
          "tradeId": "0",
          "fillSz": "0",
          "fillTime": "0",
          "state": "live",
          "avgPx": "2",
          "lever": "20",
          "tpTriggerPx": "",
          "tpOrdPx": "",
          "slTriggerPx": "",
          "slOrdPx": "",
          "feeCcy": "",
          "fee": "",
          "rebateCcy": "",
          "rebate": "",
          "category": "",
          "uTime": "1597026383085",
          "cTime": "1597026383085"
        }
      ]
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "123445");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "323");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "3");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 6);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "live");
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_OKEX, "BTC-USDT", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/orders-pending");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("instId"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_OKEX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/orders-pending");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

void verifyconvertTextMessageToMessageRestGetOpenOrders(const ExecutionManagementServiceOkexTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "BTC-USDT" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_OKEX, symbol, "", fixture->credential);
  std::string textMessage =
      R"(
    {
      "code": "0",
      "msg": "",
      "data": [
        {
          "instType": "FUTURES",
          "instId": "BTC-USDT",
          "ccy": "",
          "ordId": "123445",
          "clOrdId": "b1",
          "tag": "",
          "px": "999",
          "sz": "3",
          "pnl": "5",
          "ordType": "limit",
          "side": "buy",
          "posSide": "long",
          "tdMode": "isolated",
          "accFillSz": "323",
          "fillPx": "0",
          "tradeId": "0",
          "fillSz": "0",
          "fillTime": "0",
          "state": "live",
          "avgPx": "0",
          "lever": "20",
          "tpTriggerPx": "",
          "tpOrdPx": "",
          "slTriggerPx": "",
          "slOrdPx": "",
          "feeCcy": "",
          "fee": "",
          "rebateCcy": "",
          "rebate": "",
          "category": "",
          "uTime": "1597026383085",
          "cTime": "1597026383085"
        }
      ]
    }
  )";
  auto messageList = fixture->service->convertTextMessageToMessageRest(request, textMessage, fixture->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "123445");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "3");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "999");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "323");
  EXPECT_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
  if (!isOneInstrument) {
    EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USDT");
  }
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, false);
}

} /* namespace ccapi */
#endif
#endif
