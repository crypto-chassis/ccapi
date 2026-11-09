// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio_perpetual_futures.h"
// clang-format on

namespace ccapi {

// Capture connection setup without opening a network connection.
template <class Base>
class GateioWebsocketProbe : public Base {
 public:
  using Base::Base;

  void prepareConnect(std::shared_ptr<WsConnection> connection) override { this->connections.push_back(connection); }

  std::vector<std::shared_ptr<WsConnection>> connections;
};

class ExecutionManagementServiceGateioWebsocketTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    auto handler = [this](Event& event, Queue<Event>*) { this->events.push_back(event); };
    SessionConfigs configs;
    auto urls = configs.getUrlWebsocketOrderEntryBase();
    if (GetParam()) {
      this->exchange = CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES;
      this->prefix = "futures.";
      this->credential = {{CCAPI_GATEIO_PERPETUAL_FUTURES_API_KEY, "test-key"}, {CCAPI_GATEIO_PERPETUAL_FUTURES_API_SECRET, "test-secret"}};
      urls[this->exchange] = "wss://futures.example.com";
      configs.setUrlWebsocketOrderEntryBase(urls);
      this->service =
          std::make_shared<GateioWebsocketProbe<ExecutionManagementServiceGateioPerpetualFutures>>(handler, SessionOptions(), configs, &this->context);
    } else {
      this->exchange = CCAPI_EXCHANGE_NAME_GATEIO;
      this->prefix = "spot.";
      this->credential = {{CCAPI_GATEIO_API_KEY, "test-key"}, {CCAPI_GATEIO_API_SECRET, "test-secret"}};
      urls[this->exchange] = "wss://spot.example.com";
      configs.setUrlWebsocketOrderEntryBase(urls);
      this->service = std::make_shared<GateioWebsocketProbe<ExecutionManagementServiceGateio>>(handler, SessionOptions(), configs, &this->context);
    }
    this->subscription = Subscription(this->exchange, "", CCAPI_EM_WEBSOCKET_ORDER_ENTRY, "", "connection");
    this->connection = std::make_shared<WsConnection>();
    this->connection->id = "ws-connection";
    this->connection->url = "wss://example.com";
    this->now = UtilTime::makeTimePointFromMilliseconds(1499827319000LL);
  }

  void convert(rj::Document& document, Request::Operation operation, const std::map<std::string, std::string>& param, unsigned long id = 1) {
    Request request(operation, this->exchange, "BTC_USDT", "request-" + std::to_string(id), this->credential);
    request.appendParam(param);
    this->service->convertRequestForWebsocket(document, document.GetAllocator(), this->connection, request, id, this->now, "BTC_USDT", this->credential);
  }

  void receive(const std::string& channel, const std::string& data, const std::string& extra = "", const std::string& status = "200",
               const std::string& requestId = "1") {
    const std::string text = "{\"request_id\":\"" + requestId + "\",\"header\":{\"channel\":\"" + this->prefix + channel +
                             "\",\"event\":\"api\",\"status\":\"" + status + "\"},\"data\":" + data + extra + "}";
    this->service->onTextMessage(this->connection, this->subscription, text, this->now);
  }

  ServiceContext context;
  std::shared_ptr<ExecutionManagementServiceGateioBase> service;
  std::shared_ptr<WsConnection> connection;
  Subscription subscription;
  std::map<std::string, std::string> credential;
  std::string exchange;
  std::string prefix;
  TimePoint now;
  std::vector<Event> events;
};

