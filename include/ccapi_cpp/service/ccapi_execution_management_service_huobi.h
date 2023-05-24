#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_base.h"
namespace ccapi {
class ExecutionManagementServiceHuobi : public ExecutionManagementServiceHuobiBase {
 public:
  ExecutionManagementServiceHuobi(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                  ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceHuobiBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HUOBI;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/v2";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->apiKeyName = CCAPI_HUOBI_API_KEY;
    this->apiSecretName = CCAPI_HUOBI_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/v1/order/orders/place";
    this->cancelOrderTarget = "/v1/order/orders/{order-id}/submitcancel";
    this->cancelOrderByClientOrderIdTarget = "/v1/order/orders/submitCancelClientOrder";
    this->getOrderTarget = "/v1/order/orders/{order-id}";
    this->getOrderByClientOrderIdTarget = "/v1/order/orders/getClientOrder";
    this->getOpenOrdersTarget = "/v1/order/openOrders";
    this->cancelOpenOrdersTarget = "/v1/order/orders/batchCancelOpenOrders";
    this->getAccountsTarget = "/v1/account/accounts";
    this->getAccountBalancesTarget = "/v1/account/accounts/{account-id}/balance";
  }
  virtual ~ExecutionManagementServiceHuobi() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find("err-code") != std::string::npos; }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(document, allocator, symbolId, "symbol");
  }
  void appendSymbolId(std::map<std::string, std::string>& queryParamMap, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(queryParamMap, symbolId, "symbol");
  }
  void convertReqDetail(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                        const std::map<std::string, std::string>& credential, std::map<std::string, std::string>& queryParamMap) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_SIDE, "type"},
                              {CCAPI_EM_ORDER_QUANTITY, "amount"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client-order-id"},
                              {CCAPI_EM_ACCOUNT_ID, "account-id"},
                          });
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->createOrderTarget, queryParamMap, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = shouldUseOrderId                                      ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        auto target =
            shouldUseOrderId ? std::regex_replace(this->cancelOrderTarget, std::regex("\\{order\\-id\\}"), id) : this->cancelOrderByClientOrderIdTarget;
        if (!shouldUseOrderId) {
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          this->appendParam(document, allocator, param,
                            {
                                {CCAPI_EM_CLIENT_ORDER_ID, "client-order-id"},
                            });
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          auto body = stringBuffer.GetString();
          req.body() = body;
          req.prepare_payload();
        }
        this->signRequest(req, target, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
        std::string id = shouldUseOrderId                                      ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        auto target = shouldUseOrderId ? std::regex_replace(this->getOrderTarget, std::regex("\\{order\\-id\\}"), id) : this->getOrderByClientOrderIdTarget;
        req.target(target);
        if (!shouldUseOrderId) {
          ExecutionManagementServiceHuobiBase::appendParam(queryParamMap, param,
                                                           {
                                                               {CCAPI_EM_ACCOUNT_ID, "account-id"},
                                                           });
          queryParamMap.insert(std::make_pair("clientOrderId", Url::urlEncode(id)));
        }
        this->signRequest(req, target, queryParamMap, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        ExecutionManagementServiceHuobiBase::appendParam(queryParamMap, {},
                                                         {
                                                             {CCAPI_EM_ACCOUNT_ID, "account-id"},
                                                         });
        if (!symbolId.empty()) {
          this->appendSymbolId(queryParamMap, symbolId);
        }
        this->signRequest(req, this->getOpenOrdersTarget, queryParamMap, credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account-id"},
                          });
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->cancelOpenOrdersTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::get);
        this->signRequest(req, this->getAccountsTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto accountId = mapGetWithDefault(param, std::string(CCAPI_EM_ACCOUNT_ID));
        auto target = std::regex_replace(this->getAccountBalancesTarget, std::regex("\\{account\\-id\\}"), accountId);
        this->signRequest(req, target, queryParamMap, credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::INTEGER)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client-order-id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled-amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("filled-cash-amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    const rj::Value& data = document["data"];
    if (operation == Request::Operation::CREATE_ORDER || operation == Request::Operation::CANCEL_ORDER) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, std::string(data.GetString()));
      elementList.emplace_back(std::move(element));
    } else if (data.IsObject()) {
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      if (operation == Request::Operation::GET_ORDER) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::string(data["field-amount"].GetString()));
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::string(data["field-cash-amount"].GetString()));
      }
      elementList.emplace_back(std::move(element));
    } else {
      for (const auto& x : data.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    const auto& data = document["data"];
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : data.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, std::string(x["id"].GetString()));
          element.insert(CCAPI_EM_ACCOUNT_TYPE, x["type"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : data["list"].GetArray()) {
          if (std::string(x["type"].GetString()) == "trade") {
            Element element;
            element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["balance"].GetString());
            element.insert(CCAPI_EM_QUANTITY_TOTAL, x["balance"].GetString());
            elementList.emplace_back(std::move(element));
          }
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    std::string timestamp = UtilTime::getISOTimestamp<std::chrono::seconds>(now, "%FT%T");
    std::string signature;
    std::string queryString;
    std::map<std::string, std::string> queryParamMap;
    queryParamMap.insert(std::make_pair("accessKey", apiKey));
    queryParamMap.insert(std::make_pair("signatureMethod", "HmacSHA256"));
    queryParamMap.insert(std::make_pair("signatureVersion", "2.1"));
    queryParamMap.insert(std::make_pair("timestamp", Url::urlEncode(timestamp)));
    this->createSignature(signature, queryString, "GET", this->hostRest, "/ws/v2", queryParamMap, credential);
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("action", rj::Value("req").Move(), allocator);
    document.AddMember("ch", rj::Value("auth").Move(), allocator);
    rj::Value params(rj::kObjectType);
    params.AddMember("authType", rj::Value("api").Move(), allocator);
    params.AddMember("accessKey", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    params.AddMember("signatureMethod", rj::Value("HmacSHA256").Move(), allocator);
    params.AddMember("signatureVersion", rj::Value("2.1").Move(), allocator);
    params.AddMember("timestamp", rj::Value(timestamp.c_str(), allocator).Move(), allocator);
    params.AddMember("signature", rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("params", params, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage,
                     const TimePoint& timeReceived) override {
#else
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    std::string actionStr = document["action"].GetString();
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (actionStr == "req") {
      std::string chStr = document["ch"].GetString();
      if (chStr == "auth") {
        int code = std::stoi(document["code"].GetString());
        if (code == 200) {
          std::set<std::string> symbols;
          if (instrumentSet.empty()) {
            symbols = {"*"};
          } else {
            symbols = instrumentSet;
          }
          for (const auto& symbol : symbols) {
            for (const auto& field : fieldSet) {
              rj::Document document;
              document.SetObject();
              auto& allocator = document.GetAllocator();
              document.AddMember("action", rj::Value("sub").Move(), allocator);
              std::string ch;
              if (field == CCAPI_EM_ORDER_UPDATE) {
                ch = "orders#" + symbol;
              } else if (field == CCAPI_EM_PRIVATE_TRADE) {
                ch = "trade.clearing#" + symbol;
              }
              document.AddMember("ch", rj::Value(ch.c_str(), allocator).Move(), allocator);
              rj::StringBuffer stringBufferSubscribe;
              rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
              document.Accept(writerSubscribe);
              std::string sendString = stringBufferSubscribe.GetString();
              ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
              this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
              this->send(wsConnectionPtr, sendString, ec);
#endif
              if (ec) {
                this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
              }
            }
          }
        } else {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, textMessage, {subscription.getCorrelationId()});
        }
      }
    } else if (actionStr == "ping") {
      rj::StringBuffer stringBufferSubscribe;
      rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
      document.Accept(writerSubscribe);
      std::string sendString = stringBufferSubscribe.GetString();
      std::string toReplace("ping");
      sendString.replace(sendString.find(toReplace), toReplace.length(), "pong");
      ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
      this->send(wsConnectionPtr, sendString, ec);
#endif
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "pong");
      }
    } else {
      Event event = this->createEvent(subscription, textMessage, document, actionStr, timeReceived);
      if (!event.getMessageList().empty()) {
        this->eventHandler(event, nullptr);
      }
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& actionStr,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (actionStr == "push") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      std::string ch = document["ch"].GetString();
      if (ch.rfind("orders#", 0) == 0 && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
        const rj::Value& data = document["data"];
        std::string instrument = data["symbol"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          auto it = data.FindMember("lastActTime");
          if (it == data.MemberEnd()) {
            it = data.FindMember("orderCreateTime");
            if (it == data.MemberEnd()) {
              it = data.FindMember("tradeTime");
            }
          }
          message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(it->value.GetString())));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
              {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("orderPrice", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("orderSize", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("remainAmt", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("execAmt", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_STATUS, std::make_pair("orderStatus", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
          };
          Element info;
          this->extractOrderInfo(info, data, extractionFieldNameMap);
          std::string dataEventType = data["eventType"].GetString();
          if (dataEventType == "trigger" || dataEventType == "deletion") {
            info.insert(CCAPI_EM_ORDER_SIDE, std::string(data["orderSide"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          } else if (dataEventType == "trade") {
            info.insert(CCAPI_TRADE_ID, std::string(data["tradeId"].GetString()));
            info.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["tradePrice"].GetString());
            info.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["tradeVolume"].GetString());
            info.insert(CCAPI_IS_MAKER, data["aggressor"].GetBool() ? "0" : "1");
          }
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      } else if (ch.rfind("trade.clearing#", 0) == 0 && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
        const rj::Value& data = document["data"];
        std::string instrument = data["symbol"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(data["tradeTime"].GetString())));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, std::string(data["tradeId"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["tradePrice"].GetString());
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["tradeVolume"].GetString());
          element.insert(CCAPI_EM_ORDER_SIDE, std::string(data["orderSide"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_IS_MAKER, data["aggressor"].GetBool() ? "0" : "1");
          element.insert(CCAPI_EM_ORDER_ID, std::string(data["orderId"].GetString()));
          element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(data["clientOrderId"].GetString()));
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
          element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(data["transactFee"].GetString()));
          element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(data["feeCurrency"].GetString()));
          elementList.emplace_back(std::move(element));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    } else if (actionStr == "sub") {
      int code = std::stoi(document["code"].GetString());
      if (code == 200) {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        message.setType(Message::Type::SUBSCRIPTION_STARTED);
        Element element;
        element.insert(CCAPI_INFO_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  std::string cancelOrderByClientOrderIdTarget;
  std::string getOrderByClientOrderIdTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_H_
