#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_kraken.h"
namespace ccapi {
class ExecutionManagementServiceKrakenTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<ExecutionManagementServiceKraken>([](Event& event) {}, SessionOptions(), SessionConfigs(), wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_KRAKEN_API_KEY, "uvgK51G7bnksHUgrU++Cib03e15cCRJQA9e1f30TPuhZ+BagVrb2WUNi"},
        {CCAPI_KRAKEN_API_SECRET, "q+INlIikVemcqFtJu9CZk0QIXBMYRFpwKblA/N9iP61uGCMpsMa06ycI8VuwdxeqAvXnGPAnMIBYeiY1AoG67w=="},
    };
    this->timestamp = 1499827319000;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp);
  }
  std::shared_ptr<ExecutionManagementServiceKraken> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey) {
  EXPECT_EQ(req.base().at("API-Key").to_string(), apiKey);
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  std::string preSignedText = req.target().to_string();
  std::string body = req.body();
  std::string nonce = Url::convertFormUrlEncodedToMap(body).at("nonce");
  std::string noncePlusBodySha256;
  ExecutionManagementServiceKraken::computeHash(nonce + body, noncePlusBodySha256);
  preSignedText += noncePlusBodySha256;
  auto signature = req.base().at("API-SIGN").to_string();
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedText)), signature);
}

