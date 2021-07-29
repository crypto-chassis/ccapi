#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_bitmex.h"
namespace ccapi {
class ExecutionManagementServiceBitmexTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceBitmex>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_BITMEX_API_KEY, "LAqUlngMIQkIUjXMUreyu3qn"},
        {CCAPI_BITMEX_API_SECRET, "chNOOS4KvNXR_Xq4k4c9qsfoKWvnDecLATCRlcBwyKDYnWgO"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  std::shared_ptr<ExecutionManagementServiceBitmex> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, long long timestamp) {
  EXPECT_EQ(req.base().at("api-key").to_string(), apiKey);
  EXPECT_EQ(req.base().at("api-expires").to_string(), std::to_string(timestamp + CCAPI_BITMEX_API_RECEIVE_WINDOW_SECONDS));
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = UtilString::toUpper(req.method_string().to_string());
  preSignedText += req.target().to_string();
  preSignedText += req.base().at("api-expires").to_string();
  preSignedText += req.body();
  auto signature = req.base().at("api-signature").to_string();
  EXPECT_EQ(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true), signature);
}

TEST_F(ExecutionManagementServiceBitmexTest, signRequest) {
  http::request<http::string_body> req;
  req.set("api-expires", "1518064238");
  req.method(http::verb::post);
  req.target("/api/v1/order");
  std::string body("{\"symbol\":\"XBTM15\",\"price\":219.0,\"clOrdID\":\"mm_bitmex_1a/oemUeQ4CAJZgP3fjHsA\",\"orderQty\":98}");
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("api-signature").to_string(), "1749cd2ccae4aa49048ae09f0b95110cee706e0944e6a14ad0b3a8cb45bd336b");
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/order");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["symbol"].GetString()), "XBTUSD");
  EXPECT_EQ(std::string(document["side"].GetString()), "Buy");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["orderQty"].GetString()), "1");
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "orderID": "string",
    "clOrdID": "string",
    "clOrdLinkID": "string",
    "account": 0,
    "symbol": "string",
    "side": "string",
    "simpleOrderQty": 0,
    "orderQty": 0,
    "price": 0,
    "displayQty": 0,
    "stopPx": 0,
    "pegOffsetValue": 0,
    "pegPriceType": "string",
    "currency": "string",
    "settlCurrency": "string",
    "ordType": "string",
    "timeInForce": "string",
    "execInst": "string",
    "contingencyType": "string",
    "exDestination": "string",
    "ordStatus": "string",
    "triggered": "string",
    "workingIndicator": true,
    "ordRejReason": "string",
    "simpleLeavesQty": 0,
    "leavesQty": 0,
    "simpleCumQty": 0,
    "cumQty": 0,
    "avgPx": 0,
    "multiLegReportingType": "string",
    "text": "string",
    "transactTime": "2021-01-12T20:48:37.807Z",
    "timestamp": "2021-01-12T20:48:37.807Z"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "string");
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "string"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("orderID"), "string");
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "string"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("clOrdID"), "string");
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::string textMessage =
      R"(
  [
    {
      "orderID": "string",
      "clOrdID": "string",
      "clOrdLinkID": "string",
      "account": 0,
      "symbol": "string",
      "side": "string",
      "simpleOrderQty": 0,
      "orderQty": 0,
      "price": 0,
      "displayQty": 0,
      "stopPx": 0,
      "pegOffsetValue": 0,
      "pegPriceType": "string",
      "currency": "string",
      "settlCurrency": "string",
      "ordType": "string",
      "timeInForce": "string",
      "execInst": "string",
      "contingencyType": "string",
      "exDestination": "string",
      "ordStatus": "string",
      "triggered": "string",
      "workingIndicator": true,
      "ordRejReason": "string",
      "simpleLeavesQty": 0,
      "leavesQty": 0,
      "simpleCumQty": 0,
      "cumQty": 0,
      "avgPx": 0,
      "multiLegReportingType": "string",
      "text": "string",
      "transactTime": "2021-01-12T21:06:26.391Z",
      "timestamp": "2021-01-12T21:06:26.391Z"
    }
  ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "string"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("filter"), Url::urlEncode("{\"orderID\":\"string\"}"));
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "string"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("filter"), Url::urlEncode("{\"clOrdID\":\"string\"}"));
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::string textMessage =
      R"(
  [
    {
      "orderID": "68e6a28f-ae28-4788-8d4f-5ab4e5e5ae08",
      "clOrdID": "string",
      "clOrdLinkID": "string",
      "account": 0,
      "symbol": "string",
      "side": "Buy",
      "simpleOrderQty": 0,
      "orderQty": 1,
      "price": 0,
      "displayQty": 0,
      "stopPx": 0,
      "pegOffsetValue": 0,
      "pegPriceType": "string",
      "currency": "string",
      "settlCurrency": "string",
      "ordType": "string",
      "timeInForce": "string",
      "execInst": "string",
      "contingencyType": "string",
      "exDestination": "string",
      "ordStatus": "New",
      "triggered": "string",
      "workingIndicator": true,
      "ordRejReason": "string",
      "simpleLeavesQty": 0,
      "leavesQty": 0,
      "simpleCumQty": 0,
      "cumQty": 0,
      "avgPx": 0,
      "multiLegReportingType": "string",
      "text": "string",
      "transactTime": "2021-01-12T20:48:37.763Z",
      "timestamp": "2021-01-12T20:48:37.763Z"
    }
  ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "68e6a28f-ae28-4788-8d4f-5ab4e5e5ae08");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "New");
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "XBTUSD");
  EXPECT_EQ(paramMap.at("filter"), Url::urlEncode("{\"open\": true}"));
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITMEX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("filter"), Url::urlEncode("{\"open\": true}"));
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

