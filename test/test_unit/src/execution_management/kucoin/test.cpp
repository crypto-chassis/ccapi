#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_kucoin.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceKucoinTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceKucoin>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                       std::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_KUCOIN_API_KEY, "60bb01032f6e210006b760a9"},
        {CCAPI_KUCOIN_API_SECRET, "2ebbd3aa-35af-4b5e-b2e5-df94bb940f83"},
        {CCAPI_KUCOIN_API_PASSPHRASE, "0x1a5y8koaa9"},
        {CCAPI_KUCOIN_API_KEY_VERSION, "2"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  std::shared_ptr<ExecutionManagementServiceKucoin> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, const std::string& apiSecret, const std::string& apiPassphrase,
                     const std::string& apiKeyVersion, long long timestamp) {
  EXPECT_EQ(req.base().at("KC-API-KEY").to_string(), apiKey);
  EXPECT_EQ(req.base().at("KC-API-KEY-VERSION").to_string(), apiKeyVersion);
  if (apiKeyVersion == "2") {
    EXPECT_EQ(req.base().at("KC-API-PASSPHRASE").to_string(), UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, apiPassphrase)));
  } else {
    EXPECT_EQ(req.base().at("KC-API-PASSPHRASE").to_string(), apiPassphrase);
  }
  EXPECT_EQ(req.base().at("KC-API-TIMESTAMP").to_string(), std::to_string(timestamp * 1000LL));
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = req.base().at("KC-API-TIMESTAMP").to_string();
  preSignedText += UtilString::toUpper(std::string(req.method_string()));
  preSignedText += req.target().to_string();
  preSignedText += req.body();
  auto signature = req.base().at("KC-API-SIGN").to_string();
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText)), signature);
}

TEST_F(ExecutionManagementServiceKucoinTest, signRequest) {
  http::request<http::string_body> req;
  req.set("KC-API-TIMESTAMP", "1623088896243");
  req.method(http::verb::post);
  req.target("/api/v1/orders");
  std::string body(R"({"price":"20000","size":"0.001","side":"buy","clientOid":"krJTxrv8r45d4nFRN4qrd3VDiOTVmenj","symbol":"BTC-USDT"})");
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("KC-API-SIGN").to_string(), "WgyYyAV8wtpK2QzPGrplb9fYCMti+B0iy/35v6uTxcM=");
}