TEST_P(ExecutionManagementServiceGateioWebsocketTest, loginRequestAndSignature) {
  auto strings = this->service->createSendStringListFromSubscription(this->connection, this->subscription, this->now, this->credential);
  ASSERT_EQ(strings.size(), 1);
  rj::Document document;
  document.Parse(strings.at(0).c_str());
  ASSERT_TRUE(document.IsObject());
  EXPECT_EQ(document["time"].GetInt64(), 1499827319);
  EXPECT_EQ(std::string(document["channel"].GetString()), this->prefix + "login");
  EXPECT_EQ(std::string(document["event"].GetString()), "api");
  const auto& payload = document["payload"];
  EXPECT_EQ(std::string(payload["api_key"].GetString()), "test-key");
  EXPECT_EQ(std::string(payload["timestamp"].GetString()), "1499827319");
  EXPECT_EQ(std::string(payload["req_id"].GetString()), "login");
  EXPECT_EQ(std::string(payload["signature"].GetString()),
            Hmac::hmac(Hmac::ShaVersion::SHA512, "test-secret", "api\n" + this->prefix + "login\n\n1499827319", true));
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, loginSuccessAndFailure) {
  this->receive("login", R"({"result":{"api_key":"test-key","uid":"1"}})", "", "200", "login");
  ASSERT_EQ(this->events.size(), 1);
  EXPECT_EQ(this->events.at(0).getType(), Event::Type::AUTHORIZATION_STATUS);
  auto message = this->events.at(0).getMessageList().at(0);
  EXPECT_EQ(message.getType(), Message::Type::AUTHORIZATION_SUCCESS);
  EXPECT_EQ(message.getTimeReceived(), this->now);
  EXPECT_EQ(message.getCorrelationIdList(), std::vector<std::string>{"connection"});
  EXPECT_EQ(message.getElementList().at(0).getValue(CCAPI_CONNECTION_ID), "ws-connection");
  this->receive("login", R"({"errs":{"label":"INVALID_KEY","message":"Invalid key"}})", "", "401", "login");
  ASSERT_EQ(this->events.size(), 2);
  EXPECT_EQ(this->events.back().getMessageList().at(0).getType(), Message::Type::AUTHORIZATION_FAILURE);
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, orderRequestsIncludeChannelId) {
  for (auto operation : {Request::Operation::CREATE_ORDER, Request::Operation::AMEND_ORDER, Request::Operation::CANCEL_ORDER}) {
    rj::Document document;
    this->convert(document, operation, {});
    const auto& payload = document["payload"];
    ASSERT_TRUE(payload.HasMember("req_header"));
    const auto& header = payload["req_header"];
    ASSERT_TRUE(header.IsObject());
    ASSERT_TRUE(header.HasMember("X-Gate-Channel-Id"));
    EXPECT_EQ(std::string(header["X-Gate-Channel-Id"].GetString()), CCAPI_GATEIO_API_CHANNEL_ID);
  }
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, placeOrder) {
  rj::Document document;
  this->convert(document, Request::Operation::CREATE_ORDER,
                {{CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
                 {CCAPI_EM_ORDER_QUANTITY, "10"},
                 {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"},
                 {CCAPI_EM_CLIENT_ORDER_ID, "t-client"},
                 {"reduce_only", "false"}});
  EXPECT_EQ(std::string(document["channel"].GetString()), this->prefix + "order_place");
  EXPECT_EQ(std::string(document["event"].GetString()), "api");
  EXPECT_EQ(document["time"].GetInt64(), 1499827319);
  EXPECT_EQ(std::string(document["payload"]["req_id"].GetString()), "1");
  const auto& param = document["payload"]["req_param"];
  EXPECT_EQ(std::string(param["price"].GetString()), "25000");
  EXPECT_EQ(std::string(param["text"].GetString()), "t-client");
  EXPECT_FALSE(param["reduce_only"].GetBool());
  if (GetParam()) {
    ASSERT_TRUE(param["size"].IsInt64());
    EXPECT_EQ(param["size"].GetInt64(), 10);
    EXPECT_EQ(std::string(param["contract"].GetString()), "BTC_USDT");
    EXPECT_FALSE(param.HasMember("side"));
  } else {
    EXPECT_EQ(std::string(param["amount"].GetString()), "10");
    EXPECT_EQ(std::string(param["currency_pair"].GetString()), "BTC_USDT");
    EXPECT_EQ(std::string(param["side"].GetString()), "buy");
  }
  EXPECT_EQ(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).at(1), "request-1");
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, amendAndCancelByOrderOrClientId) {
  for (auto operation : {Request::Operation::AMEND_ORDER, Request::Operation::CANCEL_ORDER}) {
    for (const auto& idKey : {CCAPI_EM_ORDER_ID, CCAPI_EM_CLIENT_ORDER_ID, "order_id"}) {
      std::map<std::string, std::string> param{{idKey, "t-client"}};
      if (operation == Request::Operation::AMEND_ORDER) {
        param[CCAPI_EM_ORDER_QUANTITY] = "-5000000000";
        param[CCAPI_EM_ORDER_LIMIT_PRICE] = "25000";
        param["amend_text"] = "amend1";
      }
      rj::Document document;
      this->convert(document, operation, param);
      EXPECT_EQ(std::string(document["channel"].GetString()), this->prefix + (operation == Request::Operation::AMEND_ORDER ? "order_amend" : "order_cancel"));
      const auto& result = document["payload"]["req_param"];
      EXPECT_EQ(std::string(result["order_id"].GetString()), "t-client");
      EXPECT_FALSE(result.HasMember("text"));
      if (GetParam()) {
        EXPECT_FALSE(result.HasMember("contract"));
        if (operation == Request::Operation::AMEND_ORDER) {
          EXPECT_EQ(result["size"].GetInt64(), -5000000000LL);
        }
      } else {
        EXPECT_EQ(std::string(result["currency_pair"].GetString()), "BTC_USDT");
      }
    }
  }
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, nativeParamsAndOrderIdPriority) {
  rj::Document document;
  this->convert(document, Request::Operation::AMEND_ORDER,
                {{CCAPI_EM_ORDER_ID, "123"},
                 {CCAPI_EM_CLIENT_ORDER_ID, "t-client"},
                 {"order_id", "native"},
                 {"price", "25000"},
                 {GetParam() ? "size" : "amount", "2"},
                 {"settle", "usdt"},
                 {CCAPI_SETTLE_ASSET, "usdt"}});
  const auto& param = document["payload"]["req_param"];
  EXPECT_EQ(std::string(param["order_id"].GetString()), "123");
  EXPECT_FALSE(param.HasMember("settle"));
  EXPECT_FALSE(param.HasMember(CCAPI_SETTLE_ASSET));
  EXPECT_EQ(std::string(param["price"].GetString()), "25000");
  if (GetParam()) {
    EXPECT_EQ(param["size"].GetInt64(), 2);
  } else {
    EXPECT_EQ(std::string(param["amount"].GetString()), "2");
  }
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, acknowledgementsAndFinalResponses) {
  for (auto operation : {Request::Operation::CREATE_ORDER, Request::Operation::AMEND_ORDER, Request::Operation::CANCEL_ORDER}) {
    rj::Document document;
    this->convert(document, operation, {{CCAPI_EM_ORDER_ID, "123"}, {CCAPI_EM_ORDER_LIMIT_PRICE, "25000"}});
    const auto channel = operation == Request::Operation::CREATE_ORDER  ? "order_place"
                         : operation == Request::Operation::AMEND_ORDER ? "order_amend"
                                                                        : "order_cancel";
    this->events.clear();
    this->receive(channel, R"({"result":{"req_id":"1"}})", ",\"ack\":true");
    EXPECT_TRUE(this->events.empty());
    EXPECT_EQ(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).count(1), 1);
    this->receive(channel, GetParam() ? R"({"result":{"id":123,"text":"t-client","size":-10,"price":"25000","contract":"BTC_USDT"}})"
                                      : R"({"result":{"id":"123","text":"t-client","amount":"10","price":"25000","currency_pair":"BTC_USDT"}})");
    ASSERT_EQ(this->events.size(), 1);
    EXPECT_EQ(this->events.at(0).getType(), Event::Type::RESPONSE);
    auto message = this->events.at(0).getMessageList().at(0);
    EXPECT_EQ(message.getType(), this->service->requestOperationToMessageTypeMap.at(operation));
    EXPECT_EQ(message.getCorrelationIdList(), std::vector<std::string>{"request-1"});
    ASSERT_EQ(message.getElementList().size(), 1);
    EXPECT_EQ(message.getElementList().at(0).getValue(CCAPI_EM_ORDER_ID), "123");
    EXPECT_EQ(message.getElementList().at(0).getValue(CCAPI_EM_ORDER_QUANTITY), GetParam() ? "-10" : "10");
    EXPECT_TRUE(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).empty());
  }
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, errorsAndUnknownResponses) {
  rj::Document document;
  this->convert(document, Request::Operation::CANCEL_ORDER, {{CCAPI_EM_ORDER_ID, "123"}});
  for (const auto& id : {"not-a-number", "99999999999999999999999999999999", "4294967297", "2"}) {
    this->receive("order_cancel", R"({"result":{}})", "", "200", id);
  }
  EXPECT_TRUE(this->events.empty());
  EXPECT_EQ(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).count(1), 1);
  this->receive("order_cancel", R"({"errs":{"label":"ORDER_NOT_FOUND","message":"No order"}})", "", "400");
  ASSERT_EQ(this->events.size(), 1);
  auto message = this->events.at(0).getMessageList().at(0);
  EXPECT_EQ(message.getType(), Message::Type::RESPONSE_ERROR);
  EXPECT_EQ(message.getCorrelationIdList(), std::vector<std::string>{"request-1"});
  EXPECT_FALSE(message.getElementList().at(0).getValue(CCAPI_ERROR_MESSAGE).empty());
  EXPECT_TRUE(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).empty());
  this->receive("order_cancel", R"({"result":{}})");
  EXPECT_EQ(this->events.size(), 1);
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, outOfOrderResponsesAndApiErrors) {
  rj::Document first;
  rj::Document second;
  this->convert(first, Request::Operation::AMEND_ORDER, {{CCAPI_EM_ORDER_ID, "123"}, {"price", "25000"}}, 1);
  this->convert(second, Request::Operation::CANCEL_ORDER, {{CCAPI_EM_ORDER_ID, "456"}}, 2);
  this->receive("order_cancel", R"({"result":{"id":"456"}})", "", "200", "2");
  ASSERT_EQ(this->events.size(), 1);
  EXPECT_EQ(this->events.at(0).getMessageList().at(0).getCorrelationIdList(), std::vector<std::string>{"request-2"});
  EXPECT_EQ(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).count(1), 1);
  this->receive("order_amend", R"({"errs":{"label":"INVALID_ARGUMENT","message":"Invalid price"}})");
  ASSERT_EQ(this->events.size(), 2);
  EXPECT_EQ(this->events.back().getMessageList().at(0).getType(), Message::Type::RESPONSE_ERROR);
  EXPECT_EQ(this->events.back().getMessageList().at(0).getCorrelationIdList(), std::vector<std::string>{"request-1"});
  EXPECT_TRUE(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(this->connection->id).empty());
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, existingPrivateSubscriptionAuthentication) {
  Subscription subscription(this->exchange, "BTC_USDT", CCAPI_EM_ORDER_UPDATE, "", "private");
  auto strings = this->service->createSendStringListFromSubscription(this->connection, subscription, this->now, this->credential);
  ASSERT_EQ(strings.size(), 1);
  rj::Document document;
  document.Parse(strings.at(0).c_str());
  EXPECT_EQ(std::string(document["event"].GetString()), "subscribe");
  EXPECT_EQ(std::string(document["channel"].GetString()), this->prefix + "orders");
  EXPECT_EQ(std::string(document["payload"][0].GetString()), "BTC_USDT");
  EXPECT_EQ(std::string(document["auth"]["KEY"].GetString()), "test-key");
  EXPECT_EQ(std::string(document["auth"]["SIGN"].GetString()),
            Hmac::hmac(Hmac::ShaVersion::SHA512, "test-secret", "channel=" + this->prefix + "orders&event=subscribe&time=1499827319", true));
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, nativePlaceOrderAndSignedFuturesSize) {
  rj::Document document;
  this->convert(document, Request::Operation::CREATE_ORDER,
                {{GetParam() ? "size" : "amount", GetParam() ? "-10" : "10"},
                 {GetParam() ? "contract" : "currency_pair", "ETH_USDT"},
                 {CCAPI_EM_ACCOUNT_TYPE, "unified"},
                 {"price", "2500"},
                 {"side", "sell"}});
  const auto& param = document["payload"]["req_param"];
  EXPECT_EQ(std::string(param["account"].GetString()), "unified");
  EXPECT_EQ(std::string(param[GetParam() ? "contract" : "currency_pair"].GetString()), "ETH_USDT");
  EXPECT_EQ(param.MemberCount(), GetParam() ? 4 : 5);
  if (GetParam()) {
    EXPECT_EQ(param["size"].GetInt64(), -10);
  } else {
    EXPECT_EQ(std::string(param["amount"].GetString()), "10");
    EXPECT_EQ(std::string(param["side"].GetString()), "sell");
  }
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, unsupportedOperationAndNonApiMessages) {
  rj::Document document;
  EXPECT_THROW(this->convert(document, Request::Operation::GET_ORDER, {}), std::runtime_error);
  EXPECT_TRUE(this->service->requestCorrelationIdByWsRequestIdByConnectionIdMap.empty());
  this->service->onTextMessage(this->connection, this->subscription, "{\"channel\":\"spot.pong\"}", this->now);
  EXPECT_TRUE(this->events.empty());
}