void verifyconvertTextMessageToMessageRestGetOpenOrders(const ExecutionManagementServiceBitmexTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "XBTUSD" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITMEX, symbol, "", fixture->credential);
  std::string textMessage =
      R"(
  [
    {
      "orderID": "d0c5340b-6d6c-49d9-b567-48c4bfca13d2",
      "clOrdID": "string",
      "clOrdLinkID": "string",
      "account": 0,
      "symbol": "XBTUSD",
      "side": "Buy",
      "simpleOrderQty": 0,
      "orderQty": 1,
      "price": 0.10000000,
      "displayQty": 0,
      "stopPx": 0,
      "pegOffsetValue": 0,
      "pegPriceType": "string",
      "currency": "string",
      "settlCurrency": "string",
      "ordType": "string",
      "timeInForce": "string",
      "execInst": "string",
      "contingencyType": "string",
      "exDestination": "string",
      "ordStatus": "string",
      "triggered": "string",
      "workingIndicator": true,
      "ordRejReason": "string",
      "simpleLeavesQty": 0,
      "leavesQty": 0,
      "simpleCumQty": 0,
      "cumQty": 0,
      "avgPx": 0,
      "multiLegReportingType": "string",
      "text": "string",
      "transactTime": "2021-01-12T20:48:37.763Z",
      "timestamp": "2021-01-12T20:48:37.763Z"
    }
  ]
  )";
  auto messageList = fixture->service->convertTextMessageToMessageRest(request, textMessage, fixture->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.10000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
  if (!isOneInstrument) {
    EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XBTUSD");
  }
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, false);
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/order/all");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "XBTUSD");
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", "foo", this->credential);
  std::string textMessage =
      R"(
  [
    {
      "orderID": "string",
      "clOrdID": "string",
      "clOrdLinkID": "string",
      "account": 0,
      "symbol": "string",
      "side": "string",
      "simpleOrderQty": 0,
      "orderQty": 0,
      "price": 0,
      "displayQty": 0,
      "stopPx": 0,
      "pegOffsetValue": 0,
      "pegPriceType": "string",
      "currency": "string",
      "settlCurrency": "string",
      "ordType": "string",
      "timeInForce": "string",
      "execInst": "string",
      "contingencyType": "string",
      "exDestination": "string",
      "ordStatus": "string",
      "triggered": "string",
      "workingIndicator": true,
      "ordRejReason": "string",
      "simpleLeavesQty": 0,
      "leavesQty": 0,
      "simpleCumQty": 0,
      "cumQty": 0,
      "avgPx": 0,
      "multiLegReportingType": "string",
      "text": "string",
      "transactTime": "2021-01-12T20:48:37.854Z",
      "timestamp": "2021-01-12T20:48:37.854Z"
    }
  ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BITMEX, "", "foo", this->credential);
  request.appendParam({
      {CCAPI_EM_ASSET, "XBt"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/user/margin");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("currency"), "XBt");
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BITMEX, "", "foo", this->credential);
  request.appendParam({
      {CCAPI_EM_ASSET, "XBt"},
  });
  std::string textMessage =
      R"(
        {
  "account": 335606,
  "currency": "XBt",
  "riskLimit": 1000000000000,
  "prevState": "",
  "state": "",
  "action": "",
  "amount": 1000000,
  "pendingCredit": 0,
  "pendingDebit": 0,
  "confirmedDebit": 0,
  "prevRealisedPnl": 0,
  "prevUnrealisedPnl": 0,
  "grossComm": 4095,
  "grossOpenCost": 606060,
  "grossOpenPremium": 0,
  "grossExecCost": 0,
  "grossMarkValue": 1491140,
  "riskValue": 2097200,
  "taxableMargin": 0,
  "initMargin": 6975,
  "maintMargin": 15984,
  "sessionMargin": 0,
  "targetExcessMargin": 0,
  "varMargin": 0,
  "realisedPnl": -4095,
  "unrealisedPnl": -6106,
  "indicativeTax": 0,
  "unrealisedProfit": 0,
  "syntheticMargin": null,
  "walletBalance": 995905,
  "marginBalance": 989799,
  "marginBalancePcnt": 0.6638,
  "marginLeverage": 1.5065078869548263,
  "marginUsedPcnt": 0.0232,
  "excessMargin": 966840,
  "excessMarginPcnt": 0.6484,
  "availableMargin": 966840,
  "withdrawableMargin": 966840,
  "timestamp": "2021-07-11T06:37:41.401Z",
  "grossLastValue": 1491140,
  "commission": null
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "335606");
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "XBt");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "966840");
}

TEST_F(ExecutionManagementServiceBitmexTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_BITMEX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_BITMEX_API_KEY), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/position");
  verifySignature(req, this->credential.at(CCAPI_BITMEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceBitmexTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_BITMEX, "", "foo", this->credential);
  std::string textMessage =
      R"(
        [
  {
    "account": 335606,
    "symbol": "XBTUSD",
    "currency": "XBt",
    "underlying": "XBT",
    "quoteCurrency": "USD",
    "commission": 0.00075,
    "initMarginReq": 0.01,
    "maintMarginReq": 0.0035,
    "riskLimit": 20000000000,
    "leverage": 100,
    "crossMargin": true,
    "deleveragePercentile": 1,
    "rebalancedPnl": 0,
    "prevRealisedPnl": 0,
    "prevUnrealisedPnl": 0,
    "prevClosePrice": 33636.77,
    "openingTimestamp": "2021-07-11T06:00:00.000Z",
    "openingQty": 500,
    "openingCost": -1485034,
    "openingComm": 4095,
    "openOrderBuyQty": 200,
    "openOrderBuyCost": -606060,
    "openOrderBuyPremium": 0,
    "openOrderSellQty": 0,
    "openOrderSellCost": 0,
    "openOrderSellPremium": 0,
    "execBuyQty": 0,
    "execBuyCost": 0,
    "execSellQty": 0,
    "execSellCost": 0,
    "execQty": 0,
    "execCost": 0,
    "execComm": 0,
    "currentTimestamp": "2021-07-11T06:37:20.401Z",
    "currentQty": 500,
    "currentCost": -1485034,
    "currentComm": 4095,
    "realisedCost": 0,
    "unrealisedCost": -1485034,
    "grossOpenCost": 606060,
    "grossOpenPremium": 0,
    "grossExecCost": 0,
    "isOpen": true,
    "markPrice": 33525.3,
    "markValue": -1491410,
    "riskValue": 2097470,
    "homeNotional": 0.0149141,
    "foreignNotional": -500,
    "posState": "",
    "posCost": -1485034,
    "posCost2": -1482052,
    "posCross": 9358,
    "posInit": 14851,
    "posComm": 1133,
    "posLoss": 2982,
    "posMargin": 22360,
    "posMaint": 8670,
    "posAllowance": 0,
    "taxableMargin": 0,
    "initMargin": 6975,
    "maintMargin": 15984,
    "sessionMargin": 0,
    "targetExcessMargin": 0,
    "varMargin": 0,
    "realisedGrossPnl": 0,
    "realisedTax": 0,
    "realisedPnl": -4095,
    "unrealisedGrossPnl": -6376,
    "longBankrupt": 0,
    "shortBankrupt": 0,
    "taxBase": 0,
    "indicativeTaxRate": null,
    "indicativeTax": 0,
    "unrealisedTax": 0,
    "unrealisedPnl": -6376,
    "unrealisedPnlPcnt": -0.0043,
    "unrealisedRoePcnt": -0.4294,
    "simpleQty": null,
    "simpleCost": null,
    "simpleValue": null,
    "simplePnl": null,
    "simplePnlPcnt": null,
    "avgCostPrice": 33669.3535,
    "avgEntryPrice": 33669.3535,
    "breakEvenPrice": 33762.5,
    "marginCallPrice": 20288,
    "liquidationPrice": 20288,
    "bankruptPrice": 20226,
    "timestamp": "2021-07-11T06:37:20.401Z",
    "lastPrice": 33525.3,
    "lastValue": -1491410
  }
]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_POSITIONS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "335606");
  EXPECT_EQ(element.getValue(CCAPI_EM_SYMBOL), "XBTUSD");
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "XBt");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "500");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_COST), "-1485034");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_LEVERAGE), "100");
}