TEST_F(ExecutionManagementServiceKucoinTest, signApiPassphrase) {
  http::request<http::string_body> req;
  this->service->signApiPassphrase(req, this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_SECRET));
  EXPECT_EQ(req.base().at("KC-API-PASSPHRASE").to_string(), "CYLB31MY6PQb8CqECms7sJpNjKEUv+p8DnKPm13kAKo=");
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.1"},
      {CCAPI_EM_CLIENT_ORDER_ID, "a"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target(), "/api/v1/orders");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["clientOid"].GetString()), "a");
  EXPECT_EQ(std::string(document["symbol"].GetString()), "BTC-USDT");
  EXPECT_EQ(std::string(document["side"].GetString()), "buy");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.1");
  EXPECT_EQ(std::string(document["size"].GetString()), "1");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        {
          "orderId": "5bd6e9286d99522a52e458de"
        }
  })";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CREATE_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5bd6e9286d99522a52e458de");
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "5bd6e9286d99522a52e458de"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/orders/5bd6e9286d99522a52e458de");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "6d539dc614db3"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/order/client-order/6d539dc614db3");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  auto messageList =
      this->service->convertTextMessageToMessageRest(request, R"({"code":"200000","data":{"cancelledOrderIds":["60bd907bdb1f44000685f1c3"]}})", this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  auto messageList = this->service->convertTextMessageToMessageRest(
      request, R"({"code":"200000","data":{"cancelledOrderId":"60bd90f6db1f44000686222c","clientOid":"60b941acd48f800006caf77"}})", this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "5c35c02703aa673ceec2a168"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/orders/5c35c02703aa673ceec2a168");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "6d539dc614db312"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/order/client-order/6d539dc614db312");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        {
          "id": "5c35c02703aa673ceec2a168",
          "symbol": "BTC-USDT",
          "opType": "DEAL",
          "type": "limit",
          "side": "buy",
          "price": "10",
          "size": "2",
          "funds": "0",
          "dealFunds": "0.166",
          "dealSize": "2",
          "fee": "0",
          "feeCurrency": "USDT",
          "stp": "",
          "stop": "",
          "stopTriggered": false,
          "stopPrice": "0",
          "timeInForce": "GTC",
          "postOnly": false,
          "hidden": false,
          "iceberg": false,
          "visibleSize": "0",
          "cancelAfter": 0,
          "channel": "IOS",
          "clientOid": "",
          "remark": "",
          "tags": "",
          "isActive": false,
          "cancelExist": false,
          "createdAt": 1547026471000,
          "tradeType": "TRADE"
       }
  })";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ORDER);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5c35c02703aa673ceec2a168");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "10");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "2");
  EXPECT_EQ(element.getValue("isActive"), "false");
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("status"), "active");
  EXPECT_EQ(paramMap.at("symbol"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KUCOIN, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/orders");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

void verifyconvertTextMessageToMessageRestGetOpenOrders(const ExecutionManagementServiceKucoinTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "BTC-USDT" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KUCOIN, symbol, "", fixture->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        {
          "currentPage": 1,
          "pageSize": 1,
          "totalNum": 153408,
          "totalPage": 153408,
          "items": [
              {
                  "id": "5c35c02703aa673ceec2a168",
                  "symbol": "BTC-USDT",
                  "opType": "DEAL",
                  "type": "limit",
                  "side": "buy",
                  "price": "10",
                  "size": "2",
                  "funds": "0",
                  "dealFunds": "0.166",
                  "dealSize": "2",
                  "fee": "0",
                  "feeCurrency": "USDT",
                  "stp": "",
                  "stop": "",
                  "stopTriggered": false,
                  "stopPrice": "0",
                  "timeInForce": "GTC",
                  "postOnly": false,
                  "hidden": false,
                  "iceberg": false,
                  "visibleSize": "0",
                  "cancelAfter": 0,
                  "channel": "IOS",
                  "clientOid": "",
                  "remark": "",
                  "tags": "",
                  "isActive": false,
                  "cancelExist": false,
                  "createdAt": 1547026471000,
                  "tradeType": "TRADE"
              }
           ]
       }
  })";
  auto messageList = fixture->service->convertTextMessageToMessageRest(request, textMessage, fixture->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5c35c02703aa673ceec2a168");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "10");
  if (!isOneInstrument) {
    EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USDT");
  }
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, false);
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/v1/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("symbol"), "BTC-USDT");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KUCOIN, "BTC-USDT", "foo", this->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        {
            "cancelledOrderIds": [
                "5c52e11203aa677f33e493fb",
                "5c52e12103aa677f33e493fe",
                "5c52e12a03aa677f33e49401",
                "5c52e1be03aa677f33e49404",
                "5c52e21003aa677f33e49407",
                "5c6243cb03aa67580f20bf2f",
                "5c62443703aa67580f20bf32",
                "5c6265c503aa676fee84129c",
                "5c6269e503aa676fee84129f",
                "5c626b0803aa676fee8412a2"
            ]
        }
  })";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_KUCOIN, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/accounts");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_KUCOIN, "", "foo", this->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        [
          {
            "id": "5bd6e9286d99522a52e458de",
            "currency": "BTC",
            "type": "main",
            "balance": "237582.04299",
            "available": "237582.032",
            "holds": "0.01099"
          },
          {
            "id": "5bd6e9216d99522a52e458d6",
            "currency": "BTC",
            "type": "trade",
            "balance": "1234356",
            "available": "1234356",
            "holds": "0"
        }]
  })";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNTS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "5bd6e9286d99522a52e458de");
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_TYPE), "main");
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "BTC");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "237582.032");
}

TEST_F(ExecutionManagementServiceKucoinTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_KUCOIN, "", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ACCOUNT_ID, "5bd6e9286d99522a52e458de"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KUCOIN_API_KEY), this->credential.at(CCAPI_KUCOIN_API_SECRET),
                  this->credential.at(CCAPI_KUCOIN_API_PASSPHRASE), this->credential.at(CCAPI_KUCOIN_API_KEY_VERSION), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/v1/accounts/5bd6e9286d99522a52e458de");
  verifySignature(req, this->credential.at(CCAPI_KUCOIN_API_SECRET));
}

TEST_F(ExecutionManagementServiceKucoinTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_KUCOIN, "", "foo", this->credential);
  std::string textMessage =
      R"({"code":"200000","data":
        {
            "currency": "KCS",
            "balance": "1000000060.6299",
            "available": "1000000060.6299",
            "holds": "0"
        }
  })";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_BALANCES);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "KCS");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "1000000060.6299");
}

