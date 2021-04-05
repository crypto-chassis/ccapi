#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_erisx.h"
namespace ccapi {
class ExecutionManagementServiceErisxTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceErisx>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {{CCAPI_ERISX_API_KEY, "6e010dda31cc2f301c82de1eb82d0998gbbec9fe6f9438d788416d23fc56b14d4"},
                        {CCAPI_ERISX_API_SECRET, "3zMnjHV5B1nxsfh6b8Jx7AdHZHiw"}};
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  std::shared_ptr<ExecutionManagementServiceErisx> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyJwt(const http::request<http::string_body>& req, const std::string& apiKey, const std::string& apiSecret, long long timestamp) {
  auto authorizationHeader = req.base().at("Authorization").to_string();
  std::string toErase = "Bearer ";
  auto pos = authorizationHeader.find(toErase);
  authorizationHeader.erase(pos, toErase.length());
  auto splitted = UtilString::split(authorizationHeader, ".");
  auto jwtHeader = UtilAlgorithm::base64UrlDecode(splitted.at(0));
  rj::Document documentHeader;
  documentHeader.Parse(jwtHeader.c_str());
  EXPECT_EQ(std::string(documentHeader["alg"].GetString()), "HS256");
  EXPECT_EQ(std::string(documentHeader["typ"].GetString()), "JWT");
  auto jwtPayload = UtilAlgorithm::base64UrlDecode(splitted.at(1));
  rj::Document documentPayload;
  documentPayload.Parse(jwtPayload.c_str());
  EXPECT_EQ(std::string(documentPayload["sub"].GetString()), apiKey);
  EXPECT_EQ(documentPayload["iat"].GetInt64(), timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, signRequest) {
  http::request<http::string_body> req;
  this->service->signRequest(req, this->now, this->credential);
  verifyJwt(req, this->credential.at(CCAPI_ERISX_API_KEY), this->credential.at(CCAPI_ERISX_API_SECRET), this->timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_ERISX, "BTC/USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
                                           {CCAPI_EM_ORDER_QUANTITY, "1"},
                                           {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"},
                                           {CCAPI_EM_CLIENT_ORDER_ID, "ENRY34D6CVV-a"},
                                           {CCAPI_EM_PARTY_ID, "ENRY34D6CVV"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/rest-api/new-order-single");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["clOrdID"].GetString()), "ENRY34D6CVV-a");
  EXPECT_EQ(std::string(document["currency"].GetString()), "BTC");
  EXPECT_EQ(std::string(document["side"].GetString()), "BUY");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTC/USD");
  EXPECT_EQ(std::string(document["partyID"].GetString()), "ENRY34D6CVV");
  EXPECT_EQ(std::string(document["transactionTime"].GetString()), "20170712-02:41:59.000");
  EXPECT_EQ(std::string(document["partyID"].GetString()), "ENRY34D6CVV");
  EXPECT_EQ(std::string(document["orderQty"].GetString()), "1");
  EXPECT_EQ(std::string(document["ordType"].GetString()), "LIMIT");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.1");
  verifyJwt(req, this->credential.at(CCAPI_ERISX_API_KEY), this->credential.at(CCAPI_ERISX_API_SECRET), this->timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, convertTextMessageToMessageCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_ERISX, "BTC/USD", "foo", this->credential);
  std::string textMessage =
      R"(
  {
   "correlation": "134582149605314482",
   "type": "ExecutionReport",
   "orderID": "281474982380221",
   "clOrdID": "Trader_A-15978608853912",
   "origClOrdID": "Trader_A-15978608853912",
   "execID": "281475002437073",
   "execType": "NEW",
   "ordStatus": "NEW",
   "ordRejReason": null,
   "account": "acc2",
   "symbol": "BTCU0",
   "side": "BUY",
   "orderQty": 2.0,
   "ordType": "LIMIT",
   "price": 8500.0,
   "stopPrice": 0.0,
   "currency": "BTC",
   "lastPrice": 0.0,
   "lastSpotRate": 0.0,
   "leavesQty": 2.0,
   "cumQty": 0.0,
   "avgPrice": 0.0,
   "tradeDate": null,
   "transactTime": "20200819-18:14:46.149268894",
   "sendingTime": "20200819-18:14:46.168",
   "commission": 0.0,
   "commCalculated": 0.2,
   "commType": "ABSOLUTE",
   "commCurrency": "USD",
   "minQty": 0.0,
   "text": null,
   "matchingType": null,
   "lastRptRequested": null,
   "timeInForce": "Day",
   "expireDate": "20200819",
   "lastQty": 0.0,
   "targetSubId": null,
   "targetLocationId": null,
   "custOrderCapacity": 1,
   "accountType": 2,
   "customerAccountRef": "tcr001",
   "postOnly": "N",
   "unsolicitedCancel": null,
   "availableBalanceData": [
   {
   "availableBalance": 92.34484,
   "availableBalanceCurrency": "BTC"
   },
   {
   "availableBalance": 1034806.1788,
   "availableBalanceCurrency": "USD"
   }
   ],
   "partyIDs": [
   "Trader_A"
   ]
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "281474982380221");
}

TEST_F(ExecutionManagementServiceErisxTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_ERISX, "BTC/USD", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "281474982380221"},
                                           {CCAPI_EM_ORDER_SIDE, "BUY"},
                                           {CCAPI_EM_CLIENT_ORDER_ID, "ENRY34D6CVV-b"},
                                           {CCAPI_EM_ORIGINAL_CLIENT_ORDER_ID, "ENRY34D6CVV-a"},
                                           {CCAPI_EM_PARTY_ID, "ENRY34D6CVV"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target(), "/rest-api/cancel-order-single");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["clOrdID"].GetString()), "ENRY34D6CVV-b");
  EXPECT_EQ(std::string(document["origClOrdID"].GetString()), "ENRY34D6CVV-a");
  EXPECT_EQ(std::string(document["orderID"].GetString()), "281474982380221");
  EXPECT_EQ(std::string(document["side"].GetString()), "BUY");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTC/USD");
  EXPECT_EQ(std::string(document["clOrdID"].GetString()), "ENRY34D6CVV-b");
  EXPECT_EQ(std::string(document["partyID"].GetString()), "ENRY34D6CVV");
  EXPECT_EQ(std::string(document["transactionTime"].GetString()), "20170712-02:41:59.000");
  EXPECT_EQ(std::string(document["ordType"].GetString()), "LIMIT");
  verifyJwt(req, this->credential.at(CCAPI_ERISX_API_KEY), this->credential.at(CCAPI_ERISX_API_SECRET), this->timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, convertTextMessageToMessageCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_ERISX, "BTC/USD", "foo", this->credential);
  std::string textMessage =
      R"(
  {
   "correlation": "134582149605314482",
   "type": "ExecutionReport",
   "orderID": "281474982380221",
   "clOrdID": "Trader_A-15978611385294",
   "origClOrdID": "Trader_A-15978610308013",
   "execID": "281475002437077",
   "execType": "CANCELED",
   "ordStatus": "CANCELED",
   "ordRejReason": null,
   "account": "acc2",
   "symbol": "BTCU0",
   "side": "BUY",
   "orderQty": 3.0,
   "ordType": "LIMIT",
   "price": 8501.0,
   "stopPrice": 0.0,
   "currency": "BTC",
   "lastPrice": 0.0,
   "lastSpotRate": 0.0,
   "leavesQty": 0.0,
   "cumQty": 0.0,
   "avgPrice": 0.0,
   "tradeDate": null,
   "transactTime": "20200819-18:18:59.260000000",
   "sendingTime": "20200819-18:18:59.279",
   "commission": 0.0,
   "commCalculated": 0.0,
   "commType": "ABSOLUTE",
   "commCurrency": "USD",
   "minQty": 0.0,
   "text": "USER INITIATED",
   "matchingType": null,
   "lastRptRequested": null,
   "timeInForce": "Day",
   "expireDate": "20200819",
   "lastQty": 0.0,
   "targetSubId": null,
   "targetLocationId": null,
   "custOrderCapacity": 1,
   "accountType": 2,
   "customerAccountRef": "tcr001",
   "postOnly": "N",
   "unsolicitedCancel": null,
   "availableBalanceData": [
   {
   "availableBalance": 92.34484,
   "availableBalanceCurrency": "BTC"
   },
   {
   "availableBalance": 1036506.3788,
   "availableBalanceCurrency": "USD"
   }
   ],
   "partyIDs": [
   "Trader_A"
   ]
  }
  )";
  auto messageList = this->service->convertTextMessageToMessage(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceErisxTest, convertRequestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_ERISX, "", "foo", this->credential);
  std::map<std::string, std::string> param{{CCAPI_EM_ORDER_ID, "281474982380221"}, {CCAPI_EM_PARTY_ID, "ENRY34D6CVV"}};
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  EXPECT_EQ(req.target().to_string(), "/rest-api/order/ENRY34D6CVV/281474982380221");
  verifyJwt(req, this->credential.at(CCAPI_ERISX_API_KEY), this->credential.at(CCAPI_ERISX_API_SECRET), this->timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, convertTextMessageToMessageGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_ERISX, "", "foo", this->credential);
  std::string textMessage =
      R"(
  {
   "correlation": "134582149605314482",
   "type": "ExecutionReport",
   "orderID": "281474982380221",
   "clOrdID": "Trader_A-15978611385294",
   "origClOrdID": "Trader_A-15978610308013",
   "execID": "281475002437077",
   "execType": "CANCELED",
   "ordStatus": "CANCELED",
   "ordRejReason": null,
   "account": "acc2",
   "symbol": "BTCU0",
   "side": "BUY",
   "orderQty": 3.0,
   "ordType": "LIMIT",
   "price": 8501.0,
   "stopPrice": 0.0,
   "currency": "BTC",
   "lastPrice": 0.0,
   "lastSpotRate": 0.0,
   "leavesQty": 0.0,
   "cumQty": 0.0,
   "avgPrice": 0.0,
   "tradeDate": null,
   "transactTime": "20200819-18:18:59.260000000",
   "sendingTime": "20200819-18:18:59.279",
   "commission": 0.0,
   "commCalculated": 0.0,
   "commType": "ABSOLUTE",
   "commCurrency": "USD",
   "minQty": 0.0,
   "text": "USER INITIATED",
   "matchingType": null,
   "lastRptRequested": null,
   "timeInForce": "Day",
   "expireDate": "20200819",
   "lastQty": 0.0,
   "targetSubId": null,
   "targetLocationId": null,
   "custOrderCapacity": 1,
   "accountType": 2,
   "customerAccountRef": "tcr001",
   "postOnly": "N",
   "unsolicitedCancel": null,
   "availableBalanceData": [
   {
   "availableBalance": 92.34484,
   "availableBalanceCurrency": "BTC"
   },
   {
   "availableBalance": 1036506.3788,
   "availableBalanceCurrency": "USD"
   }
   ],
   "partyIDs": [
   "Trader_A"
   ]
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "281474982380221");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "3.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), CCAPI_EM_ORDER_STATUS_CLOSED);
}

TEST_F(ExecutionManagementServiceErisxTest, convertRequestGetOpenOrders) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_ERISX, "", "foo", this->credential);
  request.appendParam({{CCAPI_EM_PARTY_ID, "ENRY34D6CVV"}});
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target().to_string(), "/rest-api/order-mass-status");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["partyID"].GetString()), "ENRY34D6CVV");
  verifyJwt(req, this->credential.at(CCAPI_ERISX_API_KEY), this->credential.at(CCAPI_ERISX_API_SECRET), this->timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, convertTextMessageToMessageGetOpenOrders) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_ERISX, "", "", this->credential);
  std::string textMessage =
      R"(
  {
   "type": "MassOrderStatus",
   "correlation": "foo678",
   "orderStatuses": [
   {
   "correlation": "134582149605314482",
   "type": "ExecutionReport",
   "orderID": "281474982380342",
   "clOrdID": "Trader_A-1597861577927220",
   "origClOrdID": "Trader_A-1597861577927220",
   "execID": "281475002437465",
   "execType": "ORDER_STATUS",
   "ordStatus": "NEW",
   "ordRejReason": null,
   "account": "acc1",
   "symbol": "BCH/USD",
   "side": "SELL",
   "orderQty": 0.1113,
   "ordType": "LIMIT",
   "price": 290.3,
   "stopPrice": 0.0,
   "currency": "BCH",
   "lastPrice": 0.0,
   "lastSpotRate": 0.0,
   "leavesQty": 0.1113,
   "cumQty": 0.0,
   "avgPrice": 0.0,
   "tradeDate": null,
   "transactTime": "20200819-18:26:16.706642846",
   "sendingTime": "20200819-18:26:16.726",
   "commission": 0.0,
   "commCalculated": 0.0646,
   "commType": "ABSOLUTE",
   "commCurrency": "USD",
   "minQty": 0.0,
   "text": null,
   "matchingType": null,
   "lastRptRequested": "N",
   "timeInForce": "Day",
   "expireDate": "20200819",
   "lastQty": 0.0,
   "targetSubId": null,
   "targetLocationId": null,
   "custOrderCapacity": 0,
   "accountType": 0,
   "customerAccountRef": "tcr001",
   "postOnly": "N",
   "unsolicitedCancel": null,
   "availableBalanceData": [
   {
   "availableBalance": 986.098,
   "availableBalanceCurrency": "BCH"
   },
   {
   "availableBalance": 2250638.6126,
   "availableBalanceCurrency": "USD"
   }
   ],
   "partyIDs": [
   "Trader_A"
   ]
   }
   ]
  }
  )";
  auto messageList = this->service->convertTextMessageToMessage(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "281474982380342");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.1113");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "290.3");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BCH/USD");
}

TEST_F(ExecutionManagementServiceErisxTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_ERISX, "", "foo", this->credential);
  request.appendParam({{"PARTY_ID", "ENRY34D6CVV"}});
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  EXPECT_EQ(req.target().to_string(), "/rest-api/cancel-all");
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["partyID"].GetString()), "ENRY34D6CVV");
  verifyJwt(req, this->credential.at(CCAPI_ERISX_API_KEY), this->credential.at(CCAPI_ERISX_API_SECRET), this->timestamp);
}

TEST_F(ExecutionManagementServiceErisxTest, convertTextMessageToMessageCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_ERISX, "", "foo", this->credential);
  std::string textMessage =
      R"(
  {
   "correlation": "5535410536258384820",
   "type": "CancelAllOrdersResponse",
   "partyId": "trader1",
   "message": "Accepted"
  }
  )";
  auto messageList = this->service->convertTextMessageToMessage(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}
} /* namespace ccapi */
#endif
#endif
