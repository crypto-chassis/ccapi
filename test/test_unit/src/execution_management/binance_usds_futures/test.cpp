#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_usds_futures.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceBinanceUsdsFuturesTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceBinanceUsdsFutures>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                                   std::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_BINANCE_USDS_FUTURES_API_KEY, "vmPUZE6mv9SD5VNHk4HlWFsOr6aKE2zvsw0MuIgwCIPy6utIco14y7Ju91duEh8A"},
        {CCAPI_BINANCE_USDS_FUTURES_API_SECRET, "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j"},
    };
    this->timestamp = 1499827319559;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp);
  }
  std::shared_ptr<ExecutionManagementServiceBinanceUsdsFutures> service{nullptr};
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

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, convertTextMessageToMessageRestGetOrder) {
  Request request(Request::Operation::GET_ORDER, CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "BTCUSDT", "foo", this->credential);
  std::string textMessage =
      R"(
  {
    "avgPrice": "0.00000",
    "clientOrderId": "abc",
    "cumQuote": "0.01",
    "executedQty": "0",
    "orderId": 1917641,
    "origQty": "0.40",
    "origType": "TRAILING_STOP_MARKET",
    "price": "0",
    "reduceOnly": false,
    "side": "BUY",
    "positionSide": "SHORT",
    "status": "NEW",
    "stopPrice": "9300",
    "closePosition": false,
    "symbol": "BTCUSDT",
    "time": 1579276756075,
    "timeInForce": "GTC",
    "type": "TRAILING_STOP_MARKET",
    "activatePrice": "9020",
    "priceRate": "0.3",
    "updateTime": 1579276756075,
    "workingType": "CONTRACT_PRICE",
    "priceProtect": false
  }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  auto message = messageList.at(0);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY), "0.01");
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, createEventExecutionTypeTrade) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "BTCUSDT", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
  "e": "ORDER_TRADE_UPDATE",
  "T": 1625560320441,
  "E": 1625560320443,
  "o": {
    "s": "BTCUSDT",
    "c": "LyfbomrL94skVnLNNCsoNr",
    "S": "SELL",
    "o": "LIMIT",
    "f": "GTC",
    "q": "1",
    "p": "30000",
    "ap": "34704.74000",
    "sp": "0",
    "x": "TRADE",
    "X": "FILLED",
    "i": 2732542295,
    "l": "1",
    "z": "1",
    "L": "34704.74",
    "n": "13.88189600",
    "N": "USDT",
    "T": 1625560320441,
    "t": 185523342,
    "b": "0",
    "a": "0",
    "m": false,
    "R": false,
    "wt": "CONTRACT_PRICE",
    "ot": "LIMIT",
    "ps": "BOTH",
    "cp": false,
    "rp": "0",
    "pP": false,
    "si": 0,
    "ss": 0
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
  EXPECT_EQ(element.getValue(CCAPI_TRADE_ID), "185523342");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "34704.74");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "2732542295");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTCUSDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY), "13.88189600");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_ASSET), "USDT");
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, createEventBalanceUpdate) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "BTCUSDT", CCAPI_EM_BALANCE_UPDATE);
  std::string textMessage = R"(
    {
  "e": "ACCOUNT_UPDATE",
  "E": 1564745798939,
  "T": 1564745798938,
  "a": {
    "m": "ORDER",
    "B": [
      {
        "a": "USDT",
        "wb": "122624.12345678",
        "cw": "100.12345678",
        "bc": "50.12345678"
      },
      {
        "a": "BUSD",
        "wb": "1.00000000",
        "cw": "0.00000000",
        "bc": "-49.12345678"
      }
    ]
  }
}
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(std::shared_ptr<WsConnection>(), subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_TOTAL), "122624.12345678");
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, createEventPositionUpdate) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "BTCUSDT", CCAPI_EM_POSITION_UPDATE);
  std::string textMessage = R"(
    {
  "e": "ACCOUNT_UPDATE",
  "E": 1564745798939,
  "T": 1564745798938,
  "a": {
    "m": "ORDER",
    "P": [
      {
        "s": "BTCUSDT",
        "pa": "0",
        "ep": "0.00000",
        "bep": "0",
        "cr": "200",
        "up": "0",
        "mt": "isolated",
        "iw": "0.00000000",
        "ps": "BOTH"
      },
      {
        "s": "BTCUSDT",
        "pa": "20",
        "ep": "6563.66500",
        "bep": "6563.6",
        "cr": "0",
        "up": "2850.21200",
        "mt": "isolated",
        "iw": "13200.70726908",
        "ps": "LONG"
      },
      {
        "s": "BTCUSDT",
        "pa": "-10",
        "ep": "6563.86000",
        "bep": "6563.6",
        "cr": "-45.04000000",
        "up": "-1423.15600",
        "mt": "isolated",
        "iw": "6570.42511771",
        "ps": "SHORT"
      }
    ]
  }
}
)";
  rj::Document document;
  document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
  auto messageList = this->service->createEvent(std::shared_ptr<WsConnection>(), subscription, textMessage, document, this->now).getMessageList();
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, subscription.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 3);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_INSTRUMENT), "BTCUSDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "BOTH");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_ENTRY_PRICE), "0.00000");
  EXPECT_EQ(element.getValue(CCAPI_EM_UNREALIZED_PNL), "0");
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, createEventExecutionTypeNew) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "BTCUSDT", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
      "e": "ORDER_TRADE_UPDATE",
      "T": 1625559810396,
      "E": 1625559810398,
      "o": {
        "s": "BTCUSDT",
        "c": "swEILRb7r1nZaIg3jeMljV",
        "S": "SELL",
        "o": "LIMIT",
        "f": "GTC",
        "q": "1",
        "p": "30000",
        "ap": "0",
        "sp": "0",
        "x": "NEW",
        "X": "NEW",
        "i": 2732541457,
        "l": "0",
        "z": "0",
        "L": "0",
        "T": 1625559810396,
        "t": 0,
        "b": "0",
        "a": "0",
        "m": false,
        "R": false,
        "wt": "CONTRACT_PRICE",
        "ot": "LIMIT",
        "ps": "BOTH",
        "cp": false,
        "rp": "0",
        "pP": false,
        "si": 0,
        "ss": 0
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "2732541457");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "30000");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "1");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_STATUS), "NEW");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "BTCUSDT");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY)), 0);
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, convertRequestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_USDS_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/fapi/v2/account");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_USDS_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
            "feeTier": 0,
            "canTrade": true,
            "canDeposit": true,
            "canWithdraw": true,
            "updateTime": 0,
            "totalInitialMargin": "0.00000000",
            "totalMaintMargin": "0.00000000",
            "totalWalletBalance": "23.72469206",
            "totalUnrealizedProfit": "0.00000000",
            "totalMarginBalance": "23.72469206",
            "totalPositionInitialMargin": "0.00000000",
            "totalOpenOrderInitialMargin": "0.00000000",
            "totalCrossWalletBalance": "23.72469206",
            "totalCrossUnPnl": "0.00000000",
            "availableBalance": "23.72469206",
            "maxWithdrawAmount": "23.72469206",
            "assets": [
                {
                    "asset": "USDT",
                    "walletBalance": "23.72469206",
                    "unrealizedProfit": "0.00000000",
                    "marginBalance": "23.72469206",
                    "maintMargin": "0.00000000",
                    "initialMargin": "0.00000000",
                    "positionInitialMargin": "0.00000000",
                    "openOrderInitialMargin": "0.00000000",
                    "crossWalletBalance": "23.72469206",
                    "crossUnPnl": "0.00000000",
                    "availableBalance": "23.72469206",
                    "maxWithdrawAmount": "23.72469206",
                    "marginAvailable": true,
                    "updateTime": 1625474304765
                },
                {
                    "asset": "BUSD",
                    "walletBalance": "103.12345678",
                    "unrealizedProfit": "0.00000000",
                    "marginBalance": "103.12345678",
                    "maintMargin": "0.00000000",
                    "initialMargin": "0.00000000",
                    "positionInitialMargin": "0.00000000",
                    "openOrderInitialMargin": "0.00000000",
                    "crossWalletBalance": "103.12345678",
                    "crossUnPnl": "0.00000000",
                    "availableBalance": "103.12345678",
                    "maxWithdrawAmount": "103.12345678",
                    "marginAvailable": true,
                    "updateTime": 1625474304765
                }
            ],
            "positions": [
                {
                    "symbol": "BTCUSDT",
                    "initialMargin": "0",
                    "maintMargin": "0",
                    "unrealizedProfit": "0.00000000",
                    "positionInitialMargin": "0",
                    "openOrderInitialMargin": "0",
                    "leverage": "100",
                    "isolated": true,
                    "entryPrice": "0.00000",
                    "maxNotional": "250000",
                    "positionSide": "BOTH",
                    "positionAmt": "0",
                    "updateTime": 0
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
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "USDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "23.72469206");
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::get);
  verifyApiKey(req, this->credential.at(CCAPI_BINANCE_USDS_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/fapi/v2/positionRisk");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("timestamp"), std::to_string(this->timestamp));
  verifySignature(splitted.at(1), this->credential.at(CCAPI_BINANCE_USDS_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceBinanceUsdsFuturesTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, "", "foo", this->credential);
  std::string textMessage =
      R"(
        [
            {
                "symbol": "BTCUSDT",
                "initialMargin": "0",
                "maintMargin": "0",
                "unrealizedProfit": "0.00000000",
                "positionInitialMargin": "0",
                "openOrderInitialMargin": "0",
                "leverage": "100",
                "isolated": true,
                "entryPrice": "0.00000",
                "maxNotional": "250000",
                "positionSide": "BOTH",
                "positionAmt": "0",
                "updateTime": 0
            }
        ]
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_POSITIONS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 1);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_INSTRUMENT), "BTCUSDT");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "BOTH");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "0");
  EXPECT_DOUBLE_EQ(std::stod(element.getValue(CCAPI_EM_POSITION_ENTRY_PRICE)), 0);
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_LEVERAGE), "100");
}
} /* namespace ccapi */
#endif
#endif