TEST_F(ExecutionManagementServiceKrakenTest, signRequest) {
  http::request<http::string_body> req;
  req.method(http::verb::post);
  req.target("/0/private/GetWebSocketsToken");
  std::string body("nonce=1631066678179");
  std::string nonce("1631066678179");
  this->service->signRequest(req, body, this->credential, nonce);
  EXPECT_EQ(req.base().at("API-SIGN").to_string(), "OU1XAxrMq+2tP5wEPYl6g8qOUWqXGLGZg/6EHYs0Rq+hhg8GYSAOUMJdih6jaImAfBPDzg8UFZyHfDy24T5/5A==");
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/AddOrder");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  EXPECT_EQ(bodyMap.at("pair"), "XXBTZUSD");
  EXPECT_EQ(bodyMap.at("type"), "buy");
  EXPECT_EQ(bodyMap.at("ordertype"), "limit");
  EXPECT_EQ(bodyMap.at("price"), "0.1");
  EXPECT_EQ(bodyMap.at("volume"), "1");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "error": [],
          "result": {
            "descr": {
              "order": "buy 2.12340000 XBTUSD @ limit 45000.1 with 2:1 leverage",
              "close": "close position @ stop loss 38000.0 -> limit 36000.0"
            },
            "txid": [
              "OUF4EM-FRGI2-MQMWZD"
            ]
          }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "OUF4EM-FRGI2-MQMWZD");
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "OYVGEW-VYV5B-UUEXSK"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/CancelOrder");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  EXPECT_EQ(bodyMap.at("txid"), "OYVGEW-VYV5B-UUEXSK");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "foo", this->credential);
  std::string textMessage =R"(
    {
      "error": [],
      "result": {
        "count": 1
      }
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "OYVGEW-VYV5B-UUEXSK"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/QueryOrders");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  EXPECT_EQ(bodyMap.at("txid"), "OYVGEW-VYV5B-UUEXSK");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "error": [],
          "result": {
            "OBCMZD-JIEE7-77TH3F": {
              "refid": null,
              "userref": 0,
              "status": "closed",
              "reason": null,
              "opentm": 1616665496.7808,
              "closetm": 1616665499.1922,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "buy",
                "ordertype": "limit",
                "price": "37500.0",
                "price2": "0",
                "leverage": "none",
                "order": "buy 1.25000000 XBTUSD @ limit 37500.0",
                "close": ""
              },
              "vol": "1.25000000",
              "vol_exec": "1.25000000",
              "cost": "37526.2",
              "fee": "37.5",
              "price": "30021.0",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fciq",
              "trades": [
                "TZX2WP-XSEOP-FP7WYR"
              ]
            },
            "OMMDB2-FSB6Z-7W3HPO": {
              "refid": null,
              "userref": 0,
              "status": "closed",
              "reason": null,
              "opentm": 1616592012.2317,
              "closetm": 1616592012.2335,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "sell",
                "ordertype": "market",
                "price": "0",
                "price2": "0",
                "leverage": "none",
                "order": "sell 0.25000000 XBTUSD @ market",
                "close": ""
              },
              "vol": "0.25000000",
              "vol_exec": "0.25000000",
              "cost": "7500.0",
              "fee": "7.5",
              "price": "30000.0",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fcib",
              "trades": [
                "TJUW2K-FLX2N-AR2FLU"
              ]
            }
          }
        }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "OBCMZD-JIEE7-77TH3F");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "37500.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1.25000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "1.25000000");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 30021.0*1.25000000);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "closed");
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/OpenOrders");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, verifyconvertTextMessageToMessageRestGetOpenOrders) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "", this->credential);
  std::string textMessage =
      R"(
        {
        "error": [],
        "result": {
          "open": {
            "OQCLML-BW3P3-BUCMWZ": {
              "refid": null,
              "userref": 0,
              "status": "open",
              "opentm": 1616666559.8974,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "buy",
                "ordertype": "limit",
                "price": "30010.0",
                "price2": "0",
                "leverage": "none",
                "order": "buy 1.25000000 XBTUSD @ limit 30010.0",
                "close": ""
              },
              "vol": "1.25000000",
              "vol_exec": "0.37500000",
              "cost": "11253.7",
              "fee": "0.00000",
              "price": "30010.0",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fciq",
              "trades": [
                "TCCCTY-WE2O6-P3NB37"
              ]
            },
            "OB5VMB-B4U2U-DK2WRW": {
              "refid": null,
              "userref": 120,
              "status": "open",
              "opentm": 1616665899.5699,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "buy",
                "ordertype": "limit",
                "price": "14500.0",
                "price2": "0",
                "leverage": "5:1",
                "order": "buy 0.27500000 XBTUSD @ limit 14500.0 with 5:1 leverage",
                "close": ""
              },
              "vol": "0.27500000",
              "vol_exec": "0.00000000",
              "cost": "0.00000",
              "fee": "0.00000",
              "price": "0.00000",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fciq"
            },
            "OXHXGL-F5ICS-6DIC67": {
              "refid": null,
              "userref": 120,
              "status": "open",
              "opentm": 1616665894.0036,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "buy",
                "ordertype": "limit",
                "price": "17500.0",
                "price2": "0",
                "leverage": "5:1",
                "order": "buy 0.27500000 XBTUSD @ limit 17500.0 with 5:1 leverage",
                "close": ""
              },
              "vol": "0.27500000",
              "vol_exec": "0.00000000",
              "cost": "0.00000",
              "fee": "0.00000",
              "price": "0.00000",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fciq"
            },
            "OLQCVY-B27XU-MBPCL5": {
              "refid": null,
              "userref": 251,
              "status": "open",
              "opentm": 1616665556.7646,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "buy",
                "ordertype": "limit",
                "price": "23500.0",
                "price2": "0",
                "leverage": "none",
                "order": "buy 0.27500000 XBTUSD @ limit 23500.0",
                "close": ""
              },
              "vol": "0.27500000",
              "vol_exec": "0.00000000",
              "cost": "0.00000",
              "fee": "0.00000",
              "price": "0.00000",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fciq"
            },
            "OQCGAF-YRMIQ-AMJTNJ": {
              "refid": null,
              "userref": 0,
              "status": "open",
              "opentm": 1616665511.0373,
              "starttm": 0,
              "expiretm": 0,
              "descr": {
                "pair": "XBTUSD",
                "type": "buy",
                "ordertype": "limit",
                "price": "24500.0",
                "price2": "0",
                "leverage": "none",
                "order": "buy 1.25000000 XBTUSD @ limit 24500.0",
                "close": ""
              },
              "vol": "1.25000000",
              "vol_exec": "0.00000000",
              "cost": "0.00000",
              "fee": "0.00000",
              "price": "0.00000",
              "stopprice": "0.00000",
              "limitprice": "0.00000",
              "misc": "",
              "oflags": "fciq"
            }
          }
        }
      }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 5);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "OQCLML-BW3P3-BUCMWZ");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1.25000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "30010.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.37500000");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0.37500000*30010.0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XBTUSD");
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/CancelAll");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN, "XXBTZUSD", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "error": [],
          "result": {
            "count": 4
          }
        }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/Balance");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "error": [],
          "result": {
            "ZUSD": "171288.6158",
            "ZEUR": "504861.8946"
          }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "ZUSD");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "171288.6158");
}

TEST_F(ExecutionManagementServiceKrakenTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_API_KEY));
  EXPECT_EQ(req.target(), "/0/private/OpenPositions");
  std::map<std::string, std::string> bodyMap = Url::convertFormUrlEncodedToMap(req.body());
  EXPECT_EQ(bodyMap.at("nonce"), std::to_string(this->timestamp));
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_KRAKEN, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
  "error": [],
  "result": {
    "TF5GVO-T7ZZ2-6NBKBI": {
      "ordertxid": "OLWNFG-LLH4R-D6SFFP",
      "posstatus": "open",
      "pair": "XXBTZUSD",
      "time": 1605280097.8294,
      "type": "buy",
      "ordertype": "limit",
      "cost": "104610.52842",
      "fee": "289.06565",
      "vol": "8.82412861",
      "vol_closed": "0.20200000",
      "margin": "20922.10568",
      "value": "258797.5",
      "net": "+154186.9728",
      "terms": "0.0100% per 4 hours",
      "rollovertm": "1616672637",
      "misc": "",
      "oflags": ""
    },
    "T24DOR-TAFLM-ID3NYP": {
      "ordertxid": "OIVYGZ-M5EHU-ZRUQXX",
      "posstatus": "open",
      "pair": "XXBTZUSD",
      "time": 1607943827.3172,
      "type": "buy",
      "ordertype": "limit",
      "cost": "145756.76856",
      "fee": "335.24057",
      "vol": "8.00000000",
      "vol_closed": "0.00000000",
      "margin": "29151.35371",
      "value": "240124.0",
      "net": "+94367.2314",
      "terms": "0.0100% per 4 hours",
      "rollovertm": "1616672637",
      "misc": "",
      "oflags": ""
    },
    "TYMRFG-URRG5-2ZTQSD": {
      "ordertxid": "OF5WFH-V57DP-QANDAC",
      "posstatus": "open",
      "pair": "XXBTZUSD",
      "time": 1610448039.8374,
      "type": "buy",
      "ordertype": "limit",
      "cost": "0.00240",
      "fee": "0.00000",
      "vol": "0.00000010",
      "vol_closed": "0.00000000",
      "margin": "0.00048",
      "value": "0",
      "net": "+0.0006",
      "terms": "0.0100% per 4 hours",
      "rollovertm": "1616672637",
      "misc": "",
      "oflags": ""
    },
    "TAFGBN-TZNFC-7CCYIM": {
      "ordertxid": "OF5WFH-V57DP-QANDAC",
      "posstatus": "open",
      "pair": "XXBTZUSD",
      "time": 1610448039.8448,
      "type": "buy",
      "ordertype": "limit",
      "cost": "2.40000",
      "fee": "0.00264",
      "vol": "0.00010000",
      "vol_closed": "0.00000000",
      "margin": "0.48000",
      "value": "3.0",
      "net": "+0.6015",
      "terms": "0.0100% per 4 hours",
      "rollovertm": "1616672637",
      "misc": "",
      "oflags": ""
    },
    "T4O5L3-4VGS4-IRU2UL": {
      "ordertxid": "OF5WFH-V57DP-QANDAC",
      "posstatus": "open",
      "pair": "XXBTZUSD",
      "time": 1610448040.7722,
      "type": "buy",
      "ordertype": "limit",
      "cost": "21.59760",
      "fee": "0.02376",
      "vol": "0.00089990",
      "vol_closed": "0.00000000",
      "margin": "4.31952",
      "value": "27.0",
      "net": "+5.4133",
      "terms": "0.0100% per 4 hours",
      "rollovertm": "1616672637",
      "misc": "",
      "oflags": ""
    }
  }
}
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_POSITIONS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 5);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_SYMBOL), "XXBTZUSD");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "8.62212861");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_COST), "104610.52842");
}

TEST_F(ExecutionManagementServiceKrakenTest, createEventOwnTrades) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KRAKEN, "XBT/EUR", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    [
      [
        {
          "TDLH43-DVQXD-2KHVYY": {
            "cost": "1000000.00000",
            "fee": "1600.00000",
            "margin": "0.00000",
            "ordertxid": "TDLH43-DVQXD-2KHVYY",
            "ordertype": "limit",
            "pair": "XBT/EUR",
            "postxid": "OGTT3Y-C6I3P-XRI6HX",
            "price": "100000.00000",
            "time": "1560516023.070651",
            "type": "sell",
            "vol": "1000000000.00000000"
          }
        },
        {
          "TDLH43-DVQXD-2KHVYY": {
            "cost": "1000000.00000",
            "fee": "600.00000",
            "margin": "0.00000",
            "ordertxid": "TDLH43-DVQXD-2KHVYY",
            "ordertype": "limit",
            "pair": "XBT/EUR",
            "postxid": "OGTT3Y-C6I3P-XRI6HX",
            "price": "100000.00000",
            "time": "1560516023.070658",
            "type": "buy",
            "vol": "1000000000.00000000"
          }
        },
        {
          "TDLH43-DVQXD-2KHVYY": {
            "cost": "1000000.00000",
            "fee": "1600.00000",
            "margin": "0.00000",
            "ordertxid": "TDLH43-DVQXD-2KHVYY",
            "ordertype": "limit",
            "pair": "XBT/EUR",
            "postxid": "OGTT3Y-C6I3P-XRI6HX",
            "price": "100000.00000",
            "time": "1560520332.914657",
            "type": "sell",
            "vol": "1000000000.00000000"
          }
        },
        {
          "TDLH43-DVQXD-2KHVYY": {
            "cost": "1000000.00000",
            "fee": "600.00000",
            "margin": "0.00000",
            "ordertxid": "TDLH43-DVQXD-2KHVYY",
            "ordertype": "limit",
            "pair": "XBT/USD",
            "postxid": "OGTT3Y-C6I3P-XRI6HX",
            "price": "100000.00000",
            "time": "1560520332.914664",
            "type": "buy",
            "vol": "1000000000.00000000"
          }
        }
      ],
      "ownTrades",
      {
        "sequence": 2948
      }
    ]
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 3);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "100000.00000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "1000000000.00000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "TDLH43-DVQXD-2KHVYY");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XBT/EUR");
}

