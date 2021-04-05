#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_us.h"
namespace ccapi {
class ExecutionManagementServiceBinanceUsTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceBinanceUs>([](Event& event) {}, SessionOptions(), SessionConfigs(),
                                                                          wspp::lib::make_shared<ServiceContext>());
    this->credential = {{CCAPI_BINANCE_US_API_KEY, "vmPUZE6mv9SD5VNHk4HlWFsOr6aKE2zvsw0MuIgwCIPy6utIco14y7Ju91duEh8A"},
                        {CCAPI_BINANCE_US_API_SECRET, "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j"}};
    this->timestamp = 1499827319559;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp);
  }
  std::shared_ptr<ExecutionManagementServiceBinanceUs> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKey(const http::request<http::string_body>& req, const std::string& apiKey) { EXPECT_EQ(req.base().at("X-MBX-APIKEY").to_string(), apiKey); }

void verifySignature(const std::string& paramString, const std::string& apiSecret) {
  auto pos = paramString.find_last_of("&");
  auto paramStringWithoutSignature = paramString.substr(0, pos);
  auto signature = paramString.substr(pos + 11, paramString.length() - pos - 1);
  EXPECT_EQ(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, paramStringWithoutSignature, true), signature);
}

TEST_F(ExecutionManagementServiceBinanceUsTest, signRequest) {
  std::string queryString = "symbol=LTCBTC&side=BUY&type=LIMIT&timeInForce=GTC&quantity=1&price=0.1&recvWindow=5000&timestamp=1499827319559";
  this->service->signRequest(queryString, {{"timestamp", "1499827319559"}}, this->now, this->credential);
  EXPECT_EQ(queryString,
            "symbol=LTCBTC&side=BUY&type=LIMIT&timeInForce=GTC&quantity=1&price=0.1&recvWindow=5000&timestamp=1499827319559&"
            "signature=c8db56825ae71d6d79447849e617115f4a920fa2acdcab2b053c4b2838bd6b71");
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY}, {CCAPI_EM_ORDER_QUANTITY, "1"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSD");
  EXPECT_EQ(paramMap.at("side"), "BUY");
  EXPECT_EQ(paramMap.at("type"), "LIMIT");
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertTextMessageToMessageCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSDT", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "symbol": "BTCUSDT",
    "orderId": 28,
    "orderListId": -1,
    "clientOrderId": "6gCrw2kRUAF9CvJDGP16IP",
    "transactTime": 1507725176595
  }
  )";
  auto messageList = this->service->convertTextMessageToMessage(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CREATE_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "28");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "6gCrw2kRUAF9CvJDGP16IP");
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "28"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("orderId"), "28");
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSD");
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "6gCrw2kRUAF9CvJDGP16IP"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("origClientOrderId"), "6gCrw2kRUAF9CvJDGP16IP");
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSD");
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertTextMessageToMessageCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  auto messageList = this->service->convertTextMessageToMessage(request, "{}", this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "28"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("orderId"), "28");
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSD");
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_CLIENT_ORDER_ID, "6gCrw2kRUAF9CvJDGP16IP"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("origClientOrderId"), "6gCrw2kRUAF9CvJDGP16IP");
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSD");
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertTextMessageToMessageGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_US, "LTCBTC", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "symbol": "LTCBTC",
    "orderId": 1,
    "orderListId": -1,
    "clientOrderId": "myOrder1",
    "price": "0.1",
    "origQty": "1.0",
    "executedQty": "0.0",
    "cummulativeQuoteQty": "0.01",
    "status": "NEW",
    "timeInForce": "GTC",
    "type": "LIMIT",
    "side": "BUY",
    "stopPrice": "0.0",
    "icebergQty": "0.0",
    "time": 1499827319559,
    "updateTime": 1499827319559,
    "isWorking": true,
    "origQuoteOrderQty": "0.000000"
  }
  )";
  auto messageList = this->service->convertTextMessageToMessage(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "myOrder1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "0.01");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), CCAPI_EM_ORDER_STATUS_OPEN);
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/openOrders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "BTCUSD");
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BINANCE_US, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/openOrders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

void verifyconvertTextMessageToMessageGetOpenOrders(const ExecutionManagementServiceBinanceUsTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "LTCBTC" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BINANCE_US, symbol, "", fixture->credential);
  std::string textMessage =
      R"(
  [
    {
      "symbol": "LTCBTC",
      "orderId": 1,
      "orderListId": -1,
      "clientOrderId": "myOrder1",
      "price": "0.1",
      "origQty": "1.0",
      "executedQty": "0.0",
      "cummulativeQuoteQty": "0.01",
      "status": "NEW",
      "timeInForce": "GTC",
      "type": "LIMIT",
      "side": "BUY",
      "stopPrice": "0.0",
      "icebergQty": "0.0",
      "time": 1499827319559,
      "updateTime": 1499827319559,
      "isWorking": true,
      "origQuoteOrderQty": "0.000000"
    }
  ]
  )";
  auto messageList = fixture->service->convertTextMessageToMessage(request, textMessage, fixture->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "myOrder1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "0.01");
  if (!isOneInstrument) {
    EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "LTCBTC");
  }
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertTextMessageToMessageGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertTextMessageToMessageGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageGetOpenOrders(this, false);
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_US_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v3/openOrders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_US_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsTest, convertTextMessageToMessageCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_BINANCE_US, "BTCUSD", "foo", this->credential);
  auto messageList = this->service->convertTextMessageToMessage(request, "[]", this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}
} /* namespace ccapi */
#endif
#endif
