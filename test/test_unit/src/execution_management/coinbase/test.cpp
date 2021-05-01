#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_coinbase.h"
namespace ccapi {
class ExecutionManagementServiceCoinbaseTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceCoinbase>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {{CCAPI_COINBASE_API_KEY, "a53c4a1d047bddd07e6d4b5783ae18b0"},
                        {CCAPI_COINBASE_API_SECRET, "+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ=="},
                        {CCAPI_COINBASE_API_PASSPHRASE, "0x1a5y8koaa9"}};
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  std::shared_ptr<ExecutionManagementServiceCoinbase> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, const std::string& apiPassphrase, long long timestamp) {
  EXPECT_EQ(req.base().at("CB-ACCESS-KEY").to_string(), apiKey);
  EXPECT_EQ(req.base().at("CB-ACCESS-PASSPHRASE").to_string(), apiPassphrase);
  EXPECT_EQ(req.base().at("CB-ACCESS-TIMESTAMP").to_string(), std::to_string(timestamp));
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = req.base().at("CB-ACCESS-TIMESTAMP").to_string();
  preSignedText += UtilString::toUpper(std::string(req.method_string()));
  preSignedText += req.target().to_string();
  preSignedText += req.body();
  auto signature = req.base().at("CB-ACCESS-SIGN").to_string();
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), preSignedText)), signature);
}

TEST_F(ExecutionManagementServiceCoinbaseTest, signRequest) {
  http::request<http::string_body> req;
  req.set("CB-ACCESS-TIMESTAMP", "1610075590");
  req.method(http::verb::post);
  req.target("/orders");
  std::string body("{\"size\": \"1.0\", \"price\": \"1.0\", \"side\": \"buy\", \"product_id\": \"BTC-USD\"}");
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("CB-ACCESS-SIGN").to_string(), "QLKK5AWZ5akrAiaH5ugZzm3uRs5XnbsChlojmAs78Wk=");
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY}, {CCAPI_EM_ORDER_QUANTITY, "1"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  EXPECT_EQ(req.target(), "/orders");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["product_id"].GetString()), "BTC-USD");
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["size"].GetString()), "1");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "id": "d0c5340b-6d6c-49d9-b567-48c4bfca13d2",
    "price": "0.10000000",
    "size": "0.01000000",
    "product_id": "BTC-USD",
    "side": "buy",
    "stp": "dc",
    "type": "limit",
    "time_in_force": "GTC",
    "post_only": false,
    "created_at": "2016-12-08T20:02:28.53864Z",
    "fill_fees": "0.0000000000000000",
    "filled_size": "0.00000000",
    "executed_value": "0.0000000000000000",
    "status": "pending",
    "settled": false
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/orders/d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("product_id"), "BTC-USD");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/orders/client:d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("product_id"), "BTC-USD");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  auto messageList = this->service->convertTextMessageToMessageRest(request, "\"415bbb90-b5a5-48cc-85b9-49589cc12626\"", this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/orders/d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/orders/client:d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "id": "68e6a28f-ae28-4788-8d4f-5ab4e5e5ae08",
    "size": "1.00000000",
    "product_id": "BTC-USD",
    "side": "buy",
    "stp": "dc",
    "funds": "9.9750623400000000",
    "specified_funds": "10.0000000000000000",
    "type": "market",
    "post_only": false,
    "created_at": "2016-12-08T20:09:05.508883Z",
    "done_at": "2016-12-08T20:09:05.527Z",
    "done_reason": "filled",
    "fill_fees": "0.0249376391550000",
    "filled_size": "0.01291771",
    "executed_value": "9.9750556620000000",
    "status": "done",
    "settled": true
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "68e6a28f-ae28-4788-8d4f-5ab4e5e5ae08");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1.00000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.01291771");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "9.9750556620000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), CCAPI_EM_ORDER_STATUS_CLOSED);
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("product_id"), "BTC-USD");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_COINBASE, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/orders");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

