#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceGateioTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceGateio>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                        &this->serviceContext);
    this->credential = {
        {CCAPI_GATEIO_API_KEY, "f897ef1d2bbdca43de0c624d6b4fee7f"},
        {CCAPI_GATEIO_API_SECRET, "55de8bdbfca4f783979559baaa079f1cde28795e1b8b4527e7c22c8978c3f44c"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  ServiceContext serviceContext;std::shared_ptr<ExecutionManagementServiceGateio> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, long long timestamp) {
  EXPECT_EQ(req.base().at("KEY").to_string(), apiKey);
  EXPECT_EQ(req.base().at("TIMESTAMP").to_string(), std::to_string(timestamp));
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  std::string preSignedText = UtilString::toUpper(std::string(req.method_string()));
  preSignedText += "\n";
  auto target = req.target().to_string();
  auto splitted = UtilString::split(target, "?");
  preSignedText += splitted.at(0);
  preSignedText += "\n";
  if (splitted.size() > 1) {
    preSignedText += splitted.at(1);
  }
  preSignedText += "\n";
  preSignedText += UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, req.body(), true);
  preSignedText += "\n";
  preSignedText += req.base().at("TIMESTAMP").to_string();
  auto signature = req.base().at("SIGN").to_string();
  EXPECT_EQ(Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true), signature);
}

TEST_F(ExecutionManagementServiceGateioTest, signRequest) {
  http::request<http::string_body> req;
  req.set("TIMESTAMP", "1633741332");
  req.method(http::verb::post);
  std::string path = "/api/v4/spot/orders";
  std::string queryString = "";
  std::string body(R"({"price":"20000","amount":"0.001","side":"buy","currency_pair":"BTC_USDT"})");
  this->service->signRequest(req, path, queryString, body, this->credential);
  EXPECT_EQ(req.base().at("SIGN").to_string(),
            "0ece8964567fd0f3ed3323c9e9ffe3332869eabb3098e5b6e5a665a85b757cf3df6c16bfb9dcae7427b8937b7a3f754140e6aab5e9c66a61292e9b361d97a3c4");
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  EXPECT_EQ(req.target(), "/api/v4/spot/orders");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["currency_pair"].GetString()), "BTC_USDT");
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["amount"].GetString()), "1");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "id": "12332324",
          "text": "t-123456",
          "create_time": "1548000000",
          "update_time": "1548000100",
          "create_time_ms": 1548000000123,
          "update_time_ms": 1548000100123,
          "currency_pair": "ETH_BTC",
          "status": "cancelled",
          "type": "limit",
          "account": "spot",
          "side": "buy",
          "iceberg": "0",
          "amount": "1",
          "price": "5.00032",
          "time_in_force": "gtc",
          "left": "0.5",
          "filled_total": "2.50016",
          "fee": "0.005",
          "fee_currency": "ETH",
          "point_fee": "0",
          "gt_fee": "0",
          "gt_discount": false,
          "rebated_fee": "0",
          "rebated_fee_currency": "BTC"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "12332324");
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "t-123456"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v4/spot/orders/t-123456");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("currency_pair"), "BTC_USDT");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "t-123456"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v4/spot/orders/t-123456");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("currency_pair"), "BTC_USDT");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::string textMessage =
      R"(
        {
  "id": "12332324",
  "text": "t-123456",
  "create_time": "1548000000",
  "update_time": "1548000100",
  "create_time_ms": 1548000000123,
  "update_time_ms": 1548000100123,
  "currency_pair": "ETH_BTC",
  "status": "cancelled",
  "type": "limit",
  "account": "spot",
  "side": "buy",
  "iceberg": "0",
  "amount": "1",
  "price": "5.00032",
  "time_in_force": "gtc",
  "left": "0.5",
  "filled_total": "2.50016",
  "fee": "0.005",
  "fee_currency": "ETH",
  "point_fee": "0",
  "gt_fee": "0",
  "gt_discount": false,
  "rebated_fee": "0",
  "rebated_fee_currency": "BTC"
}
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "t-123456"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v4/spot/orders/t-123456?currency_pair=BTC_USDT");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "t-123456"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v4/spot/orders/t-123456?currency_pair=BTC_USDT");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::string textMessage =
      R"(
        {
        "id": "12332324",
        "text": "t-123456",
        "create_time": "1548000000",
        "update_time": "1548000100",
        "create_time_ms": 1548000000123,
        "update_time_ms": 1548000100123,
        "currency_pair": "ETH_BTC",
        "status": "cancelled",
        "type": "limit",
        "account": "spot",
        "side": "buy",
        "iceberg": "0",
        "amount": "1",
        "price": "5.00032",
        "time_in_force": "gtc",
        "left": "0.5",
        "filled_total": "2.50016",
        "fee": "0.005",
        "fee_currency": "ETH",
        "point_fee": "0",
        "gt_fee": "0",
        "gt_discount": false,
        "rebated_fee": "0",
        "rebated_fee_currency": "BTC"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "12332324");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "t-123456");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "ETH_BTC");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "5.00032");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0.5");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "2.50016");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "cancelled");
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v4/spot/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("currency_pair"), "BTC_USDT");
  EXPECT_EQ(paramMap.at("status"), "open");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  std::string symbol = "BTC_USDT";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GATEIO, symbol, "", this->credential);
  std::string textMessage =
      R"(
        [
        {
          "id": "12332324",
          "text": "t-123456",
          "create_time": "1548000000",
          "update_time": "1548000100",
          "create_time_ms": 1548000000123,
          "update_time_ms": 1548000100123,
          "currency_pair": "ETH_BTC",
          "status": "open",
          "type": "limit",
          "account": "spot",
          "side": "buy",
          "iceberg": "0",
          "amount": "1",
          "price": "5.00032",
          "time_in_force": "gtc",
          "left": "0.5",
          "filled_total": "2.50016",
          "fee": "0.005",
          "fee_currency": "ETH",
          "point_fee": "0",
          "gt_fee": "0",
          "gt_discount": false,
          "rebated_fee": "0",
          "rebated_fee_currency": "BTC"
        }
      ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "12332324");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "t-123456");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "ETH_BTC");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "5.00032");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0.5");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "2.50016");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "open");
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v4/spot/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("currency_pair"), "BTC_USDT");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", "foo", this->credential);
  std::string textMessage =
      R"(
        [
        {
          "id": "12332324",
          "text": "t-123456",
          "create_time": "1548000000",
          "update_time": "1548000100",
          "create_time_ms": 1548000000123,
          "update_time_ms": 1548000100123,
          "currency_pair": "ETH_BTC",
          "status": "cancelled",
          "type": "limit",
          "account": "spot",
          "side": "buy",
          "iceberg": "0",
          "amount": "1",
          "price": "5.00032",
          "time_in_force": "gtc",
          "left": "0.5",
          "filled_total": "2.50016",
          "fee": "0.005",
          "fee_currency": "ETH",
          "point_fee": "0",
          "gt_fee": "0",
          "gt_discount": false,
          "rebated_fee": "0",
          "rebated_fee_currency": "BTC"
        }
      ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceGateioTest, convertRequestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_GATEIO, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GATEIO_API_KEY), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v4/spot/accounts");
  verifySignature(req, this->credential.at(CCAPI_GATEIO_API_SECRET));
}

