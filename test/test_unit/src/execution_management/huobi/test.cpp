#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceHuobiTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceHuobi>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), std::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_HUOBI_API_KEY, "7f72bbdb-d3fa3d40-uymylwhfeg-17388"},
        {CCAPI_HUOBI_API_SECRET, "3e02e507-e8f8f2ae-a543363d-d2037"},
    };
    this->timestamp = "2017-05-11T15:19:30";
    this->now = UtilTime::parse(this->timestamp + "Z");
  }
  std::shared_ptr<ExecutionManagementServiceHuobi> service{nullptr};
  std::map<std::string, std::string> credential;
  std::string timestamp;
  TimePoint now{};
};

void verifyApiKeyEtc(const std::map<std::string, std::string> queryParamMap, const std::string& apiKey, const std::string& timestamp) {
  EXPECT_EQ(queryParamMap.at("AccessKeyId"), apiKey);
  EXPECT_EQ(queryParamMap.at("SignatureMethod"), "HmacSHA256");
  EXPECT_EQ(queryParamMap.at("SignatureVersion"), "2");
  EXPECT_EQ(Url::urlDecode(queryParamMap.at("Timestamp")), timestamp);
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  std::string preSignedText;
  preSignedText += std::string(req.method_string());
  preSignedText += "\n";
  preSignedText += "api.huobi.pro";
  preSignedText += "\n";
  auto splitted = UtilString::split(req.target().to_string(), "?");
  preSignedText += splitted.at(0);
  preSignedText += "\n";
  std::map<std::string, std::string> queryParamMap = Url::convertQueryStringToMap(splitted.at(1));
  std::string queryString;
  std::string signature = queryParamMap.at("Signature");
  queryParamMap.erase("Signature");
  int i = 0;
  for (const auto& kv : queryParamMap) {
    queryString += kv.first;
    queryString += "=";
    queryString += Url::urlEncode(kv.second);
    if (i < queryParamMap.size() - 1) {
      queryString += "&";
    }
    i++;
  }
  preSignedText += queryString;
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText)), signature);
}

TEST_F(ExecutionManagementServiceHuobiTest, signRequest) {
  http::request<http::string_body> req;
  req.method(http::verb::post);
  req.set(http::field::host, "api.huobi.pro");
  std::string path = "/v1/order/orders/place";
  std::map<std::string, std::string> queryParamMap = {
      {"AccessKeyId", this->credential.at(CCAPI_HUOBI_API_KEY)},
      {"SignatureMethod", "HmacSHA256"},
      {"SignatureVersion", "2"},
      {"Timestamp", "2021-01-14T00%3A05%3A31"},
  };
  this->service->signRequest(req, path, queryParamMap, this->credential);
  EXPECT_EQ(Url::urlDecode(Url::convertQueryStringToMap(UtilString::split(req.target().to_string(), "?").at(1)).at("Signature")),
            "Ns/D8rLOXixLe3yU3pRl4EhCjI0R4KckPMOIu6VpWmE=");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "10.1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "100.1"},
      {CCAPI_EM_ACCOUNT_ID, "100009"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["account-id"].GetString()), "100009");
  EXPECT_EQ(std::string(document["amount"].GetString()), "10.1");
  EXPECT_EQ(std::string(document["price"].GetString()), "100.1");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "btcusdt");
  EXPECT_EQ(std::string(document["type"].GetString()), "buy-limit");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/orders/place");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "data": "59378"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "59378");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "59378"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/orders/59378/submitcancel");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "a0001"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["client-order-id"].GetString()), "a0001");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/orders/submitCancelClientOrder");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "data": "59378"
  }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "59378");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "59378"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/orders/59378");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "a0001"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/orders/getClientOrder");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("clientOrderId"), "a0001");
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "ethusdt", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "data": {
      "id": 59378,
      "symbol": "ethusdt",
      "account-id": 100009,
      "amount": "10.1000000000",
      "price": "100.1000000000",
      "created-at": 1494901162595,
      "type": "buy-limit",
      "field-amount": "10.1000000000",
      "field-cash-amount": "1011.0100000000",
      "field-fees": "0.0202000000",
      "finished-at": 1494901400468,
      "user-id": 1000,
      "source": "api",
      "state": "filled",
      "canceled-at": 0
    }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "59378");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "10.1000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "100.1000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "10.1000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "1011.0100000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "filled");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  request.appendParam({
      {"ACCOUNT_ID", "100009"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/openOrders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "btcusdt");
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HUOBI, "ethusdt", "", this->credential);
  std::string textMessage =
      R"(
  {
    "data": [
      {
        "id": 5454937,
        "symbol": "ethusdt",
        "account-id": 30925,
        "amount": "1.000000000000000000",
        "price": "0.453000000000000000",
        "created-at": 1530604762277,
        "type": "sell-limit",
        "filled-amount": "0.0",
        "filled-cash-amount": "0.0",
        "filled-fees": "0.0",
        "source": "web",
        "state": "submitted"
      }
    ]
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5454937");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1.000000000000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.453000000000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "0.0");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  request.appendParam({
      {CCAPI_EM_ACCOUNT_ID, "100009"},
  });
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["symbol"].GetString()), "btcusdt");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/order/orders/batchCancelOpenOrders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "status":"ok",
          "data":{
              "success-count":2,
              "failed-count":0,
              "next-id":5454600
          }
      }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_HUOBI, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/account/accounts");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_HUOBI, "", "foo", this->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        [
        {
    "id": 100009,
    "type": "spot",
    "subtype": "",
    "state": "working"
  }]
  })";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNTS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "100009");
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_TYPE), "spot");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_HUOBI, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ACCOUNT_ID, "5bd6e9286d99522a52e458de"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/v1/account/accounts/5bd6e9286d99522a52e458de/balance");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_HUOBI, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
  "status": "ok",
  "data": {
    "id": 17469548,
    "type": "spot",
    "state": "working",
    "list": [
      {
        "currency": "lun",
        "type": "trade",
        "balance": "0"
      }
    ]
  }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "lun");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "0");
}

