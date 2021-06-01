#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_gemini.h"
namespace ccapi {
class ExecutionManagementServiceGeminiTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceGemini>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_GEMINI_API_KEY, "account-DgM6GKGlzWtjOrdWiUyh"},
        {CCAPI_GEMINI_API_SECRET, "3zMnjHV5B1nxsfh6b8Jx7AdHZHiw"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  std::shared_ptr<ExecutionManagementServiceGemini> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey) {
  EXPECT_EQ(req.base().at("Cache-Control").to_string(), "no-cache");
  EXPECT_EQ(req.base().at("Content-Length").to_string(), "0");
  EXPECT_EQ(req.base().at("Content-Type").to_string(), "text/plain");
  EXPECT_EQ(req.base().at("X-GEMINI-APIKEY").to_string(), apiKey);
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto base64Payload = req.base().at("X-GEMINI-PAYLOAD").to_string();
  auto signature = req.base().at("X-GEMINI-SIGNATURE").to_string();
  EXPECT_EQ(UtilString::toLower(Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, base64Payload, true)), signature);
}

TEST_F(ExecutionManagementServiceGeminiTest, signRequest) {
  http::request<http::string_body> req;
  std::string body("{\"request\":\"/v1/order/status\",\"nonce\":123456,\"order_id\":18834}");
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("X-GEMINI-PAYLOAD").to_string(), "eyJyZXF1ZXN0IjoiL3YxL29yZGVyL3N0YXR1cyIsIm5vbmNlIjoxMjM0NTYsIm9yZGVyX2lkIjoxODgzNH0=");
  EXPECT_EQ(UtilString::toLower(req.base().at("X-GEMINI-SIGNATURE").to_string()),
            "7c3929f78a3b4cb6c14c1ca76e851fb095d2aad9a224e0b94a9bca45d0dac6b36e7923d0e4b9469706379ec589249783");
}

TEST_F(ExecutionManagementServiceGeminiTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "btcusd", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GEMINI_API_KEY));
  EXPECT_EQ(req.target(), "/v1/order/new");
  rj::Document document;
  document.Parse(UtilAlgorithm::base64Decode(req.base().at("X-GEMINI-PAYLOAD").to_string()).c_str());
  EXPECT_EQ(std::string(document["request"].GetString()), "/v1/order/new");
  EXPECT_EQ(document["nonce"].GetInt64(), this->timestamp * 1000LL);
  EXPECT_EQ(std::string(document["symbol"].GetString()), "btcusd");
  EXPECT_EQ(std::string(document["amount"].GetString()), "1");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  verifySignature(req, this->credential.at(CCAPI_GEMINI_API_SECRET));
}

TEST_F(ExecutionManagementServiceGeminiTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "btcusd", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "order_id": "19492382044",
    "id": "106817811",
    "symbol": "btcusd",
    "exchange": "gemini",
    "avg_execution_price": "3632.8508430064554",
    "side": "buy",
    "type": "exchange limit",
    "timestamp": "1547220404",
    "timestampms": 1547220404836,
    "is_live": true,
    "is_cancelled": false,
    "is_hidden": false,
    "was_forced": false,
    "executed_amount": "3.7567928949",
    "remaining_amount": "1.2432071051",
    "client_order_id": "20190110-4738721",
    "options": [],
    "price": "3633.00",
    "original_amount": "5"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "19492382044");
}

TEST_F(ExecutionManagementServiceGeminiTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "19492382044"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GEMINI_API_KEY));
  EXPECT_EQ(req.target(), "/v1/order/cancel");
  rj::Document document;
  document.Parse(UtilAlgorithm::base64Decode(req.base().at("X-GEMINI-PAYLOAD").to_string()).c_str());
  EXPECT_EQ(document["nonce"].GetInt64(), this->timestamp * 1000LL);
  EXPECT_EQ(document["order_id"].GetInt64(), 19492382044);
  EXPECT_EQ(std::string(document["request"].GetString()), "/v1/order/cancel");
  verifySignature(req, this->credential.at(CCAPI_GEMINI_API_SECRET));
}

TEST_F(ExecutionManagementServiceGeminiTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "order_id":"19492382044",
    "id":"106817811",
    "symbol":"btcusd",
    "exchange":"gemini",
    "avg_execution_price":"3632.85101103",
    "side":"buy",
    "type":"exchange limit",
    "timestamp":"1547220404",
    "timestampms":1547220404836,
    "is_live":false,
    "is_cancelled":true,
    "is_hidden":false,
    "was_forced":false,
    "executed_amount":"3.7610296649",
    "remaining_amount":"1.2389703351",
    "reason":"Requested",
    "options":[],
    "price":"3633.00",
    "original_amount":"5"
  }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceGeminiTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "19492382044"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GEMINI_API_KEY));
  EXPECT_EQ(req.target().to_string(), "/v1/order/status");
  rj::Document document;
  document.Parse(UtilAlgorithm::base64Decode(req.base().at("X-GEMINI-PAYLOAD").to_string()).c_str());
  EXPECT_EQ(document["nonce"].GetInt64(), this->timestamp * 1000LL);
  EXPECT_EQ(document["order_id"].GetInt64(), 19492382044);
  EXPECT_EQ(std::string(document["request"].GetString()), "/v1/order/status");
  verifySignature(req, this->credential.at(CCAPI_GEMINI_API_SECRET));
}

TEST_F(ExecutionManagementServiceGeminiTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "20190110-4738721"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GEMINI_API_KEY));
  EXPECT_EQ(req.target().to_string(), "/v1/order/status");
  rj::Document document;
  document.Parse(UtilAlgorithm::base64Decode(req.base().at("X-GEMINI-PAYLOAD").to_string()).c_str());
  EXPECT_EQ(document["nonce"].GetInt64(), this->timestamp * 1000LL);
  EXPECT_EQ(std::string(document["client_order_id"].GetString()), "20190110-4738721");
  EXPECT_EQ(std::string(document["request"].GetString()), "/v1/order/status");
  verifySignature(req, this->credential.at(CCAPI_GEMINI_API_SECRET));
}

TEST_F(ExecutionManagementServiceGeminiTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "order_id" : "19492382044",
    "id" : "44375901",
    "symbol" : "btcusd",
    "exchange" : "gemini",
    "avg_execution_price" : "400.00",
    "side" : "buy",
    "type" : "exchange limit",
    "timestamp" : "1494870642",
    "timestampms" : 1494870642156,
    "is_live" : false,
    "is_cancelled" : false,
    "is_hidden" : false,
    "was_forced" : false,
    "executed_amount" : "3",
    "remaining_amount" : "0",
    "options" : [],
    "price" : "400.00",
    "original_amount" : "3"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "19492382044");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "3");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "3");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 1200);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "is_not_live");
}

TEST_F(ExecutionManagementServiceGeminiTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GEMINI_API_KEY));
  EXPECT_EQ(req.target().to_string(), "/v1/orders");
  rj::Document document;
  document.Parse(UtilAlgorithm::base64Decode(req.base().at("X-GEMINI-PAYLOAD").to_string()).c_str());
  EXPECT_EQ(document["nonce"].GetInt64(), this->timestamp * 1000LL);
  verifySignature(req, this->credential.at(CCAPI_GEMINI_API_SECRET));
}

TEST_F(ExecutionManagementServiceGeminiTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GEMINI, "", "", this->credential);
  std::string textMessage =
      R"(
  [   {
    "order_id": "19492382044",
    "id": "107421210",
    "symbol": "ethusd",
    "exchange": "gemini",
    "avg_execution_price": "0.00",
    "side": "sell",
    "type": "exchange limit",
    "timestamp": "1547241628",
    "timestampms": 1547241628042,
    "is_live": true,
    "is_cancelled": false,
    "is_hidden": false,
    "was_forced": false,
    "executed_amount": "0",
    "remaining_amount": "1",
    "options": [],
    "price": "125.51",
    "original_amount": "1"
  },
  {
    "order_id": "107421205",
    "id": "107421205",
    "symbol": "ethusd",
    "exchange": "gemini",
    "avg_execution_price": "125.41",
    "side": "buy",
    "type": "exchange limit",
    "timestamp": "1547241626",
    "timestampms": 1547241626991,
    "is_live": true,
    "is_cancelled": false,
    "is_hidden": false,
    "was_forced": false,
    "executed_amount": "0.029147",
    "remaining_amount": "0.970853",
    "options": [],
    "price": "125.42",
    "original_amount": "1"
  } ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "19492382044");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "125.51");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "ethusd");
}

TEST_F(ExecutionManagementServiceGeminiTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_GEMINI_API_KEY));
  EXPECT_EQ(req.target().to_string(), "/v1/order/cancel/session");
  rj::Document document;
  document.Parse(UtilAlgorithm::base64Decode(req.base().at("X-GEMINI-PAYLOAD").to_string()).c_str());
  EXPECT_EQ(document["nonce"].GetInt64(), this->timestamp * 1000LL);
  EXPECT_EQ(std::string(document["request"].GetString()), "/v1/order/cancel/session");
  verifySignature(req, this->credential.at(CCAPI_GEMINI_API_SECRET));
}

TEST_F(ExecutionManagementServiceGeminiTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_GEMINI, "", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "result":"ok",
    "details":
        {
            "cancelledOrders":[19492382044],
            "cancelRejects":[]
        }
  }
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