TEST_F(ExecutionManagementServiceKucoinTest, createEventOpen) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KUCOIN, "KCS-USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
        "type":"message",
        "topic":"/spotMarket/tradeOrders",
        "subject":"orderChange",
        "channelType":"private",
        "data":{
            "symbol":"KCS-USDT",
            "orderType":"limit",
            "side":"buy",
            "orderId":"5efab07953bdea00089965d2",
            "type":"open",
            "orderTime":1593487481683297666,
            "size":"0.1",
            "filledSize":"0",
            "price":"0.937",
            "clientOid":"1593487481000906",
            "remainSize":"0.1",
            "status":"open",
            "ts":1593487481683297666
        }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5efab07953bdea00089965d2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.937");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "open");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "KCS-USDT");
}

TEST_F(ExecutionManagementServiceKucoinTest, createEventMatch) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KUCOIN, "KCS-USDT", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
      "type":"message",
      "topic":"/spotMarket/tradeOrders",
      "subject":"orderChange",
      "channelType":"private",
      "data":{
          "symbol":"KCS-USDT",
          "orderType":"limit",
          "side":"sell",
          "orderId":"5efab07953bdea00089965fa",
          "liquidity":"taker",
          "type":"match",
          "orderTime":1593487482038606180,
          "size":"0.1",
          "filledSize":"0.1",
          "price":"0.938",
          "matchPrice":"0.96738",
          "matchSize":"0.1",
          "tradeId":"5efab07a4ee4c7000a82d6d9",
          "clientOid":"1593487481000313",
          "remainSize":"0",
          "status":"match",
          "ts":1593487482038606180
      }
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
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "5efab07a4ee4c7000a82d6d9");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "0.96738");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5efab07953bdea00089965fa");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "KCS-USDT");
}

TEST_F(ExecutionManagementServiceKucoinTest, createEventFilled) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KUCOIN, "KCS-USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
      "type":"message",
      "topic":"/spotMarket/tradeOrders",
      "subject":"orderChange",
      "channelType":"private",
      "data":{
          "symbol":"KCS-USDT",
          "orderType":"limit",
          "side":"sell",
          "orderId":"5efab07953bdea00089965fa",
          "type":"filled",
          "orderTime":1593487482038606180,
          "size":"0.1",
          "filledSize":"0.1",
          "price":"0.938",
          "clientOid":"1593487481000313",
          "remainSize":"0",
          "status":"done",
          "ts":1593487482038606180
      }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5efab07953bdea00089965fa");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.938");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "done");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "KCS-USDT");
}

TEST_F(ExecutionManagementServiceKucoinTest, createEventCanceled) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KUCOIN, "KCS-USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
    "type":"message",
    "topic":"/spotMarket/tradeOrders",
    "subject":"orderChange",
    "channelType":"private",
    "data":{
        "symbol":"KCS-USDT",
        "orderType":"limit",
        "side":"buy",
        "orderId":"5efab07953bdea00089965d2",
        "type":"canceled",
        "orderTime":1593487481683297666,
        "size":"0.1",
        "filledSize":"0",
        "price":"0.937",
        "clientOid":"1593487481000906",
        "remainSize":"0",
        "status":"done",
        "ts":1593487481893140844
    }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5efab07953bdea00089965d2");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.937");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "done");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "KCS-USDT");
}

TEST_F(ExecutionManagementServiceKucoinTest, createEventChange) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KUCOIN, "KCS-USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
      "type":"message",
      "topic":"/spotMarket/tradeOrders",
      "subject":"orderChange",
      "channelType":"private",
      "data":{
          "symbol":"KCS-USDT",
          "orderType":"limit",
          "side":"buy",
          "orderId":"5efab13f53bdea00089971df",
          "type":"update",
          "oldSize":"0.1",
          "orderTime":1593487679693183319,
          "size":"0.06",
          "filledSize":"0",
          "price":"0.937",
          "clientOid":"1593487679000249",
          "remainSize":"0.06",
          "status":"open",
          "ts":1593487682916117521
      }
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "5efab13f53bdea00089971df");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.937");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "0.06");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0.06");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "open");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "KCS-USDT");
}

} /* namespace ccapi */
#endif
#endif
