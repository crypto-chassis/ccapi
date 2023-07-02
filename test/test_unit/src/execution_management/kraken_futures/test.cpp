#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
// clang-format off
#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_test_execution_management_helper.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_kraken_futures.h"
// clang-format on
namespace ccapi {
class ExecutionManagementServiceKrakenFuturesTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service = std::make_shared<ExecutionManagementServiceKrakenFutures>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(),
                                                                              std::make_shared<ServiceContext>());
    this->credential = {
        {CCAPI_KRAKEN_FUTURES_API_KEY, "Bx0+DtmSNWLNXQVaJ6dcvaWwEKitPU4MadMhEltVe9aSY/nfhkab46IJ"},
        {CCAPI_KRAKEN_FUTURES_API_SECRET, "oMpjprZZIUvn8+0B55zgO3ncp448AI+a7KFwo5lHSmf650mawBQ7ez5pNvDkhRlsHDZuPM+gvPs6dDB8k+Skr9gw"},
    };
    this->timestamp = 1499827319000;
    this->now = UtilTime::makeTimePointFromMilliseconds(this->timestamp);
  }
  std::shared_ptr<ExecutionManagementServiceKrakenFutures> service{nullptr};
  std::map<std::string, std::string> credential;
  long long timestamp{};
  TimePoint now{};
};

void verifyApiKeyEtc(const http::request<http::string_body>& req, const std::string& apiKey) { EXPECT_EQ(req.base().at("APIKey").to_string(), apiKey); }

