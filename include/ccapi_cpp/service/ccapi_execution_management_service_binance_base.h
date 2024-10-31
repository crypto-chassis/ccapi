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
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void prepareConnect(WsConnection& wsConnection) override {
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    std::string target = this->listenKeyTarget;
    const auto& marginType = wsConnection.subscriptionList.at(0).getMarginType();
    if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
      target = this->listenKeyCrossMarginTarget;
    } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
      target = this->listenKeyIsolatedMarginTarget;
    }
    if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
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
              std::string url = that->baseUrlWs + "/" + listenKey;
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
        this->sessionOptions.httpRequestTimeoutMilliseconds);
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
    message.setCorrelationIdList({wsConnection.subscriptionList.at(0).getCorrelationId()});
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    this->setPingListenKeyTimer(wsConnection);
  }
  void setPingListenKeyTimer(const WsConnection& wsConnection) {
    this->pingListenKeyTimerMapByConnectionIdMap[wsConnection.id] = this->serviceContextPtr->tlsClientPtr->set_timer(
        this->pingListenKeyIntervalSeconds * 1000, [wsConnection, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](ErrorCode const& ec) {
          if (ec) {
            return;
          }
          that->setPingListenKeyTimer(wsConnection);
          http::request<http::string_body> req;
          req.set(http::field::host, that->hostRest);
          req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
          req.method(http::verb::put);
          std::string target = that->listenKeyTarget;
          const auto& marginType = wsConnection.subscriptionList.at(0).getMarginType();
          if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
            target = that->listenKeyCrossMarginTarget;
          } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
            target = that->listenKeyIsolatedMarginTarget;
          }
          if (!that->isDerivatives) {
            std::map<std::string, std::string> params;
            auto listenKey = that->extraPropertyByConnectionIdMap.at(wsConnection.id).at("listenKey");
            params.insert({"listenKey", listenKey});
            if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
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
              that->sessionOptions.httpRequestTimeoutMilliseconds);
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
#else
  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    auto hostPort = this->extractHostFromUrl(this->baseUrlRest);
    std::string host = hostPort.first;
    std::string port = hostPort.second;
    http::request<http::string_body> req;
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.method(http::verb::post);
    std::string target = this->listenKeyTarget;
    const auto& marginType = wsConnectionPtr->subscriptionList.at(0).getMarginType();
    if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
      target = this->listenKeyCrossMarginTarget;
    } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
      target = this->listenKeyIsolatedMarginTarget;
    }
    if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
      auto symbol = wsConnectionPtr->subscriptionList.at(0).getInstrument();
      target += "?" + symbol;
    }
    req.target(target);
    auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-MBX-APIKEY", apiKey);
    this->sendRequest(
        req,
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](const beast::error_code& ec) { that->onFail_(wsConnectionPtr); },
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](const http::response<http::string_body>& res) {
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              std::string listenKey = document["listenKey"].GetString();
              std::string url = that->baseUrlWs + "/" + listenKey;
              wsConnectionPtr->setUrl(url);
              that->connect(wsConnectionPtr);
              that->extraPropertyByConnectionIdMap[wsConnectionPtr->id].insert({
                  {"listenKey", listenKey},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(wsConnectionPtr);
        },
        this->sessionOptions.httpRequestTimeoutMilliseconds);
  }
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    ExecutionManagementService::onOpen(wsConnectionPtr);
    auto now = UtilTime::now();
    Event event;
    event.setType(Event::Type::SUBSCRIPTION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SUBSCRIPTION_STARTED);
    message.setCorrelationIdList({wsConnectionPtr->subscriptionList.at(0).getCorrelationId()});
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    this->setPingListenKeyTimer(wsConnectionPtr);
  }
  void setPingListenKeyTimer(const std::shared_ptr<WsConnection> wsConnectionPtr) {
    TimerPtr timerPtr(
        new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(this->pingListenKeyIntervalSeconds * 1000)));
    timerPtr->async_wait([wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceBinanceBase>()](ErrorCode const& ec) {
      if (ec) {
        return;
      }
      that->setPingListenKeyTimer(wsConnectionPtr);
      http::request<http::string_body> req;
      req.set(http::field::host, that->hostRest);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.method(http::verb::put);
      std::string target = that->listenKeyTarget;
      const auto& marginType = wsConnectionPtr->subscriptionList.at(0).getMarginType();
      if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
        target = that->listenKeyCrossMarginTarget;
      } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
        target = that->listenKeyIsolatedMarginTarget;
      }
      if (!that->isDerivatives) {
        std::map<std::string, std::string> params;
        auto listenKey = that->extraPropertyByConnectionIdMap.at(wsConnectionPtr->id).at("listenKey");
        params.insert({"listenKey", listenKey});
        if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
          auto symbol = wsConnectionPtr->subscriptionList.at(0).getInstrument();
          params.insert({"symbol", symbol});
        }
        target += "?";
        for (const auto& param : params) {
          target += param.first + "=" + Url::urlEncode(param.second);
          target += "&";
        }
      }
      req.target(target);
      auto credential = wsConnectionPtr->subscriptionList.at(0).getCredential();
      if (credential.empty()) {
        credential = that->credentialDefault;
      }
      auto apiKey = mapGetWithDefault(credential, that->apiKeyName);
      req.set("X-MBX-APIKEY", apiKey);
      that->sendRequest(
          req,
          [wsConnectionPtr, that_2 = that->shared_from_base<ExecutionManagementServiceBinanceBase>()](const beast::error_code& ec) {
            CCAPI_LOGGER_ERROR("ping listen key fail");
            that_2->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "ping listen key");
          },
          [wsConnectionPtr, that_2 = that->shared_from_base<ExecutionManagementServiceBinanceBase>()](const http::response<http::string_body>& res) {
            CCAPI_LOGGER_DEBUG("ping listen key success");
          },
          that->sessionOptions.httpRequestTimeoutMilliseconds);
    });
    this->pingListenKeyTimerMapByConnectionIdMap[wsConnectionPtr->id] = timerPtr;
  }
  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    if (this->pingListenKeyTimerMapByConnectionIdMap.find(wsConnectionPtr->id) != this->pingListenKeyTimerMapByConnectionIdMap.end()) {
      this->pingListenKeyTimerMapByConnectionIdMap.at(wsConnectionPtr->id)->cancel();
      this->pingListenKeyTimerMapByConnectionIdMap.erase(wsConnectionPtr->id);
    }
    ExecutionManagementService::onClose(wsConnectionPtr, ec);
  }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    if (queryString.find("timestamp=") == std::string::npos) {
      if (!queryString.empty()) {
        queryString += "&";
      }
      queryString += "timestamp=";
      queryString += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, queryString, true);
    queryString += "&signature=";
    queryString += signature;
  }
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
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
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
        if (param.find("newClientOrderId") == param.end() && param.find(CCAPI_EM_CLIENT_ORDER_ID) == param.end()) {
          std::string nonce = std::to_string(this->generateNonce(now, request.getIndex()));
          queryString += std::string("newClientOrderId=x-") + (this->isDerivatives ? CCAPI_BINANCE_USDS_FUTURES_API_LINK_ID : CCAPI_BINANCE_API_LINK_ID) + "-" +
                         nonce + "&";
        }
        this->signRequest(queryString, param, now, credential);
        req.target((request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN || request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN
                        ? this->createOrderMarginTarget
                        : this->createOrderTarget) +
                   "?" + queryString);
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
        req.target((request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN || request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN
                        ? this->cancelOrderMarginTarget
                        : this->cancelOrderTarget) +
                   "?" + queryString);
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
        req.target((request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN || request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN
                        ? this->getOrderMarginTarget
                        : this->getOrderTarget) +
                   "?" + queryString);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        this->signRequest(queryString, {}, now, credential);
        req.target((request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN || request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN
                        ? this->getOpenOrdersMarginTarget
                        : this->getOpenOrdersTarget) +
                   "?" + queryString);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        std::string queryString;
        this->appendParam(queryString, {});
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, {}, now, credential);
        req.target((request.getMarginType() == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN || request.getMarginType() == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN
                        ? this->cancelOpenOrdersMarginTarget
                        : this->cancelOpenOrdersTarget) +
                   "?" + queryString);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        if (!symbolId.empty()) {
          queryString += "symbols=";
          queryString += Url::urlEncode(symbolId);
          queryString += "&";
        }
        this->signRequest(queryString, {}, now, credential);
        const auto& marginType = request.getMarginType();
        std::string target = this->getAccountBalancesTarget;
        if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
          target = this->getAccountBalancesCrossMarginTarget;
        } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
          target = this->getAccountBalancesIsolatedMarginTarget;
        }
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    std::map<std::string, std::pair<std::string, JsonDataType> > extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executedQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair(this->isDerivatives ? "cumQuote" : "cummulativeQuoteQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
        {CCAPI_LAST_UPDATED_TIME_SECONDS, std::make_pair("updateTime", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      extractionFieldNameMap.insert({CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("origClientOrderId", JsonDataType::STRING)});
    } else {
      extractionFieldNameMap.insert({CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)});
    }
    if (document.IsObject()) {
      Element element;
      this->extractOrderInfo(
          element, document, extractionFieldNameMap,
          {
              {CCAPI_LAST_UPDATED_TIME_SECONDS, [](const std::string& input) { return UtilTime::convertMillisecondsStrToSecondsStr(input); }},
          });
      elementList.emplace_back(std::move(element));
    } else {
      for (const auto& x : document.GetArray()) {
        Element element;
        this->extractOrderInfo(
            element, x, extractionFieldNameMap,
            {
                {CCAPI_LAST_UPDATED_TIME_SECONDS, [](const std::string& input) { return UtilTime::convertMillisecondsStrToSecondsStr(input); }},
            });
        elementList.emplace_back(std::move(element));
      }
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        const auto& marginType = request.getMarginType();
        if (this->isDerivatives) {
          for (const auto& x : document["assets"].GetArray()) {
            Element element;
            element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
            element.insert(CCAPI_EM_QUANTITY_TOTAL, x["walletBalance"].GetString());
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["availableBalance"].GetString());
            if (this->isDerivatives) {
              element.insert(CCAPI_LAST_UPDATED_TIME_SECONDS, UtilTime::convertMillisecondsStrToSecondsStr(x["updateTime"].GetString()));
            }
            elementList.emplace_back(std::move(element));
          }
        } else {
          if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
            for (const auto& x : document["userAssets"].GetArray()) {
              Element element;
              element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
              element.insert(CCAPI_EM_QUANTITY_TOTAL, Decimal(x["free"].GetString()).add(Decimal(x["locked"].GetString())).toString());
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
              element.insert(CCAPI_EM_QUANTITY_LIABILITY, Decimal(x["borrowed"].GetString()).add(Decimal(x["interest"].GetString())).toString());
              elementList.emplace_back(std::move(element));
            }
          } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
            for (const auto& x : document["assets"].GetArray()) {
              std::string symbol = x["symbol"].GetString();
              {
                const auto& y = x["baseAsset"];
                Element element;
                element.insert(CCAPI_EM_INSTRUMENT, symbol);
                element.insert(CCAPI_EM_ASSET, y["asset"].GetString());
                element.insert(CCAPI_EM_QUANTITY_TOTAL, Decimal(y["free"].GetString()).add(Decimal(y["locked"].GetString())).toString());
                element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, y["free"].GetString());
                element.insert(CCAPI_EM_QUANTITY_LIABILITY, Decimal(y["borrowed"].GetString()).add(Decimal(y["interest"].GetString())).toString());
                elementList.emplace_back(std::move(element));
              }
              {
                const auto& y = x["quoteAsset"];
                Element element;
                element.insert(CCAPI_EM_INSTRUMENT, symbol);
                element.insert(CCAPI_EM_ASSET, y["asset"].GetString());
                element.insert(CCAPI_EM_QUANTITY_TOTAL, Decimal(y["free"].GetString()).add(Decimal(y["locked"].GetString())).toString());
                element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, y["free"].GetString());
                element.insert(CCAPI_EM_QUANTITY_LIABILITY, Decimal(y["borrowed"].GetString()).add(Decimal(y["interest"].GetString())).toString());
                elementList.emplace_back(std::move(element));
              }
            }
          } else {
            for (const auto& x : document["balances"].GetArray()) {
              Element element;
              element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
              element.insert(CCAPI_EM_QUANTITY_TOTAL, Decimal(x["free"].GetString()).add(Decimal(x["locked"].GetString())).toString());
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
              elementList.emplace_back(std::move(element));
            }
          }
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
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
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  Event createEvent(const WsConnection& wsConnection, wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage,
                    const rj::Document& document, const TimePoint& timeReceived) {
#else
  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    std::string textMessage(textMessageView);
#endif
    Event event;
    std::vector<Message> messageList;
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    std::string type = document["e"].GetString();
    if (type == (this->isDerivatives ? "ORDER_TRADE_UPDATE" : "executionReport")) {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = this->isDerivatives ? document["o"] : document;
      std::string executionType = data["x"].GetString();
      std::string instrument = data["s"].GetString();
      if (instrumentSet.empty() || instrumentSet.find(UtilString::toUpper(instrument)) != instrumentSet.end() ||
          instrumentSet.find(UtilString::toLower(instrument)) != instrumentSet.end()) {
        if (executionType == "TRADE" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          message.setTime(TimePoint(std::chrono::milliseconds(std::stoll((this->isDerivatives ? document : data)["E"].GetString()))));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, std::string(data["t"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, std::string(data["L"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, std::string(data["l"].GetString()));
          element.insert(CCAPI_EM_ORDER_SIDE, std::string(data["S"].GetString()) == "BUY" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_IS_MAKER, data["m"].GetBool() ? "1" : "0");
          element.insert(CCAPI_EM_ORDER_ID, std::string(data["i"].GetString()));
          element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(data["c"].GetString()));
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
          messageList.emplace_back(std::move(message));
        }
        if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          message.setTime(TimePoint(std::chrono::milliseconds(std::stoll((this->isDerivatives ? document : data)["E"].GetString()))));
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
          {
            auto it = data.FindMember("C");
            if (it != data.MemberEnd() && !it->value.IsNull() && it->value.GetStringLength()) {
              info.insert(CCAPI_EM_ORIGINAL_CLIENT_ORDER_ID, std::string(it->value.GetString()));
            }
          }
          {
            auto it = data.FindMember("ap");
            if (it != data.MemberEnd() && !it->value.IsNull()) {
              info.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY,
                          Decimal(UtilString::printDoubleScientific(std::stod(it->value.GetString()) * std::stod(data["z"].GetString()))).toString());
            }
          }
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    } else if (this->isDerivatives && type == "ACCOUNT_UPDATE") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const rj::Value& data = document["a"];
      if (fieldSet.find(CCAPI_EM_BALANCE_UPDATE) != fieldSet.end() && !data["B"].Empty()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(document["E"].GetString()))));
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
        std::vector<Element> elementList;
        for (const auto& x : data["B"].GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["a"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["wb"].GetString());
          elementList.emplace_back(std::move(element));
        }
        message.setElementList(elementList);
        messageList.emplace_back(std::move(message));
      }
      if (fieldSet.find(CCAPI_EM_POSITION_UPDATE) != fieldSet.end() && !data["P"].Empty()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        message.setTime(TimePoint(std::chrono::milliseconds(std::stoll((this->isDerivatives ? document : data)["E"].GetString()))));
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
        std::vector<Element> elementList;
        for (const auto& x : data["P"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["s"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["ps"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["pa"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["ep"].GetString());
          element.insert(CCAPI_EM_UNREALIZED_PNL, x["up"].GetString());
          elementList.emplace_back(std::move(element));
        }
        message.setElementList(elementList);
        messageList.emplace_back(std::move(message));
      }
    }
    event.setMessageList(messageList);
    return event;
  }
  bool isDerivatives{};
  std::string listenKeyTarget;
  int pingListenKeyIntervalSeconds;
  std::map<std::string, TimerPtr> pingListenKeyTimerMapByConnectionIdMap;
  std::string createOrderMarginTarget;
  std::string cancelOrderMarginTarget;
  std::string getOrderMarginTarget;
  std::string getOpenOrdersMarginTarget;
  std::string cancelOpenOrdersMarginTarget;
  std::string getAccountBalancesCrossMarginTarget;
  std::string getAccountBalancesIsolatedMarginTarget;
  std::string listenKeyCrossMarginTarget;
  std::string listenKeyIsolatedMarginTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
