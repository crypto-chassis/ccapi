#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#include "gtest/gtest.h"

#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_coin_swap.h"
namespace ccapi {
class ExecutionManagementServiceHuobiCoinSwapTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceHuobiCoinSwap>([](Event& event) {}, SessionOptions(), SessionConfigs(),
                                                                              wspp::lib::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_HUOBI_COIN_SWAP_API_KEY, "b33ff154-e02e01af-mjlpdje3ld-87508"},
        {CCAPI_HUOBI_COIN_SWAP_API_SECRET, "968df5e1-790fa852-ce124901-9ccc5"},
    };
    this->timestamp = "2017-05-11T15:19:30";
    this->now = UtilTime::parse(this->timestamp + "Z");
  }
  std::shared_ptr<ExecutionManagementServiceHuobiCoinSwap> service{nullptr};
  std::map<std::string, std::string> credential;
  std::string timestamp;
  TimePoint now{};
};

TEST_F(ExecutionManagementServiceHuobiCoinSwapTest, convertTextMessageToMessageRestGetAccountBalances) {
  Request request(Request::Operation::GET_ACCOUNT_BALANCES, CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
    "status": "ok",
    "data": [
        {
            "symbol": "THETA",
            "margin_balance": 717.600960962561438668000000000000000000000000000000000000,
            "margin_position": 15.483471394286599055,
            "margin_frozen": 13.765413852951653365,
            "margin_available": 688.352075715323186248,
            "profit_real": 0,
            "profit_unreal": -6.321988896485647800000000000000000000000000000000000000,
            "risk_rate": 24.134301218550508200,
            "withdraw_available": 688.352075715323186248000000000000000000000000000000000000,
            "liquidation_price": 0.198584522842823398,
            "lever_rate": 20,
            "adjust_factor": 0.400000000000000000,
            "margin_static": 723.922949859047086468,
            "contract_code": "THETA-USD"
        }
    ],
    "ts": 1603866747447
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "THETA");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "688.352075715323186248");
}

TEST_F(ExecutionManagementServiceHuobiCoinSwapTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
    "status": "ok",
    "data": [
        {
            "symbol": "THETA",
            "contract_code": "THETA-USD",
            "volume": 20.000000000000000000,
            "available": 20.000000000000000000,
            "frozen": 0,
            "cost_open": 0.604834710743801652,
            "cost_hold": 0.659310000000000000,
            "profit_unreal": -10.525756239881099200000000000000000000000000000000000000,
            "profit_rate": 1.015859675335792480,
            "lever_rate": 20,
            "position_margin": 15.693659761456371625,
            "direction": "buy",
            "profit": 16.795657677889033200000000000000000000000000000000000000,
            "last_price": 0.6372
        }
    ],
    "ts": 1603868312729
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
  EXPECT_EQ(element.getValue(CCAPI_EM_SYMBOL), "THETA-USD");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "buy");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "20.000000000000000000");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_COST), "0.604834710743801652");
}
} /* namespace ccapi */
#endif
#endif