TEST_F(ExecutionManagementServiceGateioTest, convertTextMessageToMessageRestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_GATEIO, "", "foo", this->credential);
  std::string textMessage =
      R"(
        [
      {
        "currency": "ETH",
        "available": "968.8",
        "locked": "0"
      }
    ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNTS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "ETH");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "968.8");
}

TEST_F(ExecutionManagementServiceGateioTest, createEventUserTrades) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
    "time": 1605176741,
    "channel": "spot.usertrades",
    "event": "update",
    "result": [
      {
        "id": 5736713,
        "user_id": 1000001,
        "order_id": "30784428",
        "currency_pair": "BTC_USDT",
        "create_time": 1605176741,
        "create_time_ms": "1605176741123.456",
        "side": "sell",
        "amount": "1.00000000",
        "role": "taker",
        "price": "10000.00000000",
        "fee": "0.00200000000000",
        "point_fee": "0",
        "gt_fee": "0",
        "text": "apiv4"
      }
    ]
  }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  auto messageList = this->service->createEvent(WsConnection(), wspp::lib::weak_ptr<void>(), subscription, textMessage, document, this->now).getMessageList();
#else
  auto messageList = this->service->createEvent(std::shared_ptr<WsConnection>(), subscription, textMessage, document, this->now).getMessageList();
#endif
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "5736713");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "10000.00000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "1.00000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "30784428");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "apiv4");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC_USDT");
}

TEST_F(ExecutionManagementServiceGateioTest, createEventOrders) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_GATEIO, "BTC_USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
    "time": 1605175506,
    "channel": "spot.orders",
    "event": "update",
    "result": [
      {
        "id": "30784435",
        "user": 123456,
        "text": "t-abc",
        "create_time": "1605175506",
        "create_time_ms": "1605175506123",
        "update_time": "1605175506",
        "update_time_ms": "1605175506123",
        "event": "put",
        "currency_pair": "BTC_USDT",
        "type": "limit",
        "account": "spot",
        "side": "sell",
        "amount": "1",
        "price": "10001",
        "time_in_force": "gtc",
        "left": "1",
        "filled_total": "0",
        "fee": "0",
        "fee_currency": "USDT",
        "point_fee": "0",
        "gt_fee": "0",
        "gt_discount": true,
        "rebated_fee": "0",
        "rebated_fee_currency": "USDT"
      }
    ]
  }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  auto messageList = this->service->createEvent(WsConnection(), wspp::lib::weak_ptr<void>(), subscription, textMessage, document, this->now).getMessageList();
#else
  auto messageList = this->service->createEvent(std::shared_ptr<WsConnection>(), subscription, textMessage, document, this->now).getMessageList();
#endif
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "30784435");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "t-abc");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "10001");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "put");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC_USDT");
}
} /* namespace ccapi */
#endif
#endif
