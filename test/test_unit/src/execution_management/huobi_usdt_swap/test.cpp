#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_usdt_swap.h"
namespace ccapi {
class ExecutionManagementServiceHuobiUsdtSwapTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceHuobiUsdtSwap>([](Event& event) {}, SessionOptions(), SessionConfigs(),
                                                                              wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_HUOBI_USDT_SWAP_API_KEY, "b33ff154-e02e01af-mjlpdje3ld-87508"},
        {CCAPI_HUOBI_USDT_SWAP_API_SECRET, "968df5e1-790fa852-ce124901-9ccc5"},
    };
    this->timestamp = "2017-05-11T15:19:30";
    this->now = UtilTime::parse(this->timestamp + "Z");
  }
  std::shared_ptr<ExecutionManagementServiceHuobiUsdtSwap> service{nullptr};
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
  preSignedText += "api.hbdm.com";
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

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, signRequest) {
  http::request<http::string_body> req;
  req.method(http::verb::post);
  req.set(http::field::host, "api.hbdm.com");
  std::string path = "/linear-swap-api/v1/swap_cross_order";
  std::map<std::string, std::string> queryParamMap = {
      {"AccessKeyId", this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY)},
      {"SignatureMethod", "HmacSHA256"},
      {"SignatureVersion", "2"},
      {"Timestamp", "2021-03-30T04%3A01%3A16"},
  };
  this->service->signRequest(req, path, queryParamMap, this->credential);
  EXPECT_EQ(Url::urlDecode(Url::convertQueryStringToMap(UtilString::split(req.target().to_string(), "?").at(1)).at("Signature")),
            "XgdueCZTpDpC1oKDvYwbWRIF39v9jjmK+hajlttv7M4=");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {"price", "29999"}, {"volume", "1"}, {"direction", "buy"}, {"offset", "open"}, {"lever_rate", "5"}, {"order_price_type", "opponent"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["price"].GetString()), "29999");
  EXPECT_EQ(std::string(document["volume"].GetString()), "1");
  EXPECT_EQ(std::string(document["direction"].GetString()), "buy");
  EXPECT_EQ(std::string(document["offset"].GetString()), "open");
  EXPECT_EQ(std::string(document["lever_rate"].GetString()), "5");
  EXPECT_EQ(std::string(document["order_price_type"].GetString()), "opponent");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_order");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "status": "ok",
    "data": {
        "order_id": 784017187857760256,
        "order_id_str": "784017187857760256"
    },
    "ts": 1606965863952
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "784017187857760256");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "59378"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["order_id"].GetString()), "59378");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_cancel");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestCancelOrderByClientOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "a0001"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["client_order_id"].GetString()), "a0001");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_cancel");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::string textMessage =
      R"(
  {
      "status": "ok",
      "data": {
          "errors": [
              {
                  "order_id": "784054331179532288",
                  "err_code": 1062,
                  "err_msg": "Cancelling. Please be patient."
              }
          ],
          "successes": "784054331179532288"
      },
      "ts": 1606974744952
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "784054331179532288");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestGetOrderByOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "59378"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["order_id"].GetString()), "59378");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_order_info");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestGetOrderByClientOrderId) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_CLIENT_ORDER_ID, "a0001"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["client_order_id"].GetString()), "a0001");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_order_info");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "ETH-USDT", "foo", this->credential);
  std::string textMessage =
      R"(
  {
      "status": "ok",
      "data": [
          {
              "symbol": "ETH",
              "contract_code": "ETH-USDT",
              "volume": 1,
              "price": 17,
              "order_price_type": "optimal_10_ioc",
              "order_type": 1,
              "direction": "sell",
              "offset": "open",
              "lever_rate": 5,
              "order_id": 784056956650258432,
              "client_order_id": null,
              "created_at": 1606975345528,
              "trade_volume": 1,
              "trade_turnover": 0.5,
              "fee": -0.0002,
              "trade_avg_price": 50,
              "margin_frozen": 0,
              "profit": 0,
              "status": 6,
              "order_source": "api",
              "order_id_str": "784056956650258432",
              "fee_asset": "USDT",
              "liquidation_type": "0",
              "canceled_at": 0,
              "margin_asset": "USDT",
              "margin_account": "USDT",
              "margin_mode": "cross",
              "is_tpsl": 0,
              "real_profit": 0
          }
      ],
      "ts": 1606975356655
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "784056956650258432");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "17");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "1");
  CCAPI_LOGGER_DEBUG(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY));
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 50);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "6");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "btc-usdt", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  rj::Document document;
  document.Parse(req.body().c_str());
  EXPECT_EQ(std::string(document["contract_code"].GetString()), "btc-usdt");
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_openorders");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertTextMessageToMessageRestGetOpenOrdersOneInstrument) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "ETH-USDT", "", this->credential);
  std::string textMessage =
      R"(
  {
      "status": "ok",
      "data": {
          "orders": [
              {
                  "symbol": "ETH",
                  "contract_code": "ETH-USDT",
                  "volume": 1,
                  "price": 80,
                  "order_price_type": "limit",
                  "order_type": 1,
                  "direction": "sell",
                  "offset": "open",
                  "lever_rate": 30,
                  "order_id": 784059619752280064,
                  "client_order_id": null,
                  "created_at": 1606975980467,
                  "trade_volume": 0,
                  "trade_turnover": 0,
                  "fee": 0,
                  "trade_avg_price": null,
                  "margin_frozen": 0.026666666666666666,
                  "profit": 0,
                  "status": 3,
                  "order_source": "api",
                  "order_id_str": "784059619752280064",
                  "fee_asset": "USDT",
                  "liquidation_type": null,
                  "canceled_at": null,
                  "margin_asset": "USDT",
                  "margin_account": "USDT",
                  "margin_mode": "cross",
                  "is_tpsl": 0,
                  "update_time": 1606975980467,
                  "real_profit": 0
              }
          ],
          "total_page": 1,
          "current_page": 1,
          "total_size": 2
      },
      "ts": 1606975988388
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "784059619752280064");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "80");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_account_info");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
    "status":"ok",
    "data":[
        {
            "margin_mode":"cross",
            "margin_account":"USDT",
            "margin_asset":"USDT",
            "margin_balance":0.000000549410817836,
            "margin_static":0.000000549410817836,
            "margin_position":0,
            "margin_frozen":0,
            "profit_real":0,
            "profit_unreal":0,
            "withdraw_available":0.000000549410817836,
            "risk_rate":null,
            "contract_detail":[
                {
                    "symbol":"BTC",
                    "contract_code":"BTC-USDT",
                    "margin_position":0,
                    "margin_frozen":0,
                    "margin_available":0.000000549410817836,
                    "profit_unreal":0,
                    "liquidation_price":null,
                    "lever_rate":100,
                    "adjust_factor":0.55
                },
                {
                    "symbol":"EOS",
                    "contract_code":"EOS-USDT",
                    "margin_position":0,
                    "margin_frozen":0,
                    "margin_available":0.000000549410817836,
                    "profit_unreal":0,
                    "liquidation_price":null,
                    "lever_rate":5,
                    "adjust_factor":0.06
                }
            ]
        }
    ],
    "ts":1606906200680
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
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "0.000000549410817836");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/linear-swap-api/v1/swap_cross_position_info");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  verifyApiKeyEtc(paramMap, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_KEY), this->timestamp);
  verifySignature(req, this->credential.at(CCAPI_HUOBI_USDT_SWAP_API_SECRET));
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
    "status": "ok",
    "data": [
        {
            "symbol": "BTC",
            "contract_code": "BTC-USDT",
            "volume": 2,
            "available": 2,
            "frozen": 0,
            "cost_open": 51179.1,
            "cost_hold": 51179.1,
            "profit_unreal": 0,
            "profit_rate": 0,
            "lever_rate": 100,
            "position_margin": 10.23582,
            "direction": "sell",
            "profit": 0,
            "last_price": 51179.1,
            "margin_asset": "USDT",
            "margin_mode": "cross",
            "margin_account": "USDT"
        }
    ],
    "ts": 1606962314205
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
  EXPECT_EQ(element.getValue(CCAPI_EM_SYMBOL), "BTC-USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "sell");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "2");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_COST), "51179.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_LEVERAGE), "100");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, createEventMatchOrderData) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "BTC-USDT", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
        "op":"notify",
        "topic":"matchOrders_cross.btc-usdt",
        "ts":1606981093177,
        "uid":"123456789",
        "symbol":"BTC",
        "contract_code":"BTC-USDT",
        "status":6,
        "order_id":784081061787873280,
        "order_id_str":"784081061787873280",
        "client_order_id":null,
        "order_type":1,
        "volume":1,
        "trade_volume":1,
        "created_at":1606981092647,
        "direction":"sell",
        "offset":"open",
        "lever_rate":100,
        "price":51179.1,
        "order_source":"web",
        "order_price_type":"opponent",
        "trade":[
            {
                "trade_id":33380,
                "id":"33380-784081061787873280-1",
                "trade_volume":1,
                "trade_price":51179.1,
                "trade_turnover":511.791,
                "created_at":1606981093104,
                "role":"taker",
                "is_tpsl": 0
            }
        ],
        "margin_mode":"cross",
        "margin_account":"USDT"
    }
)";
  rj::Document document;
  document.Parse(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "notify", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "33380");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "51179.1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "open");
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "784081061787873280");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USDT");
}

