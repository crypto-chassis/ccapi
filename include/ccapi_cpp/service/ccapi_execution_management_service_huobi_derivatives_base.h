#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_DERIVATIVES_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_DERIVATIVES_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)
#include "ccapi_cpp/ccapi_decimal.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_base.h"
namespace ccapi {
class ExecutionManagementServiceHuobiDerivativesBase : public ExecutionManagementServiceHuobiBase {
 public:
  ExecutionManagementServiceHuobiDerivativesBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions,
                                                 SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceHuobiBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->isDerivatives = true;
    // this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*[eE]?-?\\d*)");
    this->needDecompressWebsocketMessage = true;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
    ErrorCode ec = this->inflater.init(false, 31);
#else
    this->inflater.setWindowBitsOverride(31);
    ErrorCode ec = this->inflater.init();
#endif
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
  }
  virtual ~ExecutionManagementServiceHuobiDerivativesBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find("err_code") != std::string::npos; }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(document, allocator, symbolId, "contract_code");
  }
  void appendSymbolId(std::map<std::string, std::string>& queryParamMap, const std::string& symbolId) {
    ExecutionManagementServiceHuobiBase::appendSymbolId(queryParamMap, symbolId, "contract_code");
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
                              {CCAPI_EM_ORDER_SIDE, "direction"},
                              {CCAPI_EM_ORDER_QUANTITY, "volume"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                              {CCAPI_EM_ORDER_LEVERAGE, "lever_rate"},
                          });
        if (param.find("offset") == param.end()) {
          document.AddMember("offset", rj::Value("open").Move(), allocator);
        }
        if (param.find("lever_rate") == param.end()) {
          document.AddMember("lever_rate", rj::Value("1").Move(), allocator);
        }
        if (param.find("order_price_type") == param.end()) {
          document.AddMember("order_price_type", rj::Value("limit").Move(), allocator);
        }
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
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "order_id"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                          });
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->cancelOrderTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "order_id"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                          });
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->getOrderTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, this->getOpenOrdersTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        this->signRequest(req, this->getAccountBalancesTarget, queryParamMap, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::post);
        this->signRequest(req, this->getAccountPositionsTarget, queryParamMap, credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("direction", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("volume", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("trade_volume", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("contract_code", JsonDataType::STRING)}};
    const rj::Value& data = document["data"];
    if (operation == Request::Operation::CREATE_ORDER) {
      Element element;
      ExecutionManagementServiceHuobiDerivativesBase::extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::CANCEL_ORDER) {
      Element element;
      element.insert(CCAPI_EM_ORDER_ID, std::string(data["successes"].GetString()));
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_ORDER) {
      Element element;
      ExecutionManagementServiceHuobiDerivativesBase::extractOrderInfo(element, data[0], extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : data["orders"].GetArray()) {
        Element element;
        ExecutionManagementServiceHuobiDerivativesBase::extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    const auto& data = document["data"];
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        auto it = data[0].FindMember("margin_asset");
        element.insert(CCAPI_EM_ASSET, it != data[0].MemberEnd() ? it->value.GetString() : data[0]["symbol"].GetString());
        auto marginAvailable = Decimal(data[0]["margin_balance"].GetString())
                                   .subtract(Decimal(data[0]["margin_position"].GetString()))
                                   .subtract(Decimal(data[0]["margin_frozen"].GetString()))
                                   .toString();
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, marginAvailable);
        elementList.emplace_back(std::move(element));
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : data.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["contract_code"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["direction"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["available"].GetString());
          element.insert(CCAPI_EM_POSITION_COST, x["cost_open"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["lever_rate"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  void extractOrderInfo(Element& element, const rj::Value& x,
                        const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap) override {
    ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("trade_volume");
      auto it2 = x.FindMember("trade_avg_price");
      if (it1 != x.MemberEnd() && it2 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                       std::to_string(std::stod(it1->value.GetString()) * (it2->value.IsNull() ? 0 : std::stod(it2->value.GetString()))));
      }
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    std::string timestamp = UtilTime::getISOTimestamp<std::chrono::seconds>(now, "%FT%T");
    std::string signature;
    std::string queryString;
    std::map<std::string, std::string> queryParamMap;
    queryParamMap.insert(std::make_pair("AccessKeyId", apiKey));
    queryParamMap.insert(std::make_pair("SignatureMethod", "HmacSHA256"));
    queryParamMap.insert(std::make_pair("SignatureVersion", "2"));
    queryParamMap.insert(std::make_pair("Timestamp", Url::urlEncode(timestamp)));
    this->createSignature(signature, queryString, "GET", this->hostRest, this->authenticationPath, queryParamMap, credential);
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("auth").Move(), allocator);
    document.AddMember("type", rj::Value("api").Move(), allocator);
    document.AddMember("AccessKeyId", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    document.AddMember("SignatureMethod", rj::Value("HmacSHA256").Move(), allocator);
    document.AddMember("SignatureVersion", rj::Value("2").Move(), allocator);
    document.AddMember("Timestamp", rj::Value(timestamp.c_str(), allocator).Move(), allocator);
    document.AddMember("Signature", rj::Value(signature.c_str(), allocator).Move(), allocator);
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
    std::string op = document["op"].GetString();
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (op == "auth") {
      std::string errCode = document["err-code"].GetString();
      if (errCode == "0") {
        for (const auto& instrument : instrumentSet) {
          for (const auto& field : fieldSet) {
            rj::Document document;
            document.SetObject();
            auto& allocator = document.GetAllocator();
            document.AddMember("op", rj::Value("sub").Move(), allocator);
            std::string topic;
            if (field == CCAPI_EM_ORDER_UPDATE) {
              topic = this->orderDataTopic + "." + instrument;
            } else if (field == CCAPI_EM_PRIVATE_TRADE) {
              topic = this->matchOrderDataTopic + "." + instrument;
            }
            document.AddMember("topic", rj::Value(topic.c_str(), allocator).Move(), allocator);
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
    } else if (op == "ping") {
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
      Event event = this->createEvent(subscription, textMessage, document, op, timeReceived);
      if (!event.getMessageList().empty()) {
        this->eventHandler(event, nullptr);
      }
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& op,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (op == "notify") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      std::string topic = document["topic"].GetString();
      if (topic.rfind(this->orderDataTopic + ".", 0) == 0 && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
        std::string instrument = document["contract_code"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(document["ts"].GetString())));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
              {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_SIDE, std::make_pair("direction", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("volume", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("trade_volume", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("contract_code", JsonDataType::STRING)},
          };
          Element info;
          this->extractOrderInfo(info, document, extractionFieldNameMap);
          {
            auto it1 = document.FindMember("trade_volume");
            auto it2 = document.FindMember("trade_avg_price");
            if (it1 != document.MemberEnd() && it2 != document.MemberEnd()) {
              info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                          std::to_string(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())));
            }
          }
          for (const auto& x : document["trade"].GetArray()) {
            info.insert(CCAPI_TRADE_ID, std::string(x["trade_id"].GetString()));
            info.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["trade_price"].GetString());
            info.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["trade_volume"].GetString());
            info.insert(CCAPI_IS_MAKER, std::string(x["role"].GetString()) == "maker" ? "1" : "0");
          }
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      } else if (topic.rfind(this->matchOrderDataTopic + ".", 0) == 0 && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
        std::string instrument = document["contract_code"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          std::string orderSide = std::string(document["direction"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL;
          std::string positionSide = document["offset"].GetString();
          std::string orderId = document["order_id"].GetString();
          std::string clientOrderId;
          auto it = document.FindMember("client_order_id");
          if (!it->value.IsNull()) {
            clientOrderId = it->value.GetString();
          }
          for (const auto& x : document["trade"].GetArray()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(x["created_at"].GetString())));
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_TRADE_ID, std::string(x["trade_id"].GetString()));
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["trade_price"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["trade_volume"].GetString());
            element.insert(CCAPI_EM_ORDER_SIDE, orderSide);
            element.insert(CCAPI_EM_POSITION_SIDE, positionSide);
            element.insert(CCAPI_IS_MAKER, std::string(x["role"].GetString()) == "maker" ? "1" : "0");
            element.insert(CCAPI_EM_ORDER_ID, orderId);
            if (!clientOrderId.empty()) {
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, clientOrderId);
            }
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (op == "sub") {
      auto it = document.FindMember("err-code");
      if (it != document.MemberEnd() && std::string(it->value.GetString()) != "0") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        message.setType(Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      } else {
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
  std::string authenticationPath;
  std::string orderDataTopic;
  std::string matchOrderDataTopic;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_DERIVATIVES_BASE_H_