TEST_F(ExecutionManagementServiceBitmexTest, createEventPrivateTrade) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
  "table": "execution",
  "action": "insert",
  "data": [
    {
      "execID": "319b1cbe-f8f6-b738-1f28-4d9e8547935d",
      "orderID": "5a9d150d-9e0e-4ef9-9afc-9ff00caeb644",
      "clOrdID": "6d4eb0fb-2229-469f-873e-557dd78ac11",
      "clOrdLinkID": "",
      "account": 335606,
      "symbol": "XBTUSD",
      "side": "Buy",
      "lastQty": null,
      "lastPx": null,
      "underlyingLastPx": null,
      "lastMkt": "",
      "lastLiquidityInd": "",
      "simpleOrderQty": null,
      "orderQty": 100,
      "price": 34000,
      "displayQty": null,
      "stopPx": null,
      "pegOffsetValue": null,
      "pegPriceType": "",
      "currency": "USD",
      "settlCurrency": "XBt",
      "execType": "New",
      "ordType": "Limit",
      "timeInForce": "GoodTillCancel",
      "execInst": "",
      "contingencyType": "",
      "exDestination": "XBME",
      "ordStatus": "New",
      "triggered": "",
      "workingIndicator": true,
      "ordRejReason": "",
      "simpleLeavesQty": null,
      "leavesQty": 100,
      "simpleCumQty": null,
      "cumQty": 0,
      "avgPx": null,
      "commission": null,
      "tradePublishIndicator": "",
      "multiLegReportingType": "SingleSecurity",
      "text": "Submitted via API.",
      "trdMatchID": "00000000-0000-0000-0000-000000000000",
      "execCost": null,
      "execComm": null,
      "homeNotional": null,
      "foreignNotional": null,
      "transactTime": "2021-07-11T03:49:11.832Z",
      "timestamp": "2021-07-11T03:49:11.832Z"
    },
    {
      "execID": "85eb8b5c-3f33-988b-607e-8cf513cc2ac5",
      "orderID": "5a9d150d-9e0e-4ef9-9afc-9ff00caeb644",
      "clOrdID": "6d4eb0fb-2229-469f-873e-557dd78ac11",
      "clOrdLinkID": "",
      "account": 335606,
      "symbol": "XBTUSD",
      "side": "Buy",
      "lastQty": 100,
      "lastPx": 33613,
      "underlyingLastPx": null,
      "lastMkt": "XBME",
      "lastLiquidityInd": "RemovedLiquidity",
      "simpleOrderQty": null,
      "orderQty": 100,
      "price": 34000,
      "displayQty": null,
      "stopPx": null,
      "pegOffsetValue": null,
      "pegPriceType": "",
      "currency": "USD",
      "settlCurrency": "XBt",
      "execType": "Trade",
      "ordType": "Limit",
      "timeInForce": "GoodTillCancel",
      "execInst": "",
      "contingencyType": "",
      "exDestination": "XBME",
      "ordStatus": "Filled",
      "triggered": "",
      "workingIndicator": false,
      "ordRejReason": "",
      "simpleLeavesQty": null,
      "leavesQty": 0,
      "simpleCumQty": null,
      "cumQty": 100,
      "avgPx": 33613,
      "commission": 0.00075,
      "tradePublishIndicator": "PublishTrade",
      "multiLegReportingType": "SingleSecurity",
      "text": "Submitted via API.",
      "trdMatchID": "95c2976a-34a0-b8dc-b17d-984350d9dab5",
      "execCost": -297504,
      "execComm": 223,
      "homeNotional": 0.00297504,
      "foreignNotional": -100,
      "transactTime": "2021-07-11T03:49:11.832Z",
      "timestamp": "2021-07-11T03:49:11.832Z"
    }
  ]
}
)";
  rj::Document document;
  document.Parse(textMessage.c_str());
  auto messageList = this->service->createEvent(wspp::lib::weak_ptr<void>(), subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "95c2976a-34a0-b8dc-b17d-984350d9dab5");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "33613");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "100");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5a9d150d-9e0e-4ef9-9afc-9ff00caeb644");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "6d4eb0fb-2229-469f-873e-557dd78ac11");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XBTUSD");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY), "0.00075");
}