TEST_F(ExecutionManagementServiceHuobiUsdtSwapTest, createEventOrderData) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, "BTC-USDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
        "op":"notify",
        "topic":"orders_cross.btc-usdt",
        "ts":1606878438414,
        "symbol":"BTC",
        "contract_code":"BTC-USDT",
        "volume":8,
        "price":50000,
        "order_price_type":"limit",
        "direction":"buy",
        "offset":"close",
        "status":6,
        "lever_rate":100,
        "order_id":783650498317316098,
        "order_id_str":"783650498317316098",
        "client_order_id":null,
        "order_source":"risk",
        "order_type":3,
        "created_at":1606878438320,
        "trade_volume":8,
        "trade_turnover":4000,
        "fee":0,
        "trade_avg_price":50000,
        "margin_frozen":0,
        "profit":-1866.704,
        "trade":[
            {
                "trade_fee":0,
                "fee_asset":"USDT",
                "trade_id":783650498317316098,
                "id":"783650498317316098-783650498317316098-1",
                "trade_volume":8,
                "trade_price":50000,
                "trade_turnover":4000,
                "created_at":1606878438320,
                "profit":-1866.704,
                "real_profit":0,
                "role":"taker"
            }
        ],
        "canceled_at":0,
        "fee_asset":"USDT",
        "margin_asset":"USDT",
        "uid":"123456789",
        "liquidation_type":"1",
        "margin_mode":"cross",
        "margin_account":"USDT",
        "is_tpsl": 0,
        "real_profit":0
    }
)";
  rj::Document document;
  document.Parse(textMessage.c_str());
  auto messageList = this->service->createEvent(subscription, textMessage, document, "notify", this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "783650498317316098");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "50000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "8");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "8");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "6");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTC-USDT");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 400000);
}
} /* namespace ccapi */
#endif
#endif
