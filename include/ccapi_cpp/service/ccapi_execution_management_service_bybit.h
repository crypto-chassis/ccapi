#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/service/ccapi_execution_management_service.h"

namespace ccapi {

class ExecutionManagementServiceBybit : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBybit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                  ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BYBIT;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v5/private";
    this->baseUrlWsOrderEntry = sessionConfigs.getUrlWebsocketOrderEntryBase().at(this->exchangeName) + CCAPI_BYBIT_WS_ORDER_ENTRY_PATH;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    // this->setHostWsFromUrlWsOrderEntry(this->baseUrlWsOrderEntry);
    this->apiKeyName = CCAPI_BYBIT_API_KEY;
    this->apiSecretName = CCAPI_BYBIT_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/v5/order/create";
    this->cancelOrderTarget = "/v5/order/cancel";
    this->getOrderTarget = "/v5/order/realtime";
    this->getOpenOrdersTarget = "/v5/order/realtime";
    this->cancelOpenOrdersTarget = "/v5/order/cancel-all";
    this->getAccountBalancesTarget = "/v5/account/wallet-balance";
    this->getAccountPositionsTarget = "/v5/position/list";
  }

  virtual ~ExecutionManagementServiceBybit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override { return bodyView.find(R"("retCode":0)") == std::string::npos; }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, R"({"op":"ping"})", ec); }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = std::string(req.base().at("X-BAPI-TIMESTAMP"));
    preSignedText += apiKey;
    preSignedText += std::string(req.base().at("X-BAPI-RECV-WINDOW"));
    std::string aString;
    if (methodString == "GET") {
      aString = queryString;
    } else if (methodString == "POST") {
      aString = body;
    }
    preSignedText += aString;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("X-BAPI-SIGN", signature);
  }

  void signRequest(http::request<http::string_body>& req, const std::string& aString, const TimePoint& now,
                   const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = std::string(req.base().at("X-BAPI-TIMESTAMP"));
    preSignedText += apiKey;
    preSignedText += std::string(req.base().at("X-BAPI-RECV-WINDOW"));
    preSignedText += aString;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("X-BAPI-SIGN", signature);
  }

  void appendParamToQueryString(std::string& queryString, const std::map<std::string, std::string>& param,
                                const std::map<std::string, std::string> standardizationMap = {
                                    {CCAPI_EM_CLIENT_ORDER_ID, "orderLinkId"},
                                    {CCAPI_EM_ORDER_ID, "orderId"},
                                    {CCAPI_SETTLE_ASSET, "settleCoin"},
                                    {CCAPI_EM_ACCOUNT_TYPE, "accountType"},
                                }) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
  }

  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-BAPI-API-KEY", apiKey);
    req.set("X-BAPI-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    req.set("X-BAPI-RECV-WINDOW", std::to_string(CCAPI_BYBIT_BASE_API_RECEIVE_WINDOW_MILLISECONDS));
    req.set("X-Referer", CCAPI_BYBIT_API_BROKER_ID);
  }

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("auth").Move(), allocator);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto expires = std::chrono::duration_cast<std::chrono::milliseconds>(
                       (now + std::chrono::milliseconds(CCAPI_BYBIT_BASE_API_RECEIVE_WINDOW_MILLISECONDS)).time_since_epoch())
                       .count();
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = "GET";
    preSignedText += "/realtime";
    preSignedText += std::to_string(expires);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    rj::Value args(rj::kArrayType);
    args.PushBack(rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    args.PushBack(rj::Value(expires).Move(), allocator);
    args.PushBack(rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }

  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    Event event = this->createEvent(wsConnectionPtr, subscription, textMessageView, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }

  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "qty"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
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
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
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
        if (param.find("category") == param.end()) {
          document.AddMember("category", rj::Value("spot").Move(), allocator);
        }
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
        if (param.find("category") == param.end()) {
          document.AddMember("category", rj::Value("spot").Move(), allocator);
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
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param);
        if (param.find("category") == param.end()) {
          queryString += "category=spot&";
        }
        req.target(this->getOrderTarget + "?" + queryString);
        this->signRequest(req, queryString, now, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param);
        if (param.find("category") == param.end()) {
          queryString += "category=spot&";
        }
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
        if (param.find("category") == param.end()) {
          document.AddMember("category", rj::Value("spot").Move(), allocator);
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
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParamToQueryString(queryString, param);
        if (param.find("accountType") == param.end()) {
          queryString += "accountType=UNIFIED&";
        }
        req.target(this->getAccountBalancesTarget + "?" + queryString);
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

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const rj::Document& document) {
    const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("orderLinkId", JsonDataType::STRING)},
    };
    const rj::Value& data = document["data"];
    if (data.IsObject()) {
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else {
      for (const auto& x : data.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    }
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    std::map<std::string_view, std::pair<std::string_view, JsonDataType>> extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("orderLinkId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("qty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumExecQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, std::make_pair("cumExecValue", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("orderStatus", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::GET_OPEN_ORDERS || operation == Request::Operation::GET_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
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
        for (const auto& x : document["result"]["list"][0]["coin"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["coin"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["walletBalance"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["equity"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["result"]["list"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["side"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgPrice"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    if (wsConnectionPtr->path == CCAPI_BYBIT_WS_ORDER_ENTRY_PATH) {
      if (document.HasMember("op")) {
        std::string op = document["op"].GetString();
        if (op == "auth") {
          message.setCorrelationIdList({subscription.getCorrelationId()});
          std::string retCode = document["retCode"].GetString();
          if (retCode == "0") {
            event.setType(Event::Type::AUTHORIZATION_STATUS);
            message.setType(Message::Type::AUTHORIZATION_SUCCESS);
            Element element;
            element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
            element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
            element.insert(CCAPI_INFO_MESSAGE, textMessageView);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          } else {
            event.setType(Event::Type::AUTHORIZATION_STATUS);
            message.setType(Message::Type::AUTHORIZATION_FAILURE);
            Element element;
            element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
            element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
            element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          }
        } else if (op == "order.create" || op == "order.cancel") {
          unsigned long wsRequestId = std::stoul(document["reqId"].GetString());
          const auto& requestCorrelationId = this->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(wsConnectionPtr->id).at(wsRequestId);
          event.setType(Event::Type::RESPONSE);
          std::string retCode = document["retCode"].GetString();
          if (retCode != "0") {
            message.setType(Message::Type::RESPONSE_ERROR);
            Element element;
            element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
            message.setElementList({element});
            message.setCorrelationIdList({requestCorrelationId});
            messageList.emplace_back(std::move(message));
          } else {
            std::vector<Element> elementList;
            std::string op = document["op"].GetString();
            if (op == "order.create") {
              message.setType(Message::Type::CREATE_ORDER);
            } else if (op == "order.cancel") {
              message.setType(Message::Type::CANCEL_ORDER);
            }
            this->extractOrderInfoFromRequest(elementList, document);
            message.setElementList(elementList);
            message.setCorrelationIdList({requestCorrelationId});
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else {
      message.setCorrelationIdList({subscription.getCorrelationId()});
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      if (document.HasMember("topic")) {
        std::string topic = document["topic"].GetString();
        if (topic.rfind("order", 0) == 0) {
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          const rj::Value& data = document["data"];
          for (const auto& x : data.GetArray()) {
            std::string instrument = x["symbol"].GetString();
            if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
              auto time = TimePoint(std::chrono::milliseconds(std::stoll(x["updatedTime"].GetString())));
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(time);
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("orderLinkId", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair("qty", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("cumExecQty", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, std::make_pair("cumExecValue", JsonDataType::STRING)},
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
        } else if (topic.rfind("execution", 0) == 0) {
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          const rj::Value& data = document["data"];
          for (const auto& x : data.GetArray()) {
            std::string instrument = x["symbol"].GetString();
            if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
              auto time = TimePoint(std::chrono::milliseconds(std::stoll(x["execTime"].GetString())));
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(time);
              message.setType(topic.rfind("execution.fast", 0) == 0 ? Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE_LITE
                                                                    : Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
              std::vector<Element> elementList;
              Element element;
              element.insert(CCAPI_TRADE_ID, x["execId"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["execPrice"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["execQty"].GetString());
              element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(x["side"].GetString()) == "Buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_IS_MAKER, x["isMaker"].GetBool() ? "1" : "0");
              element.insert(CCAPI_EM_ORDER_ID, x["orderId"].GetString());
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["orderLinkId"].GetString());
              element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              {
                auto it = x.FindMember("execFee");
                if (it != x.MemberEnd() && !it->value.IsNull()) {
                  element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, it->value.GetString());
                }
              }
              elementList.emplace_back(std::move(element));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        } else if (topic == "wallet") {
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          const rj::Value& data = document["data"];
          for (const auto& x : data[0]["coin"].GetArray()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_EM_ASSET, x["coin"].GetString());
            element.insert(CCAPI_EM_QUANTITY_TOTAL, x["walletBalance"].GetString());
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["equity"].GetString());
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        } else if (topic.rfind("position", 0) == 0) {
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          const rj::Value& data = document["data"];
          for (const auto& x : data.GetArray()) {
            std::string instrument = x["symbol"].GetString();
            if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
              auto time = TimePoint(std::chrono::milliseconds(std::stoll(x["updatedTime"].GetString())));
              Message message;
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setTime(time);
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
              std::vector<Element> elementList;
              Element element;
              element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
              element.insert(CCAPI_EM_POSITION_SIDE, x["side"].GetString());
              element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
              element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["entryPrice"].GetString());
              element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
              elementList.emplace_back(std::move(element));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            }
          }
        }
      } else if (document.HasMember("op")) {
        std::string op = document["op"].GetString();
        if (op == "auth") {
          bool success = document["success"].GetBool();
          if (success) {
            event.setType(Event::Type::AUTHORIZATION_STATUS);
            message.setType(Message::Type::AUTHORIZATION_SUCCESS);
            Element element;
            element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
            element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
            element.insert(CCAPI_INFO_MESSAGE, textMessageView);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));

            rj::Document document;
            document.SetObject();
            rj::Document::AllocatorType& allocator = document.GetAllocator();
            document.AddMember("op", rj::Value("subscribe").Move(), allocator);
            rj::Value args(rj::kArrayType);
            const auto& instrumentType = subscription.getInstrumentType();
            if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
              args.PushBack(rj::Value((instrumentType.empty() ? "order" : "order." + instrumentType).c_str(), allocator).Move(), allocator);
            }
            if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
              args.PushBack(rj::Value((instrumentType.empty() ? "execution" : "execution." + instrumentType).c_str(), allocator).Move(), allocator);
            }
            if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE_LITE) != fieldSet.end()) {
              args.PushBack(rj::Value((instrumentType.empty() ? "execution.fast" : "execution.fast." + instrumentType).c_str(), allocator).Move(), allocator);
            }
            if (fieldSet.find(CCAPI_EM_POSITION_UPDATE) != fieldSet.end() && subscription.getInstrumentType() != "spot") {
              args.PushBack(rj::Value((instrumentType.empty() ? "position" : "position." + instrumentType).c_str(), allocator).Move(), allocator);
            }
            if (fieldSet.find(CCAPI_EM_BALANCE_UPDATE) != fieldSet.end()) {
              args.PushBack(rj::Value("wallet").Move(), allocator);
            }
            document.AddMember("args", args, allocator);
            rj::StringBuffer stringBuffer;
            rj::Writer<rj::StringBuffer> writer(stringBuffer);
            document.Accept(writer);
            std::string sendString = stringBuffer.GetString();
            ErrorCode ec;
            this->send(wsConnectionPtr, sendString, ec);
            if (ec) {
              this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
            }
          } else {
            event.setType(Event::Type::AUTHORIZATION_STATUS);
            message.setType(Message::Type::AUTHORIZATION_FAILURE);
            Element element;
            element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
            element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
            element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
          }
        } else if (op == "subscribe") {
          bool success = document["success"].GetBool();
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          message.setType(success ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(success ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessageView);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }

  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, std::shared_ptr<WsConnection> wsConnectionPtr,
                                  const Request& request, unsigned long wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    document.SetObject();
    document.AddMember("reqId", rj::Value(std::to_string(wsRequestId).c_str(), allocator).Move(), allocator);
    this->requestCorrelationIdByWsRequestIdByConnectionIdMap[wsConnectionPtr->id][wsRequestId] = request.getCorrelationId();
    rj::Value header(rj::kObjectType);
    header.AddMember("X-BAPI-TIMESTAMP",
                     rj::Value(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()).c_str(), allocator).Move(),
                     allocator);
    header.AddMember("X-BAPI-RECV-WINDOW", rj::Value(std::to_string(CCAPI_BYBIT_BASE_API_RECEIVE_WINDOW_MILLISECONDS).c_str(), allocator).Move(), allocator);
    header.AddMember("Referer", rj::Value(CCAPI_BYBIT_API_BROKER_ID, allocator), allocator);
    document.AddMember("header", header, allocator);
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER: {
        document.AddMember("op", rj::Value("order.create").Move(), allocator);
        rj::Value args(rj::kArrayType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value arg(rj::kObjectType);
        this->appendParam(arg, allocator, param);
        if (param.find("category") == param.end()) {
          arg.AddMember("category", rj::Value("spot").Move(), allocator);
        }
        if (param.find("orderType") == param.end()) {
          arg.AddMember("orderType", rj::Value("Limit").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(arg, allocator, symbolId, "symbol");
        }
        args.PushBack(arg, allocator);
        document.AddMember("args", args, allocator);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        document.AddMember("op", rj::Value("order.cancel").Move(), allocator);
        rj::Value args(rj::kArrayType);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value arg(rj::kObjectType);
        this->appendParam(arg, allocator, param);
        if (param.find("category") == param.end()) {
          arg.AddMember("category", rj::Value("spot").Move(), allocator);
        }
        if (!symbolId.empty()) {
          this->appendSymbolId(arg, allocator, symbolId, "symbol");
        }
        args.PushBack(arg, allocator);
        document.AddMember("args", args, allocator);
      } break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnectionPtr, request, wsRequestId, now, symbolId, credential);
    }
  }
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_H_
