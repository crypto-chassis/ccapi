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
    this->credential = {{CCAPI_BITMEX_API_KEY, "LAqUlngMIQkIUjXMUreyu3qn"}, {CCAPI_BITMEX_API_SECRET, "chNOOS4KvNXR_Xq4k4c9qsfoKWvnDecLATCRlcBwyKDYnWgO"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY}, {CCAPI_EM_ORDER_QUANTITY, "1"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "string"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "string"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "string"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "string"}};
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
} /* namespace ccapi */
#endif
#endif
