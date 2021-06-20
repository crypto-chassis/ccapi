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

TEST_F(ExecutionManagementServiceOkexTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_OKEX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target().to_string(), "/api/v5/account/balance");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_OKEX, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
    "code": "0",
    "data": [
        {
            "adjEq": "10679688.0460531643092577",
            "details": [
                {
                    "availBal": "",
                    "availEq": "9930359.9998",
                    "cashBal": "9930359.9998",
                    "ccy": "USDT",
                    "crossLiab": "0",
                    "disEq": "9439737.0772999514",
                    "eq": "9930359.9998",
                    "eqUsd": "9933041.196999946",
                    "frozenBal": "0",
                    "interest": "0",
                    "isoEq": "0",
                    "isoLiab": "0",
                    "liab": "0",
                    "maxLoan": "10000",
                    "mgnRatio": "",
                    "notionalLever": "",
                    "ordFrozen": "0",
                    "twap": "0",
                    "uTime": "1620722938250",
                    "upl": "0",
                    "uplLiab": "0"
                },
                {
                    "availBal": "",
                    "availEq": "33.6799714158199414",
                    "cashBal": "33.2009985",
                    "ccy": "BTC",
                    "crossLiab": "0",
                    "disEq": "1239950.9687532129092577",
                    "eq": "33.771820625136023",
                    "eqUsd": "1239950.9687532129092577",
                    "frozenBal": "0.0918492093160816",
                    "interest": "0",
                    "isoEq": "0",
                    "isoLiab": "0",
                    "liab": "0",
                    "maxLoan": "1453.92289531493594",
                    "mgnRatio": "",
                    "notionalLever": "",
                    "ordFrozen": "0",
                    "twap": "0",
                    "uTime": "1620722938250",
                    "upl": "0.570822125136023",
                    "uplLiab": "0"
                }
            ],
            "imr": "3372.2942371050594217",
            "isoEq": "0",
            "mgnRatio": "70375.35408747017",
            "mmr": "134.8917694842024",
            "notionalUsd": "33722.9423710505978888",
            "ordFroz": "0",
            "totalEq": "11172992.1657531589092577",
            "uTime": "1623392334718"
        }
    ],
    "msg": ""
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "9930359.9998");
}

TEST_F(ExecutionManagementServiceOkexTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_OKEX, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_OKEX_API_KEY), this->credential.at(CCAPI_OKEX_API_PASSPHRASE), this->timestampStr);
  EXPECT_EQ(req.target().to_string(), "/api/v5/account/positions");
  verifySignature(req, this->credential.at(CCAPI_OKEX_API_SECRET));
}

TEST_F(ExecutionManagementServiceOkexTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_OKEX, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
          "code": "0",
          "msg": "",
          "data": [
            {
              "adl":"1",
              "availPos":"1",
              "avgPx":"2566.31",
              "cTime":"1619507758793",
              "ccy":"ETH",
              "deltaBS":"",
              "deltaPA":"",
              "gammaBS":"",
              "gammaPA":"",
              "imr":"",
              "instId":"ETH-USD-210430",
              "instType":"FUTURES",
              "interest":"0",
              "last":"2566.22",
              "lever":"10",
              "liab":"",
              "liabCcy":"",
              "liqPx":"2352.8496681818233",
              "margin":"0.0003896645377994",
              "mgnMode":"isolated",
              "mgnRatio":"11.731726509588816",
              "mmr":"0.0000311811092368",
              "notionalUsd":"2276.2546609009605",
              "optVal":"",
              "pTime":"1619507761462",
              "pos":"1",
              "posCcy":"",
              "posId":"307173036051017730",
              "posSide":"long",
              "thetaBS":"",
              "thetaPA":"",
              "tradeId":"109844",
              "uTime":"1619507761462",
              "upl":"-0.0000009932766034",
              "uplRatio":"-0.0025490556801078",
              "vegaBS":"",
              "vegaPA":""
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
  EXPECT_EQ(element.getValue(CCAPI_EM_SYMBOL), "ETH-USD-210430");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "long");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "1");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_POSITION_COST)), 2566.31);
}
} /* namespace ccapi */
#endif
#endif
