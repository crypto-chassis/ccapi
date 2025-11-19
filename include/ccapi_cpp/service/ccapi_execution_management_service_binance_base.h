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

  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    if (wsConnectionPtr->host == this->websocketOrderEntryHost) {
      ExecutionManagementService::prepareConnect(wsConnectionPtr);
    } else {
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
                that->jsonDocumentAllocator.Clear();
                rj::Document document(&that->jsonDocumentAllocator);
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
  }

  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    ExecutionManagementService::onOpen(wsConnectionPtr);
    if (wsConnectionPtr->host != this->websocketOrderEntryHost) {
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
    std::map<std::string_view, std::pair<std::string_view, JsonDataType>> extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executedQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, std::make_pair(this->isDerivatives ? "cumQuote" : "cummulativeQuoteQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
        {CCAPI_LAST_UPDATED_TIME_SECONDS, std::make_pair("updateTime", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      extractionFieldNameMap.emplace(CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("origClientOrderId", JsonDataType::STRING));
    } else {
      extractionFieldNameMap.emplace(CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING));
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
            const auto& quantityTotalDecimal = Decimal(x["walletBalance"].GetString());
            if (quantityTotalDecimal != Decimal::zero) {
              Element element;
              element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
              element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(quantityTotalDecimal));
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["availableBalance"].GetString());
              if (this->isDerivatives) {
                element.insert(CCAPI_LAST_UPDATED_TIME_SECONDS, UtilTime::convertMillisecondsStrToSecondsStr(x["updateTime"].GetString()));
              }
              elementList.emplace_back(std::move(element));
            }
          }
        } else {
          if (marginType == CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN) {
            for (const auto& x : document["userAssets"].GetArray()) {
              const auto& quantityTotalDecimal = Decimal(x["free"].GetString()) + Decimal(x["locked"].GetString());
              if (quantityTotalDecimal != Decimal::zero) {
                Element element;
                element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
                element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(quantityTotalDecimal));
                element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
                element.insert(CCAPI_EM_QUANTITY_LIABILITY, ConvertDecimalToString(Decimal(x["borrowed"].GetString()) + (Decimal(x["interest"].GetString()))));
                elementList.emplace_back(std::move(element));
              }
            }
          } else if (marginType == CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN) {
            for (const auto& x : document["assets"].GetArray()) {
              std::string symbol = x["symbol"].GetString();
              {
                const auto& y = x["baseAsset"];
                const auto& quantityTotalDecimal = Decimal(y["free"].GetString()) + Decimal(y["locked"].GetString());
                if (quantityTotalDecimal != Decimal::zero) {
                  Element element;
                  element.insert(CCAPI_EM_INSTRUMENT, symbol);
                  element.insert(CCAPI_EM_ASSET, y["asset"].GetString());
                  element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(quantityTotalDecimal));
                  element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, y["free"].GetString());
                  element.insert(CCAPI_EM_QUANTITY_LIABILITY,
                                 ConvertDecimalToString(Decimal(y["borrowed"].GetString()) + (Decimal(y["interest"].GetString()))));
                  elementList.emplace_back(std::move(element));
                }
              }
              {
                const auto& y = x["quoteAsset"];
                const auto& quantityTotalDecimal = Decimal(y["free"].GetString()) + Decimal(y["locked"].GetString());
                if (quantityTotalDecimal != Decimal::zero) {
                  Element element;
                  element.insert(CCAPI_EM_INSTRUMENT, symbol);
                  element.insert(CCAPI_EM_ASSET, y["asset"].GetString());
                  element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(quantityTotalDecimal));
                  element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, y["free"].GetString());
                  element.insert(CCAPI_EM_QUANTITY_LIABILITY,
                                 ConvertDecimalToString(Decimal(y["borrowed"].GetString()) + (Decimal(y["interest"].GetString()))));
                  elementList.emplace_back(std::move(element));
                }
              }
            }
          } else {
            for (const auto& x : document["balances"].GetArray()) {
              const auto& quantityTotalDecimal = Decimal(x["free"].GetString()) + Decimal(x["locked"].GetString());
              if (quantityTotalDecimal != Decimal::zero) {
                Element element;
                element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
                element.insert(CCAPI_EM_QUANTITY_TOTAL, ConvertDecimalToString(quantityTotalDecimal));
                element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["free"].GetString());
                elementList.emplace_back(std::move(element));
              }
            }
          }
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
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
    if (wsConnectionPtr->host == this->websocketOrderEntryHost) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      std::string id = document["id"].GetString();
      int statusCode = std::stoi(document["status"].GetString());
      bool success = statusCode / 100 == 2;
      if (id == this->websocketOrderEntrySessionLogonJsonId) {
        if (success) {
          event.setType(Event::Type::AUTHORIZATION_STATUS);
          message.setType(Message::Type::AUTHORIZATION_SUCCESS);
          Element element;
          element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
          element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
          element.insert(CCAPI_INFO_MESSAGE, textMessageView);
          message.setElementList({element});
        } else {
          event.setType(Event::Type::AUTHORIZATION_STATUS);
          message.setType(Message::Type::AUTHORIZATION_FAILURE);
          Element element;
          element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
          element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
          element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
          message.setElementList({element});
        }
      } else if (UtilString::startsWith(id, this->websocketOrderEntryCreateOrderJsonIdPrefix) ||
                 UtilString::startsWith(id, this->websocketOrderEntryCancelOrderJsonIdPrefix)) {
        bool isCreateOrder = UtilString::startsWith(id, this->websocketOrderEntryCreateOrderJsonIdPrefix);
        std::string_view wsRequestIdStr;
        if (isCreateOrder) {
          wsRequestIdStr = std::string_view(id).substr(this->websocketOrderEntryCreateOrderJsonIdPrefix.size());
        } else {
          wsRequestIdStr = std::string_view(id).substr(this->websocketOrderEntryCancelOrderJsonIdPrefix.size());
        }
        unsigned long wsRequestId = std::stoul(std::string(wsRequestIdStr));
        const auto& requestCorrelationId = this->requestCorrelationIdByWsRequestIdByConnectionIdMap.at(wsConnectionPtr->id).at(wsRequestId);
        event.setType(Event::Type::RESPONSE);
        if (!success) {
          message.setType(Message::Type::RESPONSE_ERROR);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
          message.setElementList({element});
          message.setCorrelationIdList({requestCorrelationId});
        } else {
          std::vector<Element> elementList;
          if (isCreateOrder) {
            message.setType(Message::Type::CREATE_ORDER);
          } else {
            message.setType(Message::Type::CANCEL_ORDER);
          }
          this->extractOrderInfoFromResponse(elementList, document);
          message.setElementList(elementList);
          message.setCorrelationIdList({requestCorrelationId});
        }
      }
      messageList.emplace_back(std::move(message));
    } else {
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      std::string type = document["e"].GetString();
      if (type == "TRADE_LITE") {
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        const rj::Value& data = document;
        std::string instrument = data["s"].GetString();
        if (instrumentSet.empty() || instrumentSet.find(UtilString::toUpper(instrument)) != instrumentSet.end() ||
            instrumentSet.find(UtilString::toLower(instrument)) != instrumentSet.end()) {
          if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE_LITE) != fieldSet.end()) {
            Message message;
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setTime(TimePoint(std::chrono::milliseconds(std::stoll(data["E"].GetString()))));
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE_LITE);
            std::vector<Element> elementList;
            Element element;
            element.insert(CCAPI_TRADE_ID, data["t"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["L"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["l"].GetString());
            element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(data["S"].GetString()) == "BUY" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            element.insert(CCAPI_IS_MAKER, data["m"].GetBool() ? "1" : "0");
            element.insert(CCAPI_EM_ORDER_ID, data["i"].GetString());
            element.insert(CCAPI_EM_CLIENT_ORDER_ID, data["c"].GetString());
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            elementList.emplace_back(std::move(element));
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      } else if (type == (this->isDerivatives ? "ORDER_TRADE_UPDATE" : "executionReport")) {
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
            element.insert(CCAPI_TRADE_ID, data["t"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, data["L"].GetString());
            element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, data["l"].GetString());
            element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(data["S"].GetString()) == "BUY" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
            element.insert(CCAPI_IS_MAKER, data["m"].GetBool() ? "1" : "0");
            element.insert(CCAPI_EM_ORDER_ID, data["i"].GetString());
            {
              auto it = data.FindMember("C");
              if (it != data.MemberEnd() && !it->value.IsNull() && it->value.GetStringLength()) {
                element.insert(CCAPI_EM_CLIENT_ORDER_ID, std::string(it->value.GetString()));
              } else {
                element.insert(CCAPI_EM_CLIENT_ORDER_ID, data["c"].GetString());
              }
            }
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
            {
              auto it = data.FindMember("n");
              if (it != data.MemberEnd() && !it->value.IsNull()) {
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, it->value.GetString());
              }
            }
            {
              auto it = data.FindMember("N");
              if (it != data.MemberEnd() && !it->value.IsNull()) {
                element.insert(CCAPI_EM_ORDER_FEE_ASSET, it->value.GetString());
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
            const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
                {CCAPI_EM_ORDER_ID, std::make_pair("i", JsonDataType::INTEGER)},
                {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("C", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_SIDE, std::make_pair("S", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("p", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_QUANTITY, std::make_pair("q", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("z", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, std::make_pair("Z", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_STATUS, std::make_pair("X", JsonDataType::STRING)},
                {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("s", JsonDataType::STRING)},
            };
            Element info;
            this->extractOrderInfo(info, data, extractionFieldNameMap);
            if (info.getValue(CCAPI_EM_CLIENT_ORDER_ID).empty()) {
              auto it = data.FindMember("c");
              if (it != data.MemberEnd() && !it->value.IsNull() && it->value.GetStringLength()) {
                info.insert_or_assign(CCAPI_EM_CLIENT_ORDER_ID, std::string(it->value.GetString()));
              }
            }
            {
              auto it = data.FindMember("ap");
              if (it != data.MemberEnd() && !it->value.IsNull()) {
                info.insert(
                    CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY,
                    ConvertDecimalToString(Decimal(UtilString::printDoubleScientific(std::stod(it->value.GetString()) * std::stod(data["z"].GetString())))));
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
    }

    event.setMessageList(messageList);
    return event;
  }

  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, std::shared_ptr<WsConnection> wsConnectionPtr,
                                  const Request& request, unsigned long wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    document.SetObject();
    this->requestCorrelationIdByWsRequestIdByConnectionIdMap[wsConnectionPtr->id][wsRequestId] = request.getCorrelationId();
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER: {
        document.AddMember("id", rj::Value((this->websocketOrderEntryCreateOrderJsonIdPrefix + std::to_string(wsRequestId)).c_str(), allocator).Move(),
                           allocator);
        document.AddMember("method", rj::Value("order.place").Move(), allocator);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value params(rj::kObjectType);
        this->appendParam(params, allocator, param);
        if (!symbolId.empty()) {
          ExecutionManagementService::appendSymbolId(params, allocator, symbolId, "symbol");
        }
        if (param.find("type") == param.end()) {
          params.AddMember("type", "LIMIT", allocator);
          if (param.find("timeInForce") == param.end()) {
            params.AddMember("timeInForce", "GTC", allocator);
          }
        }
        if (param.find("newClientOrderId") == param.end() && param.find(CCAPI_EM_CLIENT_ORDER_ID) == param.end()) {
          std::string nonce = std::to_string(this->generateNonce(now, request.getIndex()));
          params.AddMember(
              "newClientOrderId",
              rj::Value((std::string("x-") + (this->isDerivatives ? CCAPI_BINANCE_USDS_FUTURES_API_LINK_ID : CCAPI_BINANCE_API_LINK_ID) + "-" + nonce).c_str(),
                        allocator)
                  .Move(),
              allocator);
        }
        if (param.find("timestamp") == param.end()) {
          params.AddMember("timestamp", rj::Value(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()).Move(), allocator);
        }
        document.AddMember("params", params, allocator);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        document.AddMember("id", rj::Value((this->websocketOrderEntryCancelOrderJsonIdPrefix + std::to_string(wsRequestId)).c_str(), allocator).Move(),
                           allocator);
        document.AddMember("method", rj::Value("order.cancel").Move(), allocator);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Value params(rj::kObjectType);
        this->appendParam(params, allocator, param,
                          {
                              {CCAPI_EM_ORDER_ID, "orderId"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "origClientOrderId"},
                          });
        if (!symbolId.empty()) {
          ExecutionManagementService::appendSymbolId(params, allocator, symbolId, "symbol");
        }
        if (param.find("timestamp") == param.end()) {
          params.AddMember("timestamp", rj::Value(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()).Move(), allocator);
        }
        document.AddMember("params", params, allocator);
      } break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnectionPtr, request, wsRequestId, now, symbolId, credential);
    }
  }

  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "quantity"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "newClientOrderId"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = value == CCAPI_EM_ORDER_SIDE_BUY ? "BUY" : "SELL";
      }
      if (value != "null") {
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
      }
    }
  }

  virtual void extractOrderInfoFromResponse(std::vector<Element>& elementList, const rj::Document& document) {
    std::map<std::string_view, std::pair<std::string_view, JsonDataType>> extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executedQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, std::make_pair(this->isDerivatives ? "cumQuote" : "cummulativeQuoteQty", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)},
        {CCAPI_LAST_UPDATED_TIME_SECONDS, std::make_pair(this->isDerivatives ? "updateTime" : "transactTime", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("origClientOrderId", JsonDataType::STRING)},
    };

    Element element;
    this->extractOrderInfo(element, document["result"], extractionFieldNameMap,
                           {
                               {CCAPI_LAST_UPDATED_TIME_SECONDS, [](const std::string& input) { return UtilTime::convertMillisecondsStrToSecondsStr(input); }},
                           });
    elementList.emplace_back(std::move(element));
  }

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    if (wsConnectionPtr->host == this->websocketOrderEntryHost) {
      auto it = credential.find(this->websocketOrderEntryApiPrivateKeyPathName);
      if (it == credential.end()) {
        throw std::runtime_error("Missing credential: " + this->websocketOrderEntryApiPrivateKeyPathName);
      }
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();

      document.AddMember("id", rj::Value(this->websocketOrderEntrySessionLogonJsonId.c_str(), allocator).Move(), allocator);
      document.AddMember("method", "session.logon", allocator);

      const auto& apiKey = credential.at(this->websocketOrderEntryApiKeyName);
      const auto& timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      std::map<std::string, std::string> paramsMap{
          {"apiKey", apiKey},
          {"timestamp", std::to_string(timestamp)},
      };

      rj::Value params(rj::kObjectType);
      std::string payload;
      int i = 0;
      for (const auto& [key, value] : paramsMap) {
        if (key == "timestamp") {
          params.AddMember("timestamp", rj::Value().SetInt64(std::stoll(value)), allocator);
        } else {
          params.AddMember("apiKey", rj::Value(value.c_str(), allocator).Move(), allocator);
        }
        payload += key;
        payload += "=";
        payload += value;

        if (i < paramsMap.size() - 1) {
          payload += "&";
        }
        ++i;
      }

      std::string password;
      if (auto it = credential.find(this->websocketOrderEntryApiPrivateKeyPasswordName); it != credential.end()) {
        password = it->second;
      }
      EVP_PKEY* pkey = UtilAlgorithm::loadPrivateKey(UtilAlgorithm::readFile(it->second), password);
      std::string signature = UtilAlgorithm::signPayload(pkey, payload);
      params.AddMember("signature", rj::Value(signature.c_str(), allocator).Move(), allocator);

      document.AddMember("params", params, allocator);

      rj::StringBuffer buffer;
      rj::Writer<rj::StringBuffer> writer(buffer);
      document.Accept(writer);

      return {buffer.GetString()};

    } else {
      return {};
    }
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

  std::string websocketOrderEntryApiKeyName;
  std::string websocketOrderEntryApiPrivateKeyPathName;
  std::string websocketOrderEntryApiPrivateKeyPasswordName;
  std::string websocketOrderEntrySessionLogonJsonId{"session_logon"};
  std::string websocketOrderEntryCreateOrderJsonIdPrefix{"order_place"};
  std::string websocketOrderEntryCancelOrderJsonIdPrefix{"order_cancel"};
  std::string websocketOrderEntryHost;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
