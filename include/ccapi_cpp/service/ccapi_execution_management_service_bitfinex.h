#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITFINEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITFINEX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBitfinex : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBitfinex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                     ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITFINEX;
    this->baseUrlWs = std::string(CCAPI_BITFINEX_PRIVATE_URL_WS_BASE) + "/ws/2";
    this->baseUrlRest = CCAPI_BITFINEX_PRIVATE_URL_REST_BASE;
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
    this->apiKeyName = CCAPI_BITFINEX_API_KEY;
    this->apiSecretName = CCAPI_BITFINEX_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/v2/auth/w/order/submit";
    this->cancelOrderTarget = "/v2/auth/w/order/cancel";
    this->getOrderTarget = "/v2/auth/r/orders/:Symbol/hist";
    this->getOpenOrdersTarget = "/v2/auth/r/orders/:Symbol";
    this->getAccountBalancesTarget = "/v2/auth/r/wallets";
    this->getAccountPositionsTarget = "/v2/auth/r/positions";
  }
  virtual ~ExecutionManagementServiceBitfinex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"cid\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"event\":\"ping\"}", wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr, "{\"cid\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"event\":\"ping\"}", ec);
  }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, request, now, credential);
    this->signRequest(req, path, body, credential);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::string& body,
                   const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = "/api";
    preSignedText += path;
    preSignedText += req.base().at("bfx-nonce").to_string();
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, preSignedText, true);
    req.set("bfx-signature", signature);
    req.target(path);
    req.body() = body;
    req.prepare_payload();
  }
  void appendParam(const Request& request, rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "cid"},
                       {CCAPI_EM_ORDER_ID, "id"},
                       {CCAPI_EM_POSITION_LEVERAGE, "lev"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key != CCAPI_EM_ORDER_QUANTITY && key != CCAPI_EM_ORDER_SIDE) {
        if (key != "id" & key != "gid" && key != "cid" && key != "flags" && key != "lev") {
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        } else {
          int64_t x = std::stoll(value);
          rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(x).Move(), allocator);
        }
      }
    }
  }
  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    rjValue.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void prepareReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    int64_t nonce = this->generateNonce(now, request.getIndex());
    req.set("bfx-nonce", std::to_string(nonce));
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("bfx-apikey", apiKey);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, request, now, credential);
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
        this->appendParam(request, document, allocator, param);
        this->appendSymbolId(document, allocator, symbolId);
        auto orderQuantity = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_QUANTITY));
        auto orderSide = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_SIDE));
        std::string amount = orderSide == CCAPI_EM_ORDER_SIDE_BUY ? orderQuantity : "-" + orderQuantity;
        document.AddMember("amount", rj::Value(amount.c_str(), allocator).Move(), allocator);
        if (param.find("type") == param.end()) {
          document.AddMember("type", rj::Value("EXCHANGE LIMIT").Move(), allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, this->createOrderTarget, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(request, document, allocator, param);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req, this->cancelOrderTarget, body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(request, document, allocator, param);
        auto id = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_ID));
        if (!id.empty()) {
          rj::Value ids(rj::kArrayType);
          for (const auto& x : UtilString::split(id, ",")) {
            ids.PushBack(rj::Value(x.c_str(), allocator).Move(), allocator);
          }
          document.AddMember("id", ids, allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req,
                          symbolId.empty() ? UtilString::replaceFirstOccurrence(this->getOrderTarget, "/:Symbol", "")
                                           : UtilString::replaceFirstOccurrence(this->getOrderTarget, ":Symbol", symbolId),
                          body, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(request, document, allocator, param);
        auto id = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_ID));
        if (!id.empty()) {
          rj::Value ids(rj::kArrayType);
          for (const auto& x : UtilString::split(id, ",")) {
            ids.PushBack(rj::Value(x.c_str(), allocator).Move(), allocator);
          }
          document.AddMember("id", ids, allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        this->signRequest(req,
                          symbolId.empty() ? UtilString::replaceFirstOccurrence(this->getOpenOrdersTarget, "/:Symbol", "")
                                           : UtilString::replaceFirstOccurrence(this->getOrderTarget, ":Symbol", symbolId),
                          body, credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->signRequest(req, this->getAccountBalancesTarget, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->signRequest(req, this->getAccountPositionsTarget, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, const WsConnection& wsConnection, const Request& request,
                                  int wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        document.SetArray();
        document.PushBack(rj::Value(0).Move(), allocator);
        document.PushBack(rj::Value("on", allocator).Move(), allocator);
        document.PushBack(rj::Value().Move(), allocator);
        rj::Value inputDetails(rj::kObjectType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(request, inputDetails, allocator, param);
        this->appendSymbolId(inputDetails, allocator, symbolId);
        auto orderQuantity = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_QUANTITY));
        auto orderSide = mapGetWithDefault(param, std::string(CCAPI_EM_ORDER_SIDE));
        std::string amount = orderSide == CCAPI_EM_ORDER_SIDE_BUY ? orderQuantity : "-" + orderQuantity;
        inputDetails.AddMember("amount", rj::Value(amount.c_str(), allocator).Move(), allocator);
        if (param.find("type") == param.end()) {
          inputDetails.AddMember("type", rj::Value("EXCHANGE LIMIT").Move(), allocator);
        }
        document.PushBack(inputDetails, allocator);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        document.SetArray();
        document.PushBack(rj::Value(0).Move(), allocator);
        document.PushBack(rj::Value("oc", allocator).Move(), allocator);
        document.PushBack(rj::Value().Move(), allocator);
        rj::Value inputDetails(rj::kObjectType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(request, inputDetails, allocator, param);
        document.PushBack(inputDetails, allocator);
      } break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnection, request, wsRequestId, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    if (operation == Request::Operation::CREATE_ORDER) {
      Element element;
      this->extractOrderInfo(element, document[4][0]);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::CANCEL_ORDER) {
      Element element;
      this->extractOrderInfo(element, document[4]);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_ORDER) {
      Element element;
      this->extractOrderInfo(element, document[0]);
      elementList.emplace_back(std::move(element));
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      for (const auto& x : document.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x);
        elementList.emplace_back(std::move(element));
      }
    }
  }
  void extractOrderInfoFromRequestForWebsocket(std::vector<Element>& elementList, const rj::Value& value) {
    Element element;
    this->extractOrderInfo(element, value);
    elementList.emplace_back(std::move(element));
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_TYPE, x[0].GetString());
          element.insert(CCAPI_EM_ASSET, x[1].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x[2].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x[4].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["posSide"].GetString());
          std::string availPos = x["availPos"].GetString();
          std::string positionQuantity = availPos.empty() ? x["pos"].GetString() : availPos;
          element.insert(CCAPI_EM_POSITION_QUANTITY, positionQuantity);
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgPx"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["lever"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  void extractOrderInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_EM_ORDER_ID, x[0].GetString());
    element.insert(CCAPI_EM_CLIENT_ORDER_ID, x[2].GetString());
    element.insert(CCAPI_EM_ORDER_INSTRUMENT, x[3].GetString());
    std::string originalAmount = x[7].GetString();
    if (originalAmount.at(0) == '-') {
      originalAmount.erase(0, 1);
      element.insert(CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_SELL);
      element.insert(CCAPI_EM_ORDER_QUANTITY, originalAmount);
    } else {
      element.insert(CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY);
      element.insert(CCAPI_EM_ORDER_QUANTITY, originalAmount);
    }
    std::string amount = x[6].GetString();
    if (amount.at(0) == '-') {
      amount.erase(0, 1);
      element.insert(CCAPI_EM_ORDER_REMAINING_QUANTITY, amount);
    } else {
      element.insert(CCAPI_EM_ORDER_REMAINING_QUANTITY, amount);
    }
    element.insert(CCAPI_EM_ORDER_REMAINING_QUANTITY, x[6].GetString());
    element.insert(CCAPI_EM_ORDER_STATUS, x[13].GetString());
    element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, x[16].GetString());
    element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                   Decimal(UtilString::printDoubleScientific(std::stod(x[17].GetString()) * (std::stod(originalAmount) - std::stod(amount)))).toString());
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    document.AddMember("event", rj::Value("auth").Move(), allocator);
    document.AddMember("apiKey", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    int64_t nonce = this->generateNonce(now);
    document.AddMember("authNonce", rj::Value(nonce).Move(), allocator);
    std::string authPayload = "AUTH" + std::to_string(nonce);
    document.AddMember("authPayload", rj::Value(authPayload.c_str(), allocator).Move(), allocator);
    std::string authSig = Hmac::hmac(Hmac::ShaVersion::SHA384, apiSecret, authPayload, true);
    document.AddMember("authSig", rj::Value(authSig.c_str(), allocator).Move(), allocator);
    rj::Value filter(rj::kArrayType);
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      if (instrumentSet.empty()) {
        filter.PushBack(rj::Value("trading").Move(), allocator);
      } else {
        for (const auto& instrument : instrumentSet) {
          filter.PushBack(rj::Value(("trading-" + instrument).c_str(), allocator).Move(), allocator);
        }
      }
    }
    document.AddMember("filter", filter, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto subscription = wsConnection.subscriptionList.at(0);
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    Event event = this->createEvent(wsConnection, hdl, subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
#else
  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    std::string textMessage(textMessageView);
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    Event event = this->createEvent(wsConnectionPtr, subscription, textMessageView, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
#endif
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  Event createEvent(const WsConnection& wsConnection, wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage,
                    const rj::Document& document, const TimePoint& timeReceived){
#else
  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    std::string textMessage(textMessageView);
#endif
      Event event;
  std::vector<Message> messageList;
  Message message;
  message.setTimeReceived(timeReceived);
  message.setCorrelationIdList({subscription.getCorrelationId()});
  const auto& fieldSet = subscription.getFieldSet();
  const auto& instrumentSet = subscription.getInstrumentSet();
  if (document.IsArray() && document.Size() >= 3 && std::string(document[0].GetString()) == "0") {
    std::string type = document[1].GetString();
    if ((type == CCAPI_BITFINEX_STREAM_TRADE_RAW_MESSAGE_TYPE) && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = document[2];
      std::string instrument = data[1].GetString();
      if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
        message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(data[2].GetString())));
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
        std::vector<Element> elementList;
        Element element;
        if (type == "tu") {
          element.insert(CCAPI_TRADE_ID, data[2].GetString());
        }
        std::string amount = data[4].GetString();
        if (amount.at(0) == '-') {
          amount.erase(0, 1);
          element.insert(CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, amount);
        } else {
          element.insert(CCAPI_EM_ORDER_SIDE, CCAPI_EM_ORDER_SIDE_BUY);
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, amount);
        }
        element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data[5].GetString());
        element.insert(CCAPI_IS_MAKER, std::string(data[8].GetString()).at(0) != '-' ? "1" : "0");
        element.insert(CCAPI_EM_ORDER_INSTRUMENT, data[1].GetString());
        element.insert(CCAPI_EM_ORDER_ID, data[3].GetString());
        element.insert(CCAPI_EM_CLIENT_ORDER_ID, data[11].GetString());
        if (type == "tu") {
          std::string fee = data[9].GetString();
          if (fee.at(0) == '-') {
            fee.erase(0, 1);
          } else {
            fee.insert(0, 1, '-');
          }
          element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, fee);
          element.insert(CCAPI_EM_ORDER_FEE_ASSET, data[10].GetString());
        }
        elementList.emplace_back(std::move(element));
        message.setElementList(elementList);
        messageList.emplace_back(std::move(message));
      }
    } else if ((type == "on" || type == "ou" || type == "oc") && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = document[2];
      std::string instrument = data[3].GetString();
      if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
        message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(data[5].GetString())));
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
        Element info;
        this->extractOrderInfo(info, data);
        std::vector<Element> elementList;
        elementList.emplace_back(std::move(info));
        message.setElementList(elementList);
        messageList.emplace_back(std::move(message));
      }
    } else if (type == "n") {
      event.setType(Event::Type::RESPONSE);
      const rj::Value& data = document[2];
      std::string status = data[6].GetString();
      if (status != "SUCCESS") {
        message.setType(Message::Type::RESPONSE_ERROR);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      } else {
        std::vector<Element> elementList;
        std::string notificationType = data[1].GetString();
        if (notificationType == "on-req") {
          message.setType(Message::Type::CREATE_ORDER);
        } else if (notificationType == "oc-req") {
          message.setType(Message::Type::CANCEL_ORDER);
        }
        this->extractOrderInfoFromRequestForWebsocket(elementList, data[4]);
        message.setElementList(elementList);
        messageList.emplace_back(std::move(message));
      }
    }
  } else if (document.IsObject()) {
    auto it = document.FindMember("event");
    if (it != document.MemberEnd()) {
      std::string eventStr = it->value.GetString();
      if (eventStr == "auth") {
        std::string status = document["status"].GetString();
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        message.setType(status == "OK" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(status == "OK" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    }
  }
  event.setMessageList(messageList);
  return event;
}
};  // namespace ccapi
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITFINEX_H_
