#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_ftx.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceFtxTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceFtx>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                    wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_FTX_API_KEY, "h3Pc-sRGCtWQANSbWi-6TzoOsLIkHD2_KbRvLULr"},
        {CCAPI_FTX_API_SECRET, "aJj_9jfURAz6JFtVKklvIIrvNRMIRfKUshUjfcYB"},
    };
    this->credential_2 = {
        {CCAPI_FTX_API_KEY, "h3Pc-sRGCtWQANSbWi-6TzoOsLIkHD2_KbRvLULr"},
        {CCAPI_FTX_API_SECRET, "aJj_9jfURAz6JFtVKklvIIrvNRMIRfKUshUjfcYB"},
        {CCAPI_FTX_API_SUBACCOUNT, "0x1a5y8koaa9"},
    };
    this->timestamp = 1499827319;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp * 1000LL);
  }
  std::shared_ptr<ExecutionManagementServiceFtx> service{nullptr};
  std::map<std::string, std::string> credential;
  std::map<std::string, std::string> credential_2;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey, const std::string& apiSubaccount, long long timestamp) {
  EXPECT_EQ(req.base().at("FTX-KEY").to_string(), apiKey);
  EXPECT_EQ(req.base().at("FTX-TS").to_string(), std::to_string(timestamp * 1000LL));
  if (!apiSubaccount.empty()) {
    EXPECT_EQ(req.base().at("FTX-SUBACCOUNT").to_string(), Url::urlEncode(apiSubaccount));
  } else {
    EXPECT_TRUE(req.base().find("FTX-SUBACCOUNT") == req.base().end());
  }
}

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto preSignedText = req.base().at("FTX-TS").to_string();
  preSignedText += UtilString::toUpper(std::string(req.method_string()));
  preSignedText += req.target().to_string();
  preSignedText += req.body();
  auto signature = req.base().at("FTX-SIGN").to_string();
  EXPECT_EQ(UtilString::toLower(UtilAlgorithm::stringToHex(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText))), signature);
}

TEST_F(ExecutionManagementServiceFtxTest, signRequest) {
  http::request<http::string_body> req;
  req.set("FTX-TS", "1622256962809");
  req.method(http::verb::post);
  req.target("/api/orders");
  std::string body = "{\"price\":20000.0,\"size\":0.001,\"side\":\"buy\",\"type\":\"limit\",\"market\":\"BTC/USD\"}";
  this->service->signRequest(req, body, this->credential);
  EXPECT_EQ(req.base().at("FTX-SIGN").to_string(), "6ea0081ca5fc8f095a748498a284e812fa92923a60d289fe889d838107d30e95");
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_FTX, "XRP-PERP", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_SELL},
      {CCAPI_EM_ORDER_QUANTITY, "31431.0"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.306525"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/orders");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["market"].GetString()), "XRP-PERP");
  EXPECT_EQ(std::string(document["side"].GetString()), "sell");
  EXPECT_EQ(std::string(document["price"].GetString()), "0.306525");
  EXPECT_EQ(std::string(document["size"].GetString()), "31431.0");
  EXPECT_EQ(std::string(document["type"].GetString()), "limit");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestCreateOrder_2) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_FTX, "XRP-PERP", "foo", this->credential_2);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_SELL},
      {CCAPI_EM_ORDER_QUANTITY, "31431.0"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "0.306525"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  verifyApiKeyEtc(req, this->credential_2.at(CCAPI_FTX_API_KEY), this->credential_2.at(CCAPI_FTX_API_SUBACCOUNT), this->timestamp);
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_FTX, "XRP-PERP", "foo", this->credential);
  std::string textMessage =
      R"(
      {
        "success": true,
        "result": {
          "createdAt": "2019-03-05T09:56:55.728933+00:00",
          "filledSize": 0,
          "future": "XRP-PERP",
          "id": 9596912,
          "market": "XRP-PERP",
          "price": 0.306525,
          "remainingSize": 31431,
          "side": "sell",
          "size": 31431,
          "status": "open",
          "type": "limit",
          "reduceOnly": false,
          "ioc": false,
          "postOnly": false,
          "clientId": null
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "9596912");
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/orders/d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/orders/by_client_id/d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": "Order queued for cancelation"
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/orders/d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "d0c5340b-6d6c-49d9-b567-48c4bfca13d2"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/orders/by_client_id/d0c5340b-6d6c-49d9-b567-48c4bfca13d2");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "success": true,
          "result": {
            "createdAt": "2019-03-05T09:56:55.728933+00:00",
            "filledSize": 10,
            "future": "XRP-PERP",
            "id": 9596912,
            "market": "XRP-PERP",
            "price": 0.306525,
            "avgFillPrice": 0.306526,
            "remainingSize": 31421,
            "side": "sell",
            "size": 31431,
            "status": "open",
            "type": "limit",
            "reduceOnly": false,
            "ioc": false,
            "postOnly": false,
            "clientId": null
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "9596912");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "31431");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "10");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0.306526 * 10);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "open");
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/orders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("market"), "BTC/USD");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/api/orders");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