TEST_F(ExecutionManagementServiceKrakenTest, createEventOpenOrders) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KRAKEN, "XBT/EUR", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    [
      [
        {
          "OGTT3Y-C6I3P-XRI6HX": {
            "cost": "0.00000",
            "descr": {
              "close": "",
              "leverage": "0:1",
              "order": "sell 10.00345345 XBT/EUR @ limit 34.50000 with 0:1 leverage",
              "ordertype": "limit",
              "pair": "XBT/EUR",
              "price": "34.50000",
              "price2": "0.00000",
              "type": "sell"
            },
            "expiretm": "0.000000",
            "fee": "0.00000",
            "limitprice": "34.50000",
            "misc": "",
            "oflags": "fcib",
            "opentm": "0.000000",
            "price": "34.50000",
            "refid": "OKIVMP-5GVZN-Z2D2UA",
            "starttm": "0.000000",
            "status": "open",
            "stopprice": "0.000000",
            "userref": 0,
            "vol": "10.00345345",
            "vol_exec": "0.00000000"
          }
        },
        {
          "OGTT3Y-C6I3P-XRI6HX": {
            "cost": "0.00000",
            "descr": {
              "close": "",
              "leverage": "0:1",
              "order": "sell 0.00000010 XBT/EUR @ limit 5334.60000 with 0:1 leverage",
              "ordertype": "limit",
              "pair": "XBT/EUR",
              "price": "5334.60000",
              "price2": "0.00000",
              "type": "sell"
            },
            "expiretm": "0.000000",
            "fee": "0.00000",
            "limitprice": "5334.60000",
            "misc": "",
            "oflags": "fcib",
            "opentm": "0.000000",
            "price": "5334.60000",
            "refid": "OKIVMP-5GVZN-Z2D2UA",
            "starttm": "0.000000",
            "status": "open",
            "stopprice": "0.000000",
            "userref": 0,
            "vol": "0.00000010",
            "vol_exec": "0.00000000"
          }
        },
        {
          "OGTT3Y-C6I3P-XRI6HX": {
            "cost": "0.00000",
            "descr": {
              "close": "",
              "leverage": "0:1",
              "order": "sell 0.00001000 XBT/EUR @ limit 90.40000 with 0:1 leverage",
              "ordertype": "limit",
              "pair": "XBT/EUR",
              "price": "90.40000",
              "price2": "0.00000",
              "type": "sell"
            },
            "expiretm": "0.000000",
            "fee": "0.00000",
            "limitprice": "90.40000",
            "misc": "",
            "oflags": "fcib",
            "opentm": "0.000000",
            "price": "90.40000",
            "refid": "OKIVMP-5GVZN-Z2D2UA",
            "starttm": "0.000000",
            "status": "open",
            "stopprice": "0.000000",
            "userref": 0,
            "vol": "0.00001000",
            "vol_exec": "0.00000000"
          }
        },
        {
          "OGTT3Y-C6I3P-XRI6HX": {
            "cost": "0.00000",
            "descr": {
              "close": "",
              "leverage": "0:1",
              "order": "sell 0.00001000 XBT/EUR @ limit 9.00000 with 0:1 leverage",
              "ordertype": "limit",
              "pair": "XBT/USD",
              "price": "9.00000",
              "price2": "0.00000",
              "type": "sell"
            },
            "expiretm": "0.000000",
            "fee": "0.00000",
            "limitprice": "9.00000",
            "misc": "",
            "oflags": "fcib",
            "opentm": "0.000000",
            "price": "9.00000",
            "refid": "OKIVMP-5GVZN-Z2D2UA",
            "starttm": "0.000000",
            "status": "open",
            "stopprice": "0.000000",
            "userref": 0,
            "vol": "0.00001000",
            "vol_exec": "0.00000000"
          }
        }
      ],
      "openOrders",
      {
        "sequence": 234
      }
    ]
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 3);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "OGTT3Y-C6I3P-XRI6HX");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "34.50000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "10.00345345");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "open");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XBT/EUR");
}
} /* namespace ccapi */
#endif
#endif
