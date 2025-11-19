#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITSTAMP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITSTAMP_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/service/ccapi_execution_management_service.h"

namespace ccapi {

class ExecutionManagementServiceBitstamp : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBitstamp(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                     ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITSTAMP;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_BITSTAMP_API_KEY;
    this->apiSecretName = CCAPI_BITSTAMP_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/api/v2";
    this->createOrderTarget = prefix + "/{buy_or_sell}/{order_type}/{currency_pair}/";
    this->cancelOrderTarget = prefix + "/cancel_order/";
    this->getOrderTarget = prefix + "/order_status/";
    this->getOpenOrdersTarget = prefix + "/open_orders/{currency_pair}/";
    this->cancelOpenOrdersTarget = prefix + "/cancel_all_orders/{currency_pair}/";
    this->getAccountBalancesTarget = prefix + "/balance/{currency_pair}/";
    this->getWebSocketsTokenTarget = prefix + "/websockets_token/";
  }

  virtual ~ExecutionManagementServiceBitstamp() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"event": "bts:heartbeat"})", ec);
  }

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override {
    return bodyView.find(R"("status": "error")") != std::string::npos || bodyView.find(R"("status":"error")") != std::string::npos ||
           bodyView.find(R"("error":)") != std::string::npos;
  }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    if (!body.empty()) {
      if (!headerString.empty()) {
        headerString += "\r\n";
      }
      headerString += "Content-Type:application/x-www-form-urlencoded";
    }
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = std::string(req.base().at("X-Auth"));
    preSignedText += methodString;
    preSignedText += std::string(req.base().at(http::field::host));
    preSignedText += std::string(req.target());
    if (!body.empty()) {
      preSignedText += "application/x-www-form-urlencoded";
    }
    preSignedText += std::string(req.base().at("X-Auth-Nonce"));
    preSignedText += std::string(req.base().at("X-Auth-Timestamp"));
    preSignedText += std::string(req.base().at("X-Auth-Version"));
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "X-Auth-Signature:" + signature;
  }

  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    if (!body.empty()) {
      req.set(beast::http::field::content_type, "application/x-www-form-urlencoded");
    }
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = std::string(req.base().at("X-Auth"));
    preSignedText += std::string(req.method_string());
    preSignedText += std::string(req.base().at(http::field::host));
    preSignedText += std::string(req.target());
    if (!body.empty()) {
      preSignedText += std::string(req.base().at(beast::http::field::content_type));
    }
    preSignedText += std::string(req.base().at("X-Auth-Nonce"));
    preSignedText += std::string(req.base().at("X-Auth-Timestamp"));
    preSignedText += std::string(req.base().at("X-Auth-Version"));
    preSignedText += body;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("X-Auth-Signature", signature);
    if (!body.empty()) {
      req.body() = body;
      req.prepare_payload();
    }
  }

  void appendParam(std::string& body, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_QUANTITY, "amount"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "client_order_id"},
                       {CCAPI_EM_ORDER_ID, "id"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key != CCAPI_EM_ORDER_SIDE) {
        body += Url::urlEncode(key);
        body += "=";
        body += Url::urlEncode(value);
        body += "&";
      }
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-Auth", "BITSTAMP " + apiKey);
    std::string nonce = UtilString::generateUuidV4();
    req.set("X-Auth-Nonce", nonce);
    req.set("X-Auth-Timestamp", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    req.set("X-Auth-Version", "v2");
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const std::string& orderSide = UtilString::toLower(param.at(CCAPI_EM_ORDER_SIDE));
        std::string orderType;
        if (param.find(CCAPI_EM_ORDER_TYPE) != param.end()) {
          orderType = param.at(CCAPI_EM_ORDER_TYPE);
        }
        std::string target = this->createOrderTarget;
        UtilString::replaceFirstOccurrence(target, "{buy_or_sell}", orderSide);
        UtilString::replaceFirstOccurrence(target, "{order_type}", orderType);
        UtilString::replaceFirstOccurrence(target, "{currency_pair}", symbolId);
        UtilString::replaceFirstOccurrence(target, "//", "/");
        req.target(target);
        std::string body;
        this->appendParam(body, param);
        body.pop_back();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        std::string body;
        this->appendParam(body, param);
        body.pop_back();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOrderTarget);
        std::string body;
        this->appendParam(body, param);
        body.pop_back();
        this->signRequest(req, body, credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string target = this->getOpenOrdersTarget;
        UtilString::replaceFirstOccurrence(target, "{currency_pair}", symbolId.empty() ? "all" : symbolId);
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string target = this->cancelOpenOrdersTarget;
        UtilString::replaceFirstOccurrence(target, "{currency_pair}", symbolId);
        UtilString::replaceFirstOccurrence(target, "//", "/");
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string target = this->getAccountBalancesTarget;
        UtilString::replaceFirstOccurrence(target, "{currency_pair}", symbolId);
        UtilString::replaceFirstOccurrence(target, "//", "/");
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractOrderInfo(Element& element, const rj::Value& x,
                        const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap,
                        const std::map<std::string_view, std::function<std::string(const std::string&)>> conversionMap = {}) override {
    ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
    {
      auto it1 = x.FindMember("type");
      if (it1 != x.MemberEnd()) {
        element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(it1->value.GetString()) == "0" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
      }
    }
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    std::map<std::string_view, std::pair<std::string_view, JsonDataType>> extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::INTEGER)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("amount", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("amount_remaining", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("currency_pair", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document["canceled"].GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else {
      if (document.IsObject()) {
        Element element;
        this->extractOrderInfo(element, document, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      } else {
        for (const auto& x : document.GetArray()) {
          Element element;
          this->extractOrderInfo(element, x, extractionFieldNameMap);
          elementList.emplace_back(std::move(element));
        }
      }
    }
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        std::map<std::string, std::map<std::string, std::string>> balances;
        for (auto itr = document.MemberBegin(); itr != document.MemberEnd(); ++itr) {
          const auto& splitted = UtilString::split(itr->name.GetString(), '_');
          if (splitted.size() >= 2) {
            const auto& type = splitted.at(1);
            if (type == "available") {
              balances[splitted.at(0)][CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING] = itr->value.GetString();
            } else if (type == "balance") {
              balances[splitted.at(0)][CCAPI_EM_QUANTITY_TOTAL] = itr->value.GetString();
            }
          }
        }
        for (const auto& kv1 : balances) {
          Element element;
          element.insert(CCAPI_EM_ASSET, UtilString::toUpper(kv1.first));
          for (const auto& kv2 : kv1.second) {
            element.insert(kv2.first, kv2.second);
          }
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    auto now = UtilTime::now();
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    std::string target = this->getWebSocketsTokenTarget;
    req.target(target);
    auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-Auth", "BITSTAMP " + apiKey);
    std::string nonce = UtilString::generateUuidV4();
    req.set("X-Auth-Nonce", nonce);
    req.set("X-Auth-Timestamp", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    req.set("X-Auth-Version", "v2");
    std::string body;
    this->signRequest(req, "", credential);
    this->sendRequest(
        req, [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceBitstamp>()](const beast::error_code& ec) { that->onFail_(wsConnectionPtr); },
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceBitstamp>()](const http::response<http::string_body>& res) {
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            try {
              that->jsonDocumentAllocator.Clear();
              rj::Document document(&that->jsonDocumentAllocator);
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              if (document.HasMember("token") && document.HasMember("user_id")) {
                std::string token = document["token"].GetString();
                std::string userId = document["user_id"].GetString();
                wsConnectionPtr->setUrl(that->baseUrlWs);
                that->connect(wsConnectionPtr);
                that->extraPropertyByConnectionIdMap[wsConnectionPtr->id].insert({
                    {"token", token},
                    {"userId", userId},
                });
              }
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(wsConnectionPtr);
        },
        this->sessionOptions.httpRequestTimeoutMilliseconds);
  }

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    std::vector<std::string> sendStringList;
    for (const auto& field : fieldSet) {
      for (const auto& instrument : instrumentSet) {
        std::string name;
        if (field == CCAPI_EM_ORDER_UPDATE) {
          name = "private-my_orders_";
        } else if (field == CCAPI_EM_PRIVATE_TRADE) {
          name = "private-my_trades_";
        }
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("bts:subscribe").Move(), allocator);
        rj::Value data(rj::kObjectType);
        data.AddMember(
            "channel",
            rj::Value((name + instrument + "-" + this->extraPropertyByConnectionIdMap.at(wsConnectionPtr->id).at("userId")).c_str(), allocator).Move(),
            allocator);
        data.AddMember("auth", rj::Value(this->extraPropertyByConnectionIdMap.at(wsConnectionPtr->id).at("token").c_str(), allocator).Move(), allocator);
        document.AddMember("data", data, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
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

  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (document.HasMember("event")) {
      std::string eventPayload = std::string(document["event"].GetString());
      if (eventPayload == "order_created" || eventPayload == "order_changed" || eventPayload == "order_deleted") {
        const rj::Value& data = document["data"];
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        message.setTime(UtilTime::makeTimePoint(UtilTime::divide(microtimestamp)));
        std::string channel = std::string(document["channel"].GetString());
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        std::string instrument = UtilString::split(channel.substr(std::string("private-my_orders_").length()), '-').at(0);
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
            const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_order_id", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("amount", JsonDataType::STRING)},
            };
            Element info;
            this->extractOrderInfo(info, data, extractionFieldNameMap);
            info.insert(CCAPI_EM_ORDER_SIDE, std::string_view(data["order_type"].GetString()) == "0" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            info.insert(CCAPI_EM_ORDER_STATUS, eventPayload);
            info.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      } else if (eventPayload == "trade") {
        const rj::Value& data = document["data"];
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        message.setTime(UtilTime::makeTimePoint(UtilTime::divide(microtimestamp)));
        std::string channel = std::string(document["channel"].GetString());
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        std::string instrument = UtilString::split(channel.substr(std::string("private-my_trades_").length()), '-').at(0);
        if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
          if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
            Element info;
            info.insert(CCAPI_TRADE_ID, data["id"].GetString());
            info.insert(CCAPI_EM_ORDER_ID, data["order_id"].GetString());
            {
              auto it = data.FindMember("client_order_id");
              if (it != data.MemberEnd()) {
                info.insert(CCAPI_EM_CLIENT_ORDER_ID, it->value.GetString());
              }
            }
            info.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["amount"].GetString());
            info.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["price"].GetString());
            info.insert(CCAPI_EM_ORDER_FEE_QUANTITY, data["fee"].GetString());
            info.insert(CCAPI_EM_ORDER_SIDE, std::string_view(data["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            info.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            std::vector<Element> elementList;
            elementList.emplace_back(std::move(info));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      } else if (eventPayload == "bts:subscription_succeeded") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        message.setType(Message::Type::SUBSCRIPTION_STARTED);
        Element element;
        element.insert(CCAPI_INFO_MESSAGE, textMessageView);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
    }
    event.setMessageList(messageList);
    return event;
  }

  std::string getWebSocketsTokenTarget;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITSTAMP_H_