TEST_F(ExecutionManagementServiceBitmexTest, createEventOrderUpdate) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_BITMEX, "XBTUSD", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
  "table": "order",
  "action": "insert",
  "data": [
    {
      "orderID": "c538f094-ea8c-42c2-81da-a137f5cbf687",
      "clOrdID": "6d4eb0fb-2229-469f-873e-557dd78ac11e",
      "clOrdLinkID": "",
      "account": 335606,
      "symbol": "XBTUSD",
      "side": "Buy",
      "simpleOrderQty": null,
      "orderQty": 100,
      "price": 34970,
      "displayQty": null,
      "stopPx": null,
      "pegOffsetValue": null,
      "pegPriceType": "",
      "currency": "USD",
      "settlCurrency": "XBt",
      "ordType": "Limit",
      "timeInForce": "GoodTillCancel",
      "execInst": "",
      "contingencyType": "",
      "exDestination": "XBME",
      "ordStatus": "Filled",
      "triggered": "",
      "workingIndicator": false,
      "ordRejReason": "",
      "simpleLeavesQty": null,
      "leavesQty": 0,
      "simpleCumQty": null,
      "cumQty": 100,
      "avgPx": 34226,
      "multiLegReportingType": "SingleSecurity",
      "text": "Submitted via API.",
      "transactTime": "2021-07-12T01:05:52.657Z",
      "timestamp": "2021-07-12T01:05:52.657Z"
    }
  ]
}
)";
  rj::Document document;
  document.Parse(textMessage.c_str());
  auto messageList = this->service->createEvent(wspp::lib::weak_ptr<void>(), subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "c538f094-ea8c-42c2-81da-a137f5cbf687");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "6d4eb0fb-2229-469f-873e-557dd78ac11e");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "34970");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "100");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "100");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "Filled");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XBTUSD");
}

} /* namespace ccapi */
#endif
#endif