void verifySignature(const http::request<http::string_body>& req, const std::string& apiSecret) {
  auto splitted = UtilString::split(req.target().to_string(), "?");
  std::string postData = splitted.size() > 1 ? splitted.at(1) : "";
  std::string nonce = req.base().at("Nonce").to_string();
  std::string path = splitted.at(0).rfind("/derivatives", 0) == 0 ? splitted.at(0).substr(std::string("/derivatives").length()) : splitted.at(0);
  std::string preSignedText = postData + nonce + path;
  std::string preSignedTextSha256 = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256, preSignedText);
  auto signature = req.base().at("Authent").to_string();
  EXPECT_EQ(UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedTextSha256)), signature);
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, signRequest) {
  http::request<http::string_body> req;
  req.method(http::verb::post);
  std::string nonce("1632851091927335");
  this->service->signRequest(req, "/api/v3/sendorder", "limitPrice=20000&size=1&side=buy&orderType=lmt&symbol=pi_xbtusd", this->credential, nonce);
  EXPECT_EQ(req.base().at("Authent").to_string(), "43taMU9dlnlbsjpm86rJKM3/XmfJu5OqVHMrP36UF2weUutjStQP7ZU86WKEexigUiG4kJP59vhVKbx+y+Kvsg==");
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY},
      {CCAPI_EM_ORDER_QUANTITY, "1"},
      {CCAPI_EM_ORDER_LIMIT_PRICE, "20000"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/derivatives/api/v3/sendorder");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("orderType"), "lmt");
  EXPECT_EQ(paramMap.at("symbol"), "pi_xbtusd");
  EXPECT_EQ(paramMap.at("side"), "buy");
  EXPECT_EQ(paramMap.at("size"), "1");
  EXPECT_EQ(paramMap.at("limitPrice"), "20000");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertTextMessageToMessageRestCreateOrder) {
  Request request(Request::Operation::CREATE_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "foo", this->credential);
  std::string textMessage =
      R"(
        {
           "result":"success",
           "sendStatus":{
              "order_id":"179f9af8-e45e-469d-b3e9-2fd4675cb7d0",
              "status":"placed",
              "receivedTime":"2019-09-05T16:33:50.734Z",
              "orderEvents":[
                 {
                    "order":{
                       "orderId":"179f9af8-e45e-469d-b3e9-2fd4675cb7d0",
                       "cliOrdId":null,
                       "type":"lmt",
                       "symbol":"pi_xbtusd",
                       "side":"buy",
                       "quantity":10000,
                       "filled":0,
                       "limitPrice":9400,
                       "reduceOnly":false,
                       "timestamp":"2019-09-05T16:33:50.734Z",
                       "lastUpdateTimestamp":"2019-09-05T16:33:50.734Z"
                    },
                    "reducedQuantity":null,
                    "type":"PLACE"
                 }
              ]
           },
           "serverTime":"2019-09-05T16:33:50.734Z"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "179f9af8-e45e-469d-b3e9-2fd4675cb7d0");
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestCancelOrderByOrderId) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "foo", this->credential);
  std::map<std::string, std::string> param{
      {CCAPI_EM_ORDER_ID, "cb4e34f6-4eb3-4d4b-9724-4c3035b99d47"},
  };
  request.appendParam(param);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/derivatives/api/v3/cancelorder");
  auto paramMap = Url::convertQueryStringToMap(splitted.at(1));
  EXPECT_EQ(paramMap.at("order_id"), "cb4e34f6-4eb3-4d4b-9724-4c3035b99d47");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertTextMessageToMessageRestCancelOrder) {
  Request request(Request::Operation::CANCEL_ORDER, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "foo", this->credential);
  std::string textMessage = R"(
    {
        "result":"success",
        "cancelStatus":{
            "status":"cancelled",
            "order_id":"cb4e34f6-4eb3-4d4b-9724-4c3035b99d47",
            "receivedTime":"2020-07-22T13:26:20.806Z",
               "orderEvents":[
              {
                "uid":"cb4e34f6-4eb3-4d4b-9724-4c3035b99d47",
                "order":{
                      "orderId":"cb4e34f6-4eb3-4d4b-9724-4c3035b99d47",
                      "cliOrdId":"1234568",
                      "type":"lmt",
                      "symbol":"pi_xbtusd",
                      "side":"buy",
                      "quantity":5500,
                      "filled":0,
                      "limitPrice":8000,
                      "reduceOnly":false,
                      "timestamp":"2020-07-22T13:25:56.366Z",
                      "lastUpdateTimestamp":"2020-07-22T13:25:56.366Z"
                  },
                  "type":"CANCEL"
               }
            ]
        },
       "serverTime":"2020-07-22T13:26:20.806Z"
    }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_ORDER);
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestGetOpenOrdersAllInstruments) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/derivatives/api/v3/openorders");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, verifyconvertTextMessageToMessageRestGetOpenOrders) {
  Request request(Request::Operation::GET_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "", this->credential);
  std::string textMessage =
      R"(
{
   "result":"success",
   "openOrders":[
      {
         "order_id":"59302619-41d2-4f0b-941f-7e7914760ad3",
         "symbol":"pi_xbtusd",
         "side":"sell",
         "orderType":"lmt",
         "limitPrice":10640,
         "unfilledSize":304,
         "receivedTime":"2019-09-05T17:01:17.410Z",
         "status":"untouched",
         "filledSize":0,
         "reduceOnly":true,
         "lastUpdateTime":"2019-09-05T17:01:17.410Z"
      }
   ],
   "serverTime":"2019-09-05T17:08:18.138Z"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "59302619-41d2-4f0b-941f-7e7914760ad3");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "10640");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_REMAINING_QUANTITY), "304");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "pi_xbtusd");
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/derivatives/api/v3/cancelallorders");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestCancelOpenOrdersGivenSymbol) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  auto splitted = UtilString::split(req.target().to_string(), "?");
  EXPECT_EQ(splitted.at(0), "/derivatives/api/v3/cancelallorders");
  EXPECT_EQ(splitted.at(1), "symbol=pi_xbtusd");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, verifyconvertTextMessageToMessageRestCancelOpenOrders) {
  Request request(Request::Operation::CANCEL_OPEN_ORDERS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "pi_xbtusd", "", this->credential);
  std::string textMessage =
      R"(
        {
           "result":"success",
           "cancelStatus":{
              "receivedTime":"2019-08-01T15:57:37.518Z",
              "cancelOnly":"all",
              "status":"cancelled",
              "cancelledOrders":[
                 {
                    "order_id":"6180adfa-e4b1-4a52-adac-ea5417620dbd"
                 },
                 {
                    "order_id":"89e3edbe-d739-4c52-b866-6f5a8407ff6e"
                 },
                 {
                    "order_id":"0cd37a77-1644-4960-a7fb-9a1f6e0e46f7"
                 }
              ],
              "orderEvents":[
                 {
                    "uid":"6180adfa-e4b1-4a52-adac-ea5417620dbd",
                    "order":{
                       "orderId":"6180adfa-e4b1-4a52-adac-ea5417620dbd",
                       "cliOrdId":null,
                       "type":"stp",
                       "symbol":"pi_xbtusd",
                       "side":"buy",
                       "quantity":1000,
                       "filled":0,
                       "limitPrice":10039,
                       "stopPrice":10039,
                       "reduceOnly":false,
                       "timestamp":"2019-08-01T15:57:19.482Z"
                    },
                    "type":"CANCEL"
                 },
                 {
                    "uid":"89e3edbe-d739-4c52-b866-6f5a8407ff6e",
                    "order":{
                       "orderId":"89e3edbe-d739-4c52-b866-6f5a8407ff6e",
                       "cliOrdId":null,
                       "type":"post",
                       "symbol":"pi_xbtusd",
                       "side":"buy",
                       "quantity":890,
                       "filled":0,
                       "limitPrice":10040,
                       "stopPrice":null,
                       "reduceOnly":false,
                       "timestamp":"2019-08-01T15:57:08.508Z"
                    },
                    "type":"CANCEL"
                 },
                 {
                    "uid":"0cd37a77-1644-4960-a7fb-9a1f6e0e46f7",
                    "order":{
                       "orderId":"0cd37a77-1644-4960-a7fb-9a1f6e0e46f7",
                       "cliOrdId":null,
                       "type":"lmt",
                       "symbol":"pi_xbtusd",
                       "side":"sell",
                       "quantity":900,
                       "filled":0,
                       "limitPrice":10145,
                       "stopPrice":null,
                       "reduceOnly":true,
                       "timestamp":"2019-08-01T15:57:14.003Z"
                    },
                    "type":"CANCEL"
                 }
              ]
           },
           "serverTime":"2019-08-01T15:57:37.520Z"
        }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::CANCEL_OPEN_ORDERS);
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  EXPECT_EQ(req.target().to_string(), "/derivatives/api/v3/accounts");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertTextMessageToMessageRestGetAccounts) {
  Request request(Request::Operation::GET_ACCOUNTS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
            "result":"success",
            "serverTime":"2016-02-25T09:45:53.818Z",
            "accounts":{
                "cash":{
                    "type":"cashAccount",
                    "balances":{
                        "xbt":141.31756797,
                        "xrp":52465.1254
                    }
                }
            }
        }
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNTS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_EM_ASSET), "xbt");
  EXPECT_EQ(element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING), "141.31756797");
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertRequestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "", "foo", this->credential);
  auto req = this->service->convertRequest(request, this->now);
  EXPECT_EQ(req.method(), http::verb::post);
  verifyApiKeyEtc(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_KEY));
  EXPECT_EQ(req.target().to_string(), "/derivatives/api/v3/openpositions");
  verifySignature(req, this->credential.at(CCAPI_KRAKEN_FUTURES_API_SECRET));
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, convertTextMessageToMessageRestGetAccountPositions) {
  Request request(Request::Operation::GET_ACCOUNT_POSITIONS, CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "", "foo", this->credential);
  std::string textMessage =
      R"(
        {
     "result":"success",
     "openPositions":[
        {
         "side":"short",
         "symbol":"pi_xbtusd",
         "price":9392.749993345933,
         "fillTime":"2020-07-22T14:39:12.376Z",
         "size":10000,
         "unrealizedFunding":1.045432180096817E-5
         },
         {
          "side":"long",
          "symbol":"fi_xbtusd_201225",
          "price":9399.749966754434,
          "fillTime":"2020-07-22T14:39:12.376Z",
          "size":20000
          }
     ],
     "serverTime":"2020-07-22T14:39:12.376Z"
}
  )";
  auto messageList = this->service->convertTextMessageToMessageRest(request, textMessage, this->now);
  EXPECT_EQ(messageList.size(), 1);
  verifyCorrelationId(messageList, request.getCorrelationId());
  auto message = messageList.at(0);
  EXPECT_EQ(message.getType(), Message::Type::GET_ACCOUNT_POSITIONS);
  auto elementList = message.getElementList();
  EXPECT_EQ(elementList.size(), 2);
  Element element = elementList.at(0);
  EXPECT_EQ(element.getValue(CCAPI_INSTRUMENT), "pi_xbtusd");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_SIDE), "short");
  EXPECT_EQ(element.getValue(CCAPI_EM_POSITION_QUANTITY), "10000");
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, createEventFills) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "PI_XBTUSD", CCAPI_EM_PRIVATE_TRADE);
  std::string textMessage = R"(
    {
    "feed":"fills",
    "username":"DemoUser",
    "fills":[
        {
            "instrument":"PI_XBTUSD",
            "time":1600256966528,
            "price":364.65,
            "seq":100,
            "buy":true,
            "qty":5000.0,
            "order_id":"3696d19b-3226-46bd-993d-a9a7aacc8fbc",
            "cli_ord_id":"8b58d9da-fcaf-4f60-91bc-9973a3eba48d",
            "fill_id":"c14ee7cb-ae25-4601-853a-d0205e576099",
            "fill_type":"taker",
            "fee_paid":0.00685588921,
            "fee_currency":"ETH"
        }
    ]
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE), "364.65");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE), "5000.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_BUY);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "3696d19b-3226-46bd-993d-a9a7aacc8fbc");
  EXPECT_EQ(element.getValue(CCAPI_EM_CLIENT_ORDER_ID), "8b58d9da-fcaf-4f60-91bc-9973a3eba48d");
  EXPECT_EQ(element.getValue(CCAPI_IS_MAKER), "0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY), "0.00685588921");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_FEE_ASSET), "ETH");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "PI_XBTUSD");
}

TEST_F(ExecutionManagementServiceKrakenFuturesTest, createEventOpenOrders) {
  Subscription subscription(CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, "PI_XBTUSD", CCAPI_EM_ORDER_UPDATE);
  std::string textMessage = R"(
    {
   "feed":"open_orders",
   "order":{
      "instrument":"PI_XBTUSD",
      "time":1567702877410,
      "last_update_time":1567702877410,
      "qty":304.0,
      "filled":0.0,
      "limit_price":10640.0,
      "stop_price":0.0,
      "type":"limit",
      "order_id":"59302619-41d2-4f0b-941f-7e7914760ad3",
      "direction":1,
      "reduce_only":true
   },
   "is_cancel":false,
   "reason":"new_placed_order_by_user"
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
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_ID), "59302619-41d2-4f0b-941f-7e7914760ad3");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_SIDE), CCAPI_EM_ORDER_SIDE_SELL);
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE), "10640.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_QUANTITY), "304.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY), "0.0");
  EXPECT_EQ(element.getValue(CCAPI_EM_ORDER_INSTRUMENT), "PI_XBTUSD");
  EXPECT_EQ(element.getValue("is_cancel"), "0");
}
} /* namespace ccapi */
#endif
#endif