void verifyconvertTextMessageToMessageRestGetOpenOrders(const ExecutionManagementServiceCoinbaseTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "BTC-USD" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_COINBASE, symbol, "", fixture->credential);
  std::string textMessage =
      R"(
  [
    {
        "id": "d0c5340b-6d6c-49d9-b567-48c4bfca13d2",
        "price": "0.10000000",
        "size": "0.01000000",
        "product_id": "BTC-USD",
        "side": "buy",
        "stp": "dc",
        "type": "limit",
        "time_in_force": "GTC",
        "post_only": false,
        "created_at": "2016-12-08T20:02:28.53864Z",
        "fill_fees": "0.0000000000000000",
        "filled_size": "0.00000000",
        "executed_value": "0.0000000000000000",
        "status": "open",
        "settled": false
    },
    {
        "id": "8b99b139-58f2-4ab2-8e7a-c11c846e3022",
        "price": "1.00000000",
        "size": "1.00000000",
        "product_id": "BTC-USD",
        "side": "buy",
        "stp": "dc",
        "type": "limit",
        "time_in_force": "GTC",
        "post_only": false,
        "created_at": "2016-12-08T20:01:19.038644Z",
        "fill_fees": "0.0000000000000000",
        "filled_size": "0.00000000",
        "executed_value": "0.0000000000000000",
        "status": "open",
        "settled": false
    }
  ]
  )";
  auto messageList = fixture->service->convertTextMessageToMessageRest(request, textMessage, fixture->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.01000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.10000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.00000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "0.0000000000000000");
  if (!isOneInstrument) {
    EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USD");
  }
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, false);
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("product_id"), "BTC-USD");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_COINBASE, "BTC-USD", "foo", this->credential);
  std::string textMessage =
      R"(
  [
    "144c6f8e-713f-4682-8435-5280fbe8b2b4",
    "debe4907-95dc-442f-af3b-cec12f42ebda",
    "cf7aceee-7b08-4227-a76c-3858144323ab",
    "dfc5ae27-cadb-4c0c-beef-8994936fde8a",
    "34fecfbf-de33-4273-b2c6-baf8e8948be4"
  ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_COINBASE, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/accounts");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_COINBASE, "", "foo", this->credential);
  std::string textMessage =
      R"(
    [
        {
            "id": "71452118-efc7-4cc4-8780-a5e22d4baa53",
            "currency": "BTC",
            "balance": "0.0000000000000000",
            "available": "0.0000000000000000",
            "hold": "0.0000000000000000",
            "profile_id": "75da88c5-05bf-4f54-bc85-5c775bd68254",
            "trading_enabled": true
        },
        {
            "id": "e316cb9a-0808-4fd7-8914-97829c1925de",
            "currency": "USD",
            "balance": "80.2301373066930000",
            "available": "79.2266348066930000",
            "hold": "1.0035025000000000",
            "profile_id": "75da88c5-05bf-4f54-bc85-5c775bd68254",
            "trading_enabled": true
        }
    ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNTS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "71452118-efc7-4cc4-8780-a5e22d4baa53");
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_COINBASE, "", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ACCOUNT_ID, "71452118-efc7-4cc4-8780-a5e22d4baa53"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_COINBASE_API_KEY), this->credential.at(CCAPI_COINBASE_API_PASSPHRASE), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/accounts/71452118-efc7-4cc4-8780-a5e22d4baa53");
  verifySignature(req, this->credential.at(CCAPI_COINBASE_API_SECRET));
}

TEST_F(ExecutionManagementServiceCoinbaseTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_COINBASE, "", "foo", this->credential);
  std::string textMessage =
      R"(
    {
        "id": "a1b2c3d4",
        "balance": "1.100",
        "holds": "0.100",
        "available": "1.00",
        "currency": "USD"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "a1b2c3d4");
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USD");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "1.00");
}
} /* namespace ccapi */
#endif
#endif
