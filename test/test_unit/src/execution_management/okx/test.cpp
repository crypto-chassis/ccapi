#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKX
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_okx.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceOkxTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceOkx>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                    wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_OKX_API_KEY, "a53c4a1d047bddd07e6d4b5783ae18b0"},
        {CCAPI_OKX_API_SECRET, "+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ=="},
        {CCAPI_OKX_API_PASSPHRASE, "0x1a5y8koaa9"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
    this->timestampStr = "2017-07-12T02:41:59.000Z";
  }
  std::shared_ptr<ExecutionManagementServiceOkx> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
  std::string timestampStr;
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, const std::string& apiPassphrase,
                     const std::string& timestampStr) {
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

TEST_F(ExecutionManagementServiceOkxTest, signRequest) {
  http::request<http::string_body> req;
  req.set("OK-ACCESS-TIMESTAMP", "2021-04-01T18:23:16.027Z");
  req.method(http::verb::post);
  req.target("/api/v5/trade/order");
  std::string body("{\"px\":\"2.15\",\"ordType\":\"limit\",\"sz\":\"2\",\"side\":\"buy\",\"tdMode\":\"cash\",\"instId\":\"BTC-USDT\"}");
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("OK-ACCESS-SIGN").to_string(), "vnOpLd3yPc2Ojwm8w0TafZqnujwm3qfjyIpNrmhUrsk=");
}

TEST_F(ExecutionManagementServiceOkxTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "2"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "2.15"},
      {"tdMode", "cash"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target(), "/api/v5/trade/order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["instId"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  EXPECT_EQ(std::string(document["sz"].GetString()), "2");
  EXPECT_EQ(std::string(document["px"].GetString()), "2.15");
  EXPECT_EQ(std::string(document["ordType"].GetString()), "limit");
  EXPECT_EQ(std::string(document["tdMode"].GetString()), "cash");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
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

TEST_F(ExecutionManagementServiceOkxTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "2510789768709120"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target(), "/api/v5/trade/cancel-order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["instId"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["ordId"].GetString()), "2510789768709120");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "oktswap6"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target(), "/api/v5/trade/cancel-order");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["instId"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["clOrdId"].GetString()), "oktswap6");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
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

TEST_F(ExecutionManagementServiceOkxTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "2510789768709120"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("ordId"), "2510789768709120");
  EXPECT_EQ(paramMap.at("instId"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "b1"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("clOrdId"), "b1");
  EXPECT_EQ(paramMap.at("instId"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
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

TEST_F(ExecutionManagementServiceOkxTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/orders-pending");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("instId"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_OKX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v5/trade/orders-pending");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

void verifyconvertTextMessageToMessageRestGetOpenOrders(const ExecutionManagementServiceOkxTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "BTC-USDT" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_OKX, symbol, "", fixture->credential);
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

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, false);
}

TEST_F(ExecutionManagementServiceOkxTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_OKX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target().to_string(), "/api/v5/account/balance");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_OKX, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
    "code": "0",
    "data": [
        {
            "adjEq": "10679688.0460531643092577",
            "details": [
                {
                    "availBal": "",
                    "availEq": "9930359.9998",
                    "cashBal": "9930359.9998",
                    "ccy": "USDT",
                    "crossLiab": "0",
                    "disEq": "9439737.0772999514",
                    "eq": "9930359.9998",
                    "eqUsd": "9933041.196999946",
                    "frozenBal": "0",
                    "interest": "0",
                    "isoEq": "0",
                    "isoLiab": "0",
                    "liab": "0",
                    "maxLoan": "10000",
                    "mgnRatio": "",
                    "notionalLever": "",
                    "ordFrozen": "0",
                    "twap": "0",
                    "uTime": "1620722938250",
                    "upl": "0",
                    "uplLiab": "0"
                },
                {
                    "availBal": "",
                    "availEq": "33.6799714158199414",
                    "cashBal": "33.2009985",
                    "ccy": "BTC",
                    "crossLiab": "0",
                    "disEq": "1239950.9687532129092577",
                    "eq": "33.771820625136023",
                    "eqUsd": "1239950.9687532129092577",
                    "frozenBal": "0.0918492093160816",
                    "interest": "0",
                    "isoEq": "0",
                    "isoLiab": "0",
                    "liab": "0",
                    "maxLoan": "1453.92289531493594",
                    "mgnRatio": "",
                    "notionalLever": "",
                    "ordFrozen": "0",
                    "twap": "0",
                    "uTime": "1620722938250",
                    "upl": "0.570822125136023",
                    "uplLiab": "0"
                }
            ],
            "imr": "3372.2942371050594217",
            "isoEq": "0",
            "mgnRatio": "70375.35408747017",
            "mmr": "134.8917694842024",
            "notionalUsd": "33722.9423710505978888",
            "ordFroz": "0",
            "totalEq": "11172992.1657531589092577",
            "uTime": "1623392334718"
        }
    ],
    "msg": ""
  }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_BALANCES);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "9930359.9998");
}

TEST_F(ExecutionManagementServiceOkxTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_OKX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKX_API_KEY), this->credential.at(CCAPI_OKX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target().to_string(), "/api/v5/account/positions");
  verifySignature(req, this->credential.at(CCAPI_OKX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkxTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_OKX, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "code": "0",
          "msg": "",
          "data": [
            {
              "adl":"1",
              "availPos":"1",
              "avgPx":"2566.31",
              "cTime":"1619507758793",
              "ccy":"ETH",
              "deltaBS":"",
              "deltaPA":"",
              "gammaBS":"",
              "gammaPA":"",
              "imr":"",
              "instId":"ETH-USD-210430",
              "instType":"FUTURES",
              "interest":"0",
              "last":"2566.22",
              "lever":"10",
              "liab":"",
              "liabCcy":"",
              "liqPx":"2352.8496681818233",
              "margin":"0.0003896645377994",
              "mgnMode":"isolated",
              "mgnRatio":"11.731726509588816",
              "mmr":"0.0000311811092368",
              "notionalUsd":"2276.2546609009605",
              "optVal":"",
              "pTime":"1619507761462",
              "pos":"1",
              "posCcy":"",
              "posId":"307173036051017730",
              "posSide":"long",
              "thetaBS":"",
              "thetaPA":"",
              "tradeId":"109844",
              "uTime":"1619507761462",
              "upl":"-0.0000009932766034",
              "uplRatio":"-0.0025490556801078",
              "vegaBS":"",
              "vegaPA":""
            }
          ]
        }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_POSITIONS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_INSTRUMENT), "ETH-USD-210430");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "long");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "1");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_POSITION_ENTRY_PRICE)), 2566.31);
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_LEVERAGE), "10");
}

TEST_F(ExecutionManagementServiceOkxTest, createEventFilled) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
      "arg": {
        "channel": "orders",
        "instType": "ANY",
        "instId": "BTC-USDT"
      },
      "data": [
        {
          "accFillSz": "0.001",
          "amendResult": "",
          "avgPx": "39200",
          "cTime": "1623824529470",
          "category": "normal",
          "ccy": "",
          "clOrdId": "",
          "code": "0",
          "execType": "T",
          "fee": "-0.0392",
          "feeCcy": "USDT",
          "fillFee": "-0.0392",
          "fillFeeCcy": "USDT",
          "fillNotionalUsd": "39.199216",
          "fillPx": "39200",
          "fillSz": "0.001",
          "fillTime": "1623824529475",
          "instId": "BTC-USDT",
          "instType": "SPOT",
          "lever": "",
          "msg": "",
          "notionalUsd": "19.9996",
          "ordId": "325278884568641536",
          "ordType": "limit",
          "pnl": "0",
          "posSide": "",
          "px": "20000",
          "rebate": "0",
          "rebateCcy": "BTC",
          "reqId": "",
          "side": "sell",
          "slOrdPx": "",
          "slTriggerPx": "",
          "state": "filled",
          "sz": "0.001",
          "tag": "",
          "tdMode": "cash",
          "tpOrdPx": "",
          "tpTriggerPx": "",
          "tradeId": "94615762",
          "uTime": "1623824529476"
        }
      ]
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "94615762");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "39200");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "0.001");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "325278884568641536");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY), "-0.0392");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_ASSET), "USDT");
}

TEST_F(ExecutionManagementServiceOkxTest, createEventLive) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_OKX, "BTC-USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
      "arg": {
        "channel": "orders",
        "instType": "ANY",
        "instId": "BTC-USDT"
      },
      "data": [
        {
          "accFillSz": "0",
          "amendResult": "",
          "avgPx": "",
          "cTime": "1623824125174",
          "category": "normal",
          "ccy": "",
          "clOrdId": "",
          "code": "0",
          "execType": "",
          "fee": "0",
          "feeCcy": "BTC",
          "fillFee": "0",
          "fillFeeCcy": "",
          "fillNotionalUsd": "",
          "fillPx": "",
          "fillSz": "0",
          "fillTime": "",
          "instId": "BTC-USDT",
          "instType": "SPOT",
          "lever": "",
          "msg": "",
          "notionalUsd": "19.9998",
          "ordId": "325277188828311552",
          "ordType": "limit",
          "pnl": "0",
          "posSide": "",
          "px": "20000",
          "rebate": "0",
          "rebateCcy": "USDT",
          "reqId": "",
          "side": "buy",
          "slOrdPx": "",
          "slTriggerPx": "",
          "state": "live",
          "sz": "0.001",
          "tag": "",
          "tdMode": "cash",
          "tpOrdPx": "",
          "tpTriggerPx": "",
          "tradeId": "",
          "uTime": "1623824125174"
        }
      ]
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "325277188828311552");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "20000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.001");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "live");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USDT");
}

TEST_F(ExecutionManagementServiceOkxTest, createEventWebsocketTradePlaceOrder) {
  Subscription subscription("okx", "BTC-USDT", "ORDER_UPDATE", "", "same correlation id for subscription and request");
  std::string textMessage = R"(
    {
      "id": "1512",
      "op": "order",
      "data": [
        {
          "clOrdId": "",
          "ordId": "12345689",
          "tag": "",
          "sCode": "0",
          "sMsg": ""
        }
      ],
      "code": "0",
      "msg": ""
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CREATE_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "12345689");
}

TEST_F(ExecutionManagementServiceOkxTest, createEventWebsocketTradeCancelOrder) {
  Subscription subscription("okx", "BTC-USDT", "ORDER_UPDATE", "", "same correlation id for subscription and request");
  std::string textMessage = R"(
    {
      "code": "0",
      "data": [
        {
          "clOrdId": "",
          "ordId": "325631903554482176",
          "sCode": "0",
          "sMsg": ""
        }
      ],
      "id": "1",
      "msg": "",
      "op": "cancel-order"
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "325631903554482176");
}
} /* namespace ccapi */
#endif
#endif