TEST_F(ExecutionManagementServiceHuobiTest, createEventTradeDetails) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
      "action": "push",
    "ch": "trade.clearing#btcusdt#0",
    "data": {
         "eventType": "trade",
         "symbol": "btcusdt",
         "orderId": 99998888,
         "tradePrice": "9999.99",
         "tradeVolume": "0.96",
         "orderSide": "buy",
         "aggressor": true,
         "tradeId": 919219323232,
         "tradeTime": 998787897878,
         "transactFee": "19.88",
         "feeDeduct ": "0",
         "feeDeductType": "",
         "feeCurrency": "btc",
         "accountId": 9912791,
         "source": "spot-api",
         "orderPrice": "10000",
         "orderSize": "1",
         "clientOrderId": "a001",
         "orderCreateTime": 998787897878,
         "orderStatus": "partial-filled"
    }
}
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "push", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "919219323232");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "9999.99");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "0.96");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "99998888");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "btcusdt");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY), "19.88");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_ASSET), "btc");
}

TEST_F(ExecutionManagementServiceHuobiTest, createEventOrderUpdatesTrigger) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"###(
    {
    "action":"push",
    "ch":"orders#btcusdt",
    "data":
    {
        "orderSide":"buy",
        "lastActTime":1583853365586,
        "clientOrderId":"abc123",
        "orderStatus":"rejected",
        "symbol":"btcusdt",
        "eventType":"trigger",
        "errCode": 2002,
        "errMessage":"invalid.client.order.id (NT)"
    }
}
)###";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "push", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "abc123");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "rejected");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "btcusdt");
}

TEST_F(ExecutionManagementServiceHuobiTest, createEventOrderUpdatesDeletion) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
    "action":"push",
    "ch":"orders#btcusdt",
    "data":
    {
        "orderSide":"buy",
        "lastActTime":1583853365586,
        "clientOrderId":"abc123",
        "orderStatus":"canceled",
        "symbol":"btcusdt",
        "eventType":"deletion"
    }
}
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "push", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "abc123");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "canceled");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "btcusdt");
}

TEST_F(ExecutionManagementServiceHuobiTest, createEventOrderUpdatesCreation) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
    "action":"push",
    "ch":"orders#btcusdt",
    "data":
    {
        "orderSize":"2.000000000000000000",
        "orderCreateTime":1583853365586,
        "accountld":992701,
        "orderPrice":"77.000000000000000000",
        "type":"sell-limit",
        "orderId":27163533,
        "clientOrderId":"abc123",
        "orderSource":"spot-api",
        "orderStatus":"submitted",
        "symbol":"btcusdt",
        "eventType":"creation"
    }
}
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "push", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "2.000000000000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "77.000000000000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "27163533");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "abc123");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "submitted");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "btcusdt");
}

TEST_F(ExecutionManagementServiceHuobiTest, createEventOrderUpdatesTrade) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
        "action":"push",
        "ch":"orders#btcusdt",
        "data":
        {
            "tradePrice":"76.000000000000000000",
            "tradeVolume":"1.013157894736842100",
            "tradeId":301,
            "tradeTime":1583854188883,
            "aggressor":true,
            "remainAmt":"0.000000000000000400000000000000000000",
            "execAmt":"2",
            "orderId":27163536,
            "type":"sell-limit",
            "clientOrderId":"abc123",
            "orderSource":"spot-api",
            "orderPrice":"15000",
            "orderSize":"0.01",
            "orderStatus":"filled",
            "symbol":"btcusdt",
            "eventType":"trade"
        }
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "push", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.01");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "15000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "27163536");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "abc123");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "filled");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "btcusdt");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0.000000000000000400000000000000000000");
}

TEST_F(ExecutionManagementServiceHuobiTest, createEventOrderUpdatesCancellation) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
        "action":"push",
        "ch":"orders#btcusdt",
        "data":
        {
            "lastActTime":1583853475406,
            "remainAmt":"2.000000000000000000",
            "execAmt":"2",
            "orderId":27163533,
            "type":"sell-limit",
            "clientOrderId":"abc123",
            "orderSource":"spot-api",
            "orderPrice":"15000",
            "orderSize":"0.01",
            "orderStatus":"canceled",
            "symbol":"btcusdt",
            "eventType":"cancellation"
        }
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "push", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.01");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "15000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "27163533");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "abc123");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "canceled");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "btcusdt");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "2.000000000000000000");
}
} /* namespace ccapi */
#endif
#endif
