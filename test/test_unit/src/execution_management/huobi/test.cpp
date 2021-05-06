#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi.h"
namespace ccapi {
class ExecutionManagementServiceHuobiTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceHuobi>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {{CCAPI_HUOBI_API_KEY, "7f72bbdb-d3fa3d40-uymylwhfeg-17388"}, {CCAPI_HUOBI_API_SECRET, "3e02e507-e8f8f2ae-a543363d-d2037"}};
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
  preSignedText += req.base().at(http::field::host).to_string();
  preSignedText += "\n";
  auto splitted = UtilString::split(req.target().to_string(), "?");
  preSignedText += splitted.at(0);
  preSignedText += "\n";
  std::map<std::string, std::string> queryParamMap = Url::convertQueryStringToMap(splitted.at(1));
  std::string queryString;
  std::string signature = Url::urlDecode(queryParamMap.at("Signature"));
  queryParamMap.erase("Signature");
  int i = 0;
  for (const auto& kv : queryParamMap) {
    queryString += kv.first;
    queryString += "=";
    queryString += kv.second;
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
  std::map<std::string, std::string> queryParamMap = {{"AccessKeyId", this->credential.at(CCAPI_HUOBI_API_KEY)},
                                                      {"SignatureMethod", "HmacSHA256"},
                                                      {"SignatureVersion", "2"},
                                                      {"Timestamp", "2021-01-14T00%3A05%3A31"}};
  this->service->signRequest(req, path, queryParamMap, this->credential);
  EXPECT_EQ(Url::urlDecode(Url::convertQueryStringToMap(UtilString::split(req.target().to_string(), "?").at(1)).at("Signature")),
            "Ns/D8rLOXixLe3yU3pRl4EhCjI0R4KckPMOIu6VpWmE=");
}

TEST_F(ExecutionManagementServiceHuobiTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_HUOBI, "btcusdt", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
                                           {CCAPI_EM_ORDER_QUANTITY, "10.1"},
                                           {CCAPI_EM_ORDER_LIMIT_PRICE, "100.1"},
                                           {CCAPI_EM_ACCOUNT_ID, "100009"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
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
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "59378"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "a0001"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
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
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "59378"}};
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
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "a0001"}};
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
      "filled-amount": "10.1000000000",
      "filled-cash-amount": "1011.0100000000",
      "filled-fees": "0.0202000000",
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
  request.appendParam({{"ACCOUNT_ID", "100009"}});
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
} /* namespace ccapi */
#endif
#endif
