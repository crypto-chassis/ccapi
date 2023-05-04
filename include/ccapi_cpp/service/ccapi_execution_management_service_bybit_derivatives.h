#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_DERIVATIVES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_DERIVATIVES_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT_DERIVATIVES
#include "ccapi_cpp/service/ccapi_execution_management_service_bybit_base.h"
namespace ccapi {
class ExecutionManagementServiceBybitDerivatives : public ExecutionManagementServiceBybitBase {
 public:
  ExecutionManagementServiceBybitDerivatives(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions,
                                             SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBybitBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BYBIT_DERIVATIVES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/unified/private/v3";
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
    this->apiKeyName = CCAPI_BYBIT_DERIVATIVES_API_KEY;
    this->apiSecretName = CCAPI_BYBIT_DERIVATIVES_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/unified/v3/private/order/create";
    this->cancelOrderTarget = "/unified/v3/private/order/cancel";
    this->getOrderTarget = "/unified/v3/private/order/list";
    this->getOpenOrdersTarget = "/unified/v3/private/order/unfilled-orders";
    this->cancelOpenOrdersTarget = "/unified/v3/private/order/cancel-all";
    this->getAccountBalancesTarget = "/unified/v3/private/account/wallet/balance";
    this->getAccountPositionsTarget = "/unified/v3/private/position/list";
  }
  virtual ~ExecutionManagementServiceBybitDerivatives() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "qty"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "orderLinkId"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_LIMIT, "limit"},
                       {CCAPI_INSTRUMENT_TYPE, "category"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "Buy" : "Sell";
      }
      if (value != "null") {
        if (key == "reduceOnly" || key == "closeOnTrigger" || key == "mmp") {
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
    }
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, now, credential);
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.set(beast::http::field::content_type, "application/json");
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        if (param.find("orderType") == param.end()) {
          document.AddMember("orderType", rj::Value("Limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId, "symbol");
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, now, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.set(beast::http::field::content_type, "application/json");
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        this->appendSymbolId(document, allocator, symbolId, "symbol");
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, now, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param,
                                       {
                                           {CCAPI_EM_CLIENT_ORDER_ID, "orderLinkId"},
                                           {CCAPI_EM_ORDER_ID, "orderId"},
                                           {CCAPI_LIMIT, "limit"},
                                           {CCAPI_INSTRUMENT_TYPE, "category"},
                                       });
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, queryString, now, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param,
                                       {
                                           {CCAPI_EM_CLIENT_ORDER_ID, "orderLinkId"},
                                           {CCAPI_EM_ORDER_ID, "orderId"},
                                           {CCAPI_LIMIT, "limit"},
                                           {CCAPI_INSTRUMENT_TYPE, "category"},
                                       });
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId, "symbol");
        }
        req.target(this->getOpenOrdersTarget + "?" + queryString);
        this->signRequest(req, queryString, now, credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.set(beast::http::field::content_type, "application/json");
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOpenOrdersTarget);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId, "symbol");
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, now, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParamToQueryString(queryString, {});
        req.target(queryString.empty() ? this->getAccountBalancesTarget : this->getAccountBalancesTarget + "?" + queryString);
        this->signRequest(req, queryString, now, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param);
        req.target(this->getAccountPositionsTarget + "?" + queryString);
        this->signRequest(req, queryString, now, credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    std::map<std::string, std::pair<std::string, JsonDataType> > extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("qty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumExecQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("cumExecValue", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("leavesQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("orderStatus", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::GET_ORDER || operation == Request::Operation::GET_OPEN_ORDERS || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document["result"]["list"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else {
      Element element;
      this->extractOrderInfo(element, document["result"], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document["result"]["coin"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["currencyCoin"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["walletBalance"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["availableBalance"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["result"]["list"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["side"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["entryPrice"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  Event createEvent(const WsConnection& wsConnection, wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage,
                    const rj::Document& document, const TimePoint& timeReceived) override{
#else
  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) override {
    std::string textMessage(textMessageView);
#endif
      Event event;
  std::vector<Message> messageList;
  Message message;
  message.setTimeReceived(timeReceived);
  message.setCorrelationIdList({subscription.getCorrelationId()});
  const auto& fieldSet = subscription.getFieldSet();
  const auto& instrumentSet = subscription.getInstrumentSet();
  if (document.HasMember("topic")) {
    std::string topic = document["topic"].GetString();
    if (topic == "user.order.unifiedAccount") {
      if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        const rj::Value& data = document["data"]["result"];
        for (const auto& x : data.GetArray()) {
          std::string instrument = x["symbol"].GetString();
          if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["updatedTime"].GetString()))));
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("orderLinkId", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("qty", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumExecQty", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("cumExecValue", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_STATUS, std::make_pair("orderStatus", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
            };
            Element info;
            this->extractOrderInfo(info, x, extractionFieldNameMap);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (topic == "user.execution.unifiedAccount") {
      if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        const rj::Value& data = document["data"]["result"];
        for (const auto& x : data.GetArray()) {
          std::string execType = x["execType"].GetString();
          if (execType == "TRADE") {
            std::string instrument = x["symbol"].GetString();
            if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(x["execTime"].GetString()))));
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
              std::vector<Element> elementList;
              Element element;
              element.insert(CCAPI_TRADE_ID, x["execId"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["execPrice"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["execQty"].GetString());
              element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "Buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, std::string(x["lastLiquidityInd"].GetString()) == "MAKER" ? "1" : "0");
              element.insert(CCAPI_EM_ORDER_ID, x["orderId"].GetString());
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["orderLinkId"].GetString());
              element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              {
                auto it = x.FindMember("execFee");
                if (it != x.MemberEnd() && !it->value.IsNull()) {
                  element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(it->value.GetString()));
                }
              }
              elementList.emplace_back(std::move(element));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        }
      }
    }
  } else if (document.HasMember("type")) {
    std::string type = document["type"].GetString();
    if (type == "AUTH_RESP") {
      bool success = document["success"].GetBool();
      if (success) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("subscribe").Move(), allocator);
        rj::Value args(rj::kArrayType);
        for (const auto& field : subscription.getFieldSet()) {
          std::string channelId;
          if (field == CCAPI_EM_ORDER_UPDATE) {
            channelId = "user.order.unifiedAccount";
          } else if (field == CCAPI_EM_PRIVATE_TRADE) {
            channelId = "user.execution.unifiedAccount";
          }
          args.PushBack(rj::Value(channelId.c_str(), allocator).Move(), allocator);
        }
        document.AddMember("args", args, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        ErrorCode ec;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
        this->send(hdl, sendString, wspp::frame::opcode::text, ec);
#else
          this->send(wsConnectionPtr, sendString, ec);
#endif
        if (ec) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
        }
      }
    } else if (type == "COMMAND_RESP") {
      bool success = document["success"].GetBool();
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(success ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(success ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    }
  } else if (document.HasMember("ret_code")) {
    std::string ret_code = document["ret_code"].GetString();
    if (ret_code != "0") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    }
  }
  event.setMessageList(messageList);
  return event;
}
};  // namespace ccapi
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_DERIVATIVES_H_