TEST_P(ExecutionManagementServiceGateioWebsocketTest, subscriptionUsesOrderEntryUrl) {
  for (const auto& instrument : {"", "BTC_USD", "BTC_USDT"}) {
    std::vector<Subscription> subscriptions{Subscription(this->exchange, instrument, CCAPI_EM_WEBSOCKET_ORDER_ENTRY, "", "connection")};
    this->service->subscribe(subscriptions);
  }
  this->context.ioContextPtr->poll();
  std::vector<std::shared_ptr<WsConnection>> connections;
  if (GetParam()) {
    connections = std::dynamic_pointer_cast<GateioWebsocketProbe<ExecutionManagementServiceGateioPerpetualFutures>>(this->service)->connections;
  } else {
    connections = std::dynamic_pointer_cast<GateioWebsocketProbe<ExecutionManagementServiceGateio>>(this->service)->connections;
  }
  ASSERT_EQ(connections.size(), 3);
  EXPECT_EQ(connections.at(0)->url, GetParam() ? "wss://futures.example.com/v4/ws/usdt" : "wss://spot.example.com/ws/v4/");
  EXPECT_EQ(connections.at(1)->url, GetParam() ? "wss://futures.example.com/v4/ws/btc" : "wss://spot.example.com/ws/v4/");
  EXPECT_EQ(connections.at(2)->url, GetParam() ? "wss://futures.example.com/v4/ws/usdt" : "wss://spot.example.com/ws/v4/");
}

INSTANTIATE_TEST_SUITE_P(SpotAndFutures, ExecutionManagementServiceGateioWebsocketTest, ::testing::Bool());

}  // namespace ccapi
