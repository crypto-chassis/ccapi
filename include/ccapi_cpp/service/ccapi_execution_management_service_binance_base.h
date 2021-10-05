#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_BINANCE_US) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES) || \
    defined(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBinanceBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBinanceBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                        ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->enableCheckPingPongWebsocketApplicationLevel = false;
    this->pingListenKeyIntervalSeconds = 600;
  }
  virtual ~ExecutionManagementServiceBinanceBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void signRequest(std::string& queryString, const std::map<std::string, std::string>& param, const TimePoint& now,
                   const std::map<std::string, std::string>& credential) {
    if (param.find("timestamp") == param.end()) {
      queryString += "timestamp=";
      queryString += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
      queryString += "&";
    }
    if (queryString.back() == '&') {
      queryString.pop_back();
    }
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, queryString, true);
    queryString += "&signature=";
    queryString += signature;
  }
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void prepareReq(http::request<http::string_body>& req, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-MBX-APIKEY", apiKey);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, credential);
    switch (request.getOperation()) {
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_SIDE, "side"},
                              {CCAPI_EM_ORDER_QUANTITY, "quantity"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "newClientOrderId"},
                          });
        this->appendSymbolId(queryString, symbolId);
        if (param.find("type") == param.end()) {
          queryString += "type=LIMIT&";
          if (param.find("timeInForce") == param.end()) {
            queryString += "timeInForce=GTC&";
          }
        }
        this->signRequest(queryString, param, now, credential);
        req.target(std::string(this->createOrderTarget) + "?" + queryString);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderId"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "origClientOrderId"},
                          });
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->cancelOrderTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderId"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "origClientOrderId"},
                          });
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->getOrderTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        this->signRequest(queryString, {}, now, credential);
        req.target(this->getOpenOrdersTarget + "?" + queryString);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        std::string queryString;
        this->appendParam(queryString, {});
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, {}, now, credential);
        req.target(this->cancelOpenOrdersTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        this->signRequest(queryString, {}, now, credential);
        req.target(this->getAccountBalancesTarget + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executedQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair(this->isDerivatives ? "cumQuote" : "cummulativeQuoteQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}};
    std::vector<Element> elementList;
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
    return elementList;
  }
  std::vector<Element> extractAccountInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
    std::vector<Element> elementList;
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        for (const auto& x : document[this->isDerivatives ? "assets" : "balances"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x[this->isDerivatives ? "availableBalance" : "free"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return elementList;
  }
  void prepareConnect(WsConnection& wsConnection) override {
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    std::string target = this->listenKeyTarget;
    if (this->listenKeyTarget == "/sapi/v1/userDataStream/isolated") {
      auto symbol = wsConnection.subscriptionList.at(0).getInstrument();
      target += "?" + symbol;
    }
    req.target(target);
    auto credential = wsConnection.subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-MBX-APIKEY", apiKey);
    this->sendRequest(
        req,
        [wsConnection, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              std::string listenKey = document["listenKey"].GetString();
              std::string url = that->baseUrl + "/" + listenKey;
              thisWsConnection.url = url;
              that->connect(thisWsConnection);
              that->extraPropertyByConnectionIdMap[thisWsConnection.id].insert({
                  {"listenKey", listenKey},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(thisWsConnection);
        },
        this->sessionOptions.httpRequestTimeoutMilliSeconds);
  }
  void onOpen(wspp::connection_hdl hdl) override {
    ExecutionManagementService::onOpen(hdl);
    auto now = UtilTime::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    Event event;
    event.setType(Event::Type::SUBSCRIPTION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SUBSCRIPTION_STARTED);
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    this->pingListenKeyTimerMapByConnectionIdMap[wsConnection.id] = this->serviceContextPtr->tlsClientPtr->set_timer(
        this->pingListenKeyIntervalSeconds * 1000, [wsConnection, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](ErrorCode const& ec) {
          http::request<http::string_body> req;
          req.set(http::field::host, that->hostRest);
          req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
          req.method(http::verb::put);
          std::string target = that->listenKeyTarget;
          if (!that->isDerivatives) {
            std::map<std::string, std::string> params;
            auto listenKey = that->extraPropertyByConnectionIdMap.at(wsConnection.id).at("listenKey");
            params.insert({"listenKey", listenKey});
            if (that->listenKeyTarget == "/sapi/v1/userDataStream/isolated") {
              auto symbol = wsConnection.subscriptionList.at(0).getInstrument();
              params.insert({"symbol", symbol});
            }
            target += "?";
            for (const auto& param : params) {
              target += param.first + "=" + Url::urlEncode(param.second);
              target += "&";
            }
          }
          req.target(target);
          auto credential = wsConnection.subscriptionList.at(0).getCredential();
          if (credential.empty()) {
            credential = that->credentialDefault;
          }
          auto apiKey = mapGetWithDefault(credential, that->apiKeyName);
          req.set("X-MBX-APIKEY", apiKey);
          that->sendRequest(
              req,
              [wsConnection, that_2 = that->shared_from_base<ExecutionManagementServiceBinanceBase>()](const beast::error_code& ec) {
                CCAPI_LOGGER_ERROR("ping listen key fail");
                that_2->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping listen key");
              },
              [wsConnection, that_2 = that->shared_from_base<ExecutionManagementServiceBinanceBase>()](const http::response<http::string_body>& res) {
                CCAPI_LOGGER_DEBUG("ping listen key success");
              },
              that->sessionOptions.httpRequestTimeoutMilliSeconds);
        });
  }
  void onClose(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    if (this->pingListenKeyTimerMapByConnectionIdMap.find(wsConnection.id) != this->pingListenKeyTimerMapByConnectionIdMap.end()) {
      this->pingListenKeyTimerMapByConnectionIdMap.at(wsConnection.id)->cancel();
      this->pingListenKeyTimerMapByConnectionIdMap.erase(wsConnection.id);
    }
    ExecutionManagementService::onClose(hdl);
  }
  void onTextMessage(const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage, const rj::Document& document,
                     const TimePoint& timeReceived) override {
    Event event = this->createEvent(subscription, textMessage, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }
  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    auto fieldSet = subscription.getFieldSet();
    auto instrumentSet = subscription.getInstrumentSet();
    std::string type = document["e"].GetString();
    if (type == (this->isDerivatives ? "ORDER_TRADE_UPDATE" : "executionReport")) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = this->isDerivatives ? document["o"] : document;
      std::string executionType = data["x"].GetString();
      std::string instrument = data["s"].GetString();
      if (instrumentSet.empty() || instrumentSet.find(UtilString::toUpper(instrument)) != instrumentSet.end() ||
          instrumentSet.find(UtilString::toLower(instrument)) != instrumentSet.end()) {
        message.setTime(TimePoint(std::chrono::milliseconds(std::stoll((this->isDerivatives ? document : data)["E"].GetString()))));
        if (executionType == "TRADE" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, std::string(data["t"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(data["L"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(data["l"].GetString()));
          element.insert(CCAPI_EM_ORDER_SIDE, std::string(data["S"].GetString()) == "BUY" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_IS_MAKER, data["m"].GetBool() ? "1" : "0");
          element.insert(CCAPI_EM_ORDER_ID, std::string(data["i"].GetString()));
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
          {
            auto it = data.FindMember("n");
            if (it != data.MemberEnd() && !it->value.IsNull()) {
              element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(it->value.GetString()));
            }
          }
          {
            auto it = data.FindMember("N");
            if (it != data.MemberEnd() && !it->value.IsNull()) {
              element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(it->value.GetString()));
            }
          }
          elementList.emplace_back(std::move(element));
          message.setElementList(elementList);
          messageList.push_back(std::move(message));
        } else if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
              {CCAPI_EM_ORDER_ID, std::make_pair("i", JsonDataType::INTEGER)},
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
          this->extractOrderInfo(info, data, extractionFieldNameMap);
          auto it = data.FindMember("ap");
          if (it != data.MemberEnd() && !it->value.IsNull()) {
            info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                        std::to_string(std::stod(it->value.GetString()) * std::stod(data["z"].GetString())));
          }
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.push_back(std::move(message));
        }
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  bool isDerivatives{};
  std::string listenKeyTarget;
  int pingListenKeyIntervalSeconds;
  std::map<std::string, TimerPtr> pingListenKeyTimerMapByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
