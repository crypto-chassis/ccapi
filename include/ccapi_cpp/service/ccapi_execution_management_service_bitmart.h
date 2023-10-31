#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITMART_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITMART_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITMART
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBitmart : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBitmart(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                    ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITMART;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/user?protocol=1.1";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    //     try {
    //       this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->apiKeyName = CCAPI_BITMART_API_KEY;
    this->apiSecretName = CCAPI_BITMART_API_SECRET;
    this->apiMemoName = CCAPI_BITMART_API_MEMO;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiMemoName});
    this->createOrderTarget = "/spot/v2/submit_order";
    this->cancelOrderTarget = "/spot/v3/cancel_order";
    this->getOrderTarget = "/spot/v2/order_detail";
    this->getOpenOrdersTarget = "/spot/v3/orders";
    this->cancelOpenOrdersTarget = "/spot/v1/cancel_orders";
    this->getAccountBalancesTarget = "/spot/v1/wallet";
    this->needDecompressWebsocketMessage = true;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    ErrorCode ec = this->inflater.init(false);
#else
    ErrorCode ec = this->inflater.init();
#endif
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
  }
  virtual ~ExecutionManagementServiceBitmart() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, "ping", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*1000")); }
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("X-BM-TIMESTAMP").to_string();
    preSignedText += "#";
    preSignedText += mapGetWithDefault(credential, this->apiMemoName);
    preSignedText += "#";
    std::string paramString;
    if (methodString == "GET" || methodString == "DELETE") {
      paramString = queryString;
    } else if (methodString == "POST" || methodString == "PUT") {
      paramString = body;
    }
    preSignedText += paramString;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "X-BM-SIGN:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& paramString, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("X-BM-TIMESTAMP").to_string();
    preSignedText += "#";
    preSignedText += mapGetWithDefault(credential, this->apiMemoName);
    preSignedText += "#";
    preSignedText += paramString;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("X-BM-SIGN", signature);
  }
  void appendParam(Request::Operation operation, rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "size"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                       {CCAPI_SYMBOL_ID, "symbol"},
                       {CCAPI_EM_ORDER_ID, "order_id"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_ID, "order_id"},
                       {CCAPI_LIMIT, "N"},
                   }) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
  }
  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    rjValue.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-BM-KEY", apiKey);
    req.set("X-BM-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(operation, document, allocator, param);
        if (param.find("type") == param.end()) {
          document.AddMember("type", rj::Value("limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.target(this->cancelOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(operation, document, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param);
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        queryString += "&status=9";
        if (queryString.back() == '&') {
          queryString.pop_back();
        }
        req.target(queryString.empty() ? this->getOpenOrdersTarget : this->getOpenOrdersTarget + "?" + queryString);
        this->signRequest(req, queryString, credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->cancelOpenOrdersTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(operation, document, allocator, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        req.target(this->getAccountBalancesTarget);
        this->signRequest(req, "", credential);
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
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_size", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("filled_notional", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    const rj::Value& data = document["data"];
    if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : data["orders"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
    } else {
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document["data"]["wallet"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["id"].GetString());
          std::string available = x["available"].GetString();
          std::string frozen = x["frozen"].GetString();
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, available);
          element.insert(CCAPI_EM_QUANTITY_TOTAL, (Decimal(available).add(Decimal(frozen))).toString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("login").Move(), allocator);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = timestamp + "#" + mapGetWithDefault(credential, this->apiMemoName) + "#bitmart.WebSocket";
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    rj::Value args(rj::kArrayType);
    args.PushBack(rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    args.PushBack(rj::Value(timestamp.c_str(), allocator).Move(), allocator);
    args.PushBack(rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage,
                     const TimePoint& timeReceived) override {
#else
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    if (textMessage != "pong") {
      rj::Document document;
      document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
      auto it = document.FindMember("errorCode");
      std::string errorCode = it != document.MemberEnd() ? it->value.GetString() : "";
      Event event;
      const auto& correlationId = subscription.getCorrelationId();
      if (errorCode.empty()) {
        auto it = document.FindMember("event");
        std::string eventStr = it != document.MemberEnd() ? it->value.GetString() : "";
        if (eventStr == "login") {
          rj::Document document;
          document.SetObject();
          auto& allocator = document.GetAllocator();
          document.AddMember("op", rj::Value("subscribe").Move(), allocator);
          rj::Value args(rj::kArrayType);
          const auto& fieldSet = subscription.getFieldSet();
          const auto& instrumentSet = subscription.getInstrumentSet();
          for (const auto& instrument : instrumentSet) {
            std::string arg = "spot/user/order:" + instrument;
            args.PushBack(rj::Value(arg.c_str(), allocator).Move(), allocator);
          }
          document.AddMember("args", args, allocator);
          rj::StringBuffer stringBufferSubscribe;
          rj::Writer<rj::StringBuffer> writerSubscribe(stringBufferSubscribe);
          document.Accept(writerSubscribe);
          std::string sendString = stringBufferSubscribe.GetString();
          ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          this->send(wsConnection.hdl, sendString, wspp::frame::opcode::text, ec);
#else
          this->send(wsConnectionPtr, sendString, ec);
#endif
          if (ec) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
          }
          // else {
          //   this->serviceContextPtr->tlsClientPtr->set_timer(1000,
          //        [wsConnection,correlationId,sendString,that=this](ErrorCode const& ec) {
          //          auto timeReceived=UtilTime::now();
          //          if (ec) {
          //            that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
          //          } else {
          //            if (!that->subscriptionFailedByConnectionIdCorrelationIdMap[wsConnection.id][correlationId]){
          //              Event event;
          //              event.setType(Event::Type::SUBSCRIPTION_STATUS);
          //              std::vector<Message> messageList;
          //              Message message;
          //              message.setTimeReceived(timeReceived);
          //              message.setCorrelationIdList({correlationId});
          //              message.setType(Message::Type::SUBSCRIPTION_STARTED);
          //              Element element;
          //              element.insert(CCAPI_INFO_MESSAGE, sendString);
          //              message.setElementList({element});
          //              messageList.emplace_back(std::move(message));
          //              event.addMessages(messageList);
          //              that->eventHandler(event, nullptr);
          //            }
          //          }
          //        });
          // }
        } else {
          event = this->createEvent(subscription, textMessage, document, eventStr, timeReceived);
        }
      } else {
        std::string eventStr = document["event"].GetString();
        if (eventStr.rfind("spot/user/order:", 0) == 0) {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          message.setCorrelationIdList({correlationId});
          messageList.emplace_back(std::move(message));
          event.setMessageList(messageList);
          // this->subscriptionFailedByConnectionIdCorrelationIdMap[wsConnection.id][correlationId] = true;
        }
      }
      if (!event.getMessageList().empty()) {
        this->eventHandler(event, nullptr);
      }
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& eventStr,
                    const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    const auto& correlationId = subscription.getCorrelationId();
    message.setCorrelationIdList({correlationId});
    const auto& fieldSet = subscription.getFieldSet();
    const rj::Value& data = document["data"];
    if (data.IsString() && std::string(data.GetString()).empty() || data.IsArray() && data.GetArray().Empty()) {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else {
      std::string table = std::string(document["table"].GetString());
      if (table == "spot/user/order") {
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        for (const auto& x : data.GetArray()) {
          std::string instrument = x["symbol"].GetString();
          const auto& instrumentSet = subscription.getInstrumentSet();
          if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
            if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
              const auto& itDetailId = x.FindMember("detail_id");
              if (itDetailId != x.MemberEnd() && !itDetailId->value.IsNull()) {
                std::string tradeId = itDetailId->value.GetString();
                if (!tradeId.empty()) {
                  Message message;
                  message.setTimeReceived(timeReceived);
                  message.setCorrelationIdList({subscription.getCorrelationId()});
                  message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["last_fill_time"].GetString()))));
                  message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                  std::vector<Element> elementList;
                  Element element;
                  element.insert(CCAPI_TRADE_ID, tradeId);
                  element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(x["last_fill_price"].GetString()));
                  element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(x["last_fill_count"].GetString()));
                  element.insert(CCAPI_EM_ORDER_SIDE, std::string(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                  element.insert(CCAPI_IS_MAKER, std::string(x["exec_type"].GetString()) == "M" ? "1" : "0");
                  element.insert(CCAPI_EM_ORDER_ID, std::string(x["order_id"].GetString()));
                  const auto& itClientOrderId = x.FindMember("client_order_id");
                  if (itClientOrderId != x.MemberEnd() && !itClientOrderId->value.IsNull()) {
                    element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(itClientOrderId->value.GetString()));
                  }
                  element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                  element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(x["dealFee"].GetString()));
                  elementList.emplace_back(std::move(element));
                  message.setElementList(elementList);
                  messageList.emplace_back(std::move(message));
                }
              }
            }
            if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["ms_t"].GetString()))));
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_size", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("filled_notional", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
              };
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              info.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  // void onClose(wspp::connection_hdl hdl) override {
  //   WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
  //   this->subscriptionFailedByConnectionIdCorrelationIdMap.erase(wsConnection.id);
  //   ExecutionManagementService::onClose(hdl);
  // }
  std::string apiMemoName;
  // std::map<std::string, std::map<std::string, bool>> subscriptionFailedByConnectionIdCorrelationIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITMART_H_
