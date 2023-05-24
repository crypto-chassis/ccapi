#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/service/ccapi_execution_management_service_bybit_base.h"
namespace ccapi {
class ExecutionManagementServiceBybit : public ExecutionManagementServiceBybitBase {
 public:
  ExecutionManagementServiceBybit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                  ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBybitBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BYBIT;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/spot/private/v3";
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
    this->apiKeyName = CCAPI_BYBIT_API_KEY;
    this->apiSecretName = CCAPI_BYBIT_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/spot/v3/private/order";
    this->cancelOrderTarget = "/spot/v3/private/cancel-order";
    this->getOrderTarget = "/spot/v3/private/order";
    this->getOpenOrdersTarget = "/spot/v3/private/open-orders";
    this->cancelOpenOrdersTarget = "/spot/v3/private/cancel-orders";
    this->getAccountBalancesTarget = "/spot/v3/private/account";
  }
  virtual ~ExecutionManagementServiceBybit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "orderQty"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "orderPrice"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "orderLinkId"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                       {CCAPI_LIMIT, "limit"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "Buy" : "Sell";
      }
      if (value != "null") {
        if (key == "orderCategory") {
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), std::stoi(value), allocator);
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
          document.AddMember("orderType", rj::Value("LIMIT").Move(), allocator);
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
        this->appendParamToQueryString(queryString, param);
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, queryString, now, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param);
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
        req.target(this->getAccountBalancesTarget);
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
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("orderLinkId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("orderQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("orderPrice", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("execQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("cummulativeQuoteQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : document["result"]["list"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (operation != Request::Operation::CANCEL_OPEN_ORDERS) {
      Element element;
      this->extractOrderInfo(element, document["result"], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document["result"]["balances"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["coin"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["total"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
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
    if (topic == "order") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = document["data"];
      for (const auto& x : data.GetArray()) {
        std::string instrument = x["s"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          auto time = TimePoint(std::chrono::milliseconds(std::stoll(x["E"].GetString())));
          auto itTradeId = x.FindMember("t");
          if (itTradeId != x.MemberEnd() && !itTradeId->value.IsNull() && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(time);
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_TRADE_ID, itTradeId->value.GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["L"].GetString()));
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["l"].GetString()));
            element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["S"].GetString()) == "BUY" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            element.insert(CCAPI_IS_MAKER, x["m"].GetBool() ? "1" : "0");
            element.insert(CCAPI_EM_ORDER_ID, std::string(x["i"].GetString()));
            element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(x["c"].GetString()));
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            {
              auto it = x.FindMember("n");
              if (it != x.MemberEnd() && !it->value.IsNull()) {
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(it->value.GetString()));
              }
            }
            {
              auto it = x.FindMember("N");
              if (it != x.MemberEnd() && !it->value.IsNull()) {
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(it->value.GetString()));
              }
            }
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(time);
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("i", JsonDataType::STRING)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("c", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_SIDE, std::make_pair("S", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("p", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("q", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("z", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("Z", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_STATUS, std::make_pair("X", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("s", JsonDataType::STRING)},
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
    }
  } else if (document.HasMember("op")) {
    std::string op = document["op"].GetString();
    if (op == "auth") {
      bool success = document["success"].GetBool();
      if (success) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("subscribe").Move(), allocator);
        rj::Value args(rj::kArrayType);
        args.PushBack(rj::Value("order").Move(), allocator);
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
    } else if (op == "subscribe") {
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_H_