void verifyconvertTextMessageToMessageRestGetOpenOrders(const ExecutionManagementServiceFtxTest* fixture, bool isOneInstrument) {
  std::string symbol = isOneInstrument ? "XRP-PERP" : "";
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_FTX, symbol, "", fixture->credential);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": [
        {
          "createdAt": "2019-03-05T09:56:55.728933+00:00",
          "filledSize": 10,
          "future": "XRP-PERP",
          "id": 9596912,
          "market": "XRP-PERP",
          "price": 0.306525,
          "avgFillPrice": 0.306526,
          "remainingSize": 31421,
          "side": "sell",
          "size": 31431,
          "status": "open",
          "type": "limit",
          "reduceOnly": false,
          "ioc": false,
          "postOnly": false,
          "clientId": null
        }
      ]
    }
  )";
  auto messageList = fixture->service->convertTextMessageToMessageRest(request, textMessage, fixture->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_OPEN_ORDERS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "9596912");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "31431");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.306525");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "10");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0.306526 * 10);
  if (!isOneInstrument) {
    EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XRP-PERP");
  }
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, true);
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetOpenOrdersAllInstruments) {
  verifyconvertTextMessageToMessageRestGetOpenOrders(this, false);
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::delete_);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/orders");
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(req.body().c_str());
  EXPECT_EQ(std::string(document["market"].GetString()), "BTC/USD");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_FTX, "BTC/USD", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": "Orders queued for cancelation"
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/subaccounts");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": [
        {
          "nickname": "sub1",
          "deletable": true,
          "editable": true,
          "competition": true
        }
      ]
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNTS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ACCOUNT_ID), "sub1");
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/wallet/balances");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": [
        {
          "coin": "USDTBEAR",
          "free": 2320.2,
          "total": 2340.2
        }
      ]
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDTBEAR");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "2320.2");
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetAccountBalances_2) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential_2);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ACCOUNT_ID, "sub1"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential_2.at(CCAPI_FTX_API_KEY), this->credential_2.at(CCAPI_FTX_API_SUBACCOUNT), this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/subaccounts/sub1/balances");
  verifySignature(req, this->credential_2.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetAccountBalances_2) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential_2);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": [
        {
          "coin": "USDT",
          "free": 4321.2,
          "total": 4340.2,
          "spotBorrow": 0,
          "availableWithoutBorrow": 2320.2
        }
      ]
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "4321.2");
}

TEST_F(ExecutionManagementServiceFtxTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_FTX_API_KEY), "", this->timestamp);
  EXPECT_EQ(req.target().to_string(), "/api/positions");
  verifySignature(req, this->credential.at(CCAPI_FTX_API_SECRET));
}

TEST_F(ExecutionManagementServiceFtxTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_FTX, "", "foo", this->credential);
  std::string textMessage =
      R"(
    {
      "success": true,
      "result": [
        {
          "cost": -31.7906,
          "entryPrice": 138.22,
          "estimatedLiquidationPrice": 152.1,
          "future": "ETH-PERP",
          "initialMarginRequirement": 0.1,
          "longOrderSize": 1744.55,
          "maintenanceMarginRequirement": 0.04,
          "netSize": -0.23,
          "openSize": 1744.32,
          "realizedPnl": 3.39441714,
          "shortOrderSize": 1732.09,
          "side": "sell",
          "size": 0.23,
          "unrealizedPnl": 0,
          "collateralUsed": 3.17906
        }
      ]
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_POSITIONS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_SYMBOL), "ETH-PERP");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "sell");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "0.23");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_COST), "-31.7906");
}

TEST_F(ExecutionManagementServiceFtxTest, createEventFills) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_FTX, "BTC-PERP", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
      "channel": "fills",
      "data": {
        "fee": 78.05799225,
        "feeRate": 0.0014,
        "future": "BTC-PERP",
        "id": 7828307,
        "liquidity": "taker",
        "market": "BTC-PERP",
        "orderId": 38065410,
        "tradeId": 19129310,
        "price": 3723.75,
        "side": "buy",
        "size": 14.973,
        "time": "2019-05-07T16:40:58.358438+00:00",
        "type": "order"
      },
      "type": "update"
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "19129310");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "3723.75");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "14.973");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "38065410");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-PERP");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY), "78.05799225");
}

TEST_F(ExecutionManagementServiceFtxTest, createEventOrders) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_FTX, "XRP-PERP", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
      "channel": "orders",
      "data": {
        "id": 24852229,
        "clientId": null,
        "market": "XRP-PERP",
        "type": "limit",
        "side": "buy",
        "size": 42353.0,
        "price": 0.2977,
        "reduceOnly": false,
        "ioc": false,
        "postOnly": false,
        "status": "closed",
        "filledSize": 0.0,
        "remainingSize": 0.0,
        "avgFillPrice": 0.2978
      },
      "type": "update"
    }
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "24852229");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "0.2977");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "42353.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "0.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "closed");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "XRP-PERP");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
}
} /* namespace ccapi */
#endif
#endif
