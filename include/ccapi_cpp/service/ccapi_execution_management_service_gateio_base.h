#pragma once

#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_GATEIO) || defined(CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"

namespace ccapi {

class ExecutionManagementServiceGateioBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceGateioBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                       ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}

  virtual ~ExecutionManagementServiceGateioBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr,
               "{\"time\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"channel\":\"" + (this->isDerivatives ? "futures" : "spot") + ".ping\"}",
               ec);
  }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = methodString;
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    preSignedText += queryString;
    preSignedText += "\n";
    preSignedText += UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, body, true);
    preSignedText += "\n";
    preSignedText += std::string(req.base().at("TIMESTAMP"));
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true);
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "SIGN:" + signature;
  }

  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::string& queryString, const std::string& body,
                   const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = std::string(req.method_string());
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    preSignedText += queryString;
    preSignedText += "\n";
    preSignedText += UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, body, true);
    preSignedText += "\n";
    preSignedText += std::string(req.base().at("TIMESTAMP"));
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true);
    req.set("SIGN", signature);
    req.target(queryString.empty() ? path : path + "?" + queryString);
    req.body() = body;
    req.prepare_payload();
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
    queryString += this->symbolName;
    queryString += "=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }

  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      if (value != "null") {
        if (value == "true" || value == "false") {
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
    }
  }

  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param) {
    this->appendParam(document, allocator, param,
                      {
                          {CCAPI_EM_ORDER_SIDE, "side"},
                          {CCAPI_EM_ORDER_QUANTITY, this->amountName},
                          {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                          {CCAPI_EM_CLIENT_ORDER_ID, "text"},
                          {CCAPI_EM_ORDER_TYPE, "type"},
                          {CCAPI_EM_ACCOUNT_TYPE, "account"},
                      });
  }

  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document.AddMember(rj::Value(symbolName.c_str(), allocator).Move(), rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }

  void substituteParamSettle(std::string& target, const std::map<std::string, std::string>& param, const std::string& symbolId) {
    this->substituteParam(target, param,
                          {
                              {"settle", "{settle}"},
                              {CCAPI_SETTLE_ASSET, "{settle}"},
                          });
    std::string settle;
    if (UtilString::endsWith(symbolId, "_USD")) {
      settle = "btc";
    } else if (UtilString::endsWith(symbolId, "_USDT")) {
      settle = "usdt";
    }
    this->substituteParam(target, {
                                      {"{settle}", settle},
                                  });
  }

  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set("X-Gate-Channel-Id", CCAPI_GATEIO_API_CHANNEL_ID);
    req.set("Accept", "application/json");
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("KEY", apiKey);
    req.set("TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()));
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
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param);
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        std::string path = this->createOrderTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, "", body, credential);
      } break;
      case Request::Operation::AMEND_ORDER: {
        req.method(this->isDerivatives ? http::verb::put : http::verb::patch);
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : mapGetWithDefault(param, std::string("order_id"));
        std::string path = this->amendOrderTarget;
        this->substituteParam(path, {{"{order_id}", Url::urlEncode(id)}});
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
          param.erase("settle");
          param.erase(CCAPI_SETTLE_ASSET);
          param.erase(CCAPI_SYMBOL_ID);
          param.erase("contract");
        }
        param.erase(CCAPI_EM_ORDER_ID);
        param.erase(CCAPI_EM_CLIENT_ORDER_ID);
        param.erase("order_id");
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, param,
                          {
                              {CCAPI_EM_ORDER_QUANTITY, this->amountName},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                              {CCAPI_EM_ACCOUNT_TYPE, "account"},
                              {CCAPI_SYMBOL_ID, this->symbolName},
                          });
        if (this->isDerivatives) {
          if (document.HasMember("size")) {
            document["size"].SetInt64(std::stoll(document["size"].GetString()));
          }
        } else if (!symbolId.empty() && !document.HasMember("currency_pair")) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        this->signRequest(req, path, "", stringBuffer.GetString(), credential);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::delete_);
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        std::string path = this->cancelOrderTarget;
        this->substituteParam(path, {
                                        {"{order_id}", id},
                                    });
        param.erase(CCAPI_EM_ORDER_ID);
        param.erase(CCAPI_EM_CLIENT_ORDER_ID);
        param.erase("order_id");
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end()          ? param.at(CCAPI_EM_ORDER_ID)
                         : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                               : "";
        std::string path = this->getOrderTarget;
        this->substituteParam(path, {
                                        {"{order_id}", id},
                                    });
        param.erase(CCAPI_EM_ORDER_ID);
        param.erase(CCAPI_EM_CLIENT_ORDER_ID);
        param.erase("order_id");
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        param["status"] = "open";
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        std::string path = this->getOpenOrdersTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ACCOUNT_ID, "account"},
                          });
        this->appendSymbolId(queryString, symbolId);
        queryString.pop_back();
        std::string path = this->getOpenOrdersTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNTS: {
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ASSET, "currency"},
                          });
        if (!queryString.empty()) {
          queryString.pop_back();
        }
        std::string path = this->getAccountsTarget;
        if (this->isDerivatives) {
          this->substituteParamSettle(path, param, symbolId);
        }
        this->signRequest(req, path, queryString, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, std::shared_ptr<WsConnection> wsConnectionPtr,
                                  const Request& request, unsigned long wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    const auto operation = request.getOperation();
    std::string channel = this->isDerivatives ? "futures." : "spot.";
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
        channel += "order_place";
        break;
      case Request::Operation::AMEND_ORDER:
        channel += "order_amend";
        break;
      case Request::Operation::CANCEL_ORDER:
        channel += "order_cancel";
        break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnectionPtr, request, wsRequestId, now, symbolId, credential);
        return;
    }
    auto param = request.getFirstParamWithDefault();
    if (operation != Request::Operation::CREATE_ORDER) {
      if (param.find(CCAPI_EM_ORDER_ID) != param.end()) {
        param["order_id"] = param.at(CCAPI_EM_ORDER_ID);
      } else if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
        param["order_id"] = param.at(CCAPI_EM_CLIENT_ORDER_ID);
      }
      param.erase(CCAPI_EM_ORDER_ID);
      param.erase(CCAPI_EM_CLIENT_ORDER_ID);
    }
    param.erase("settle");
    param.erase(CCAPI_SETTLE_ASSET);
    rj::Document reqParam;
    reqParam.SetObject();
    this->appendParam(reqParam, allocator, param,
                      {
                          {CCAPI_EM_ORDER_SIDE, "side"},
                          {CCAPI_EM_ORDER_QUANTITY, this->amountName},
                          {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                          {CCAPI_EM_CLIENT_ORDER_ID, "text"},
                          {CCAPI_EM_ORDER_TYPE, "type"},
                          {CCAPI_EM_ACCOUNT_ID, "account"},
                          {CCAPI_EM_ACCOUNT_TYPE, "account"},
                          {CCAPI_SYMBOL_ID, this->symbolName},
                      });
    if (this->isDerivatives) {
      reqParam.RemoveMember("side");
      if (reqParam.HasMember("size")) {
        reqParam["size"].SetInt64(std::stoll(reqParam["size"].GetString()));
      }
      if (operation != Request::Operation::CREATE_ORDER) {
        reqParam.RemoveMember("contract");
      }
    }
    if ((!this->isDerivatives || operation == Request::Operation::CREATE_ORDER) && !symbolId.empty() && !reqParam.HasMember(this->symbolName.c_str())) {
      this->appendSymbolId(reqParam, allocator, symbolId);
    }
    document.SetObject();
    document.AddMember("time", rj::Value().SetInt64(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()), allocator);
    document.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
    document.AddMember("event", "api", allocator);
    rj::Value payload(rj::kObjectType);
    rj::Value reqHeader(rj::kObjectType);
    reqHeader.AddMember("X-Gate-Channel-Id", rj::Value(CCAPI_GATEIO_API_CHANNEL_ID, allocator).Move(), allocator);
    payload.AddMember("req_header", reqHeader, allocator);
    payload.AddMember("req_id", rj::Value(std::to_string(wsRequestId).c_str(), allocator).Move(), allocator);
    payload.AddMember("req_param", reqParam.Move(), allocator);
    document.AddMember("payload", payload, allocator);
    this->requestCorrelationIdByWsRequestIdByConnectionIdMap[wsConnectionPtr->id][wsRequestId] = request.getCorrelationId();
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          float total = std::stof(x["locked"].GetString()) + std::stof(x["available"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, std::to_string(total));
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("text", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair(this->amountName, JsonDataType::STRING)},
        {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("left", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_total", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(this->symbolName, JsonDataType::STRING)},
    };
    if (operation == Request::Operation::GET_OPEN_ORDERS || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
      for (const auto& x : document.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    } else {
      Element element;
      this->extractOrderInfo(element, document, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    }
  }

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    int time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto& fieldSet = subscription.getFieldSet();
    const auto& instrumentSet = subscription.getInstrumentSet();
    if (fieldSet.find(CCAPI_EM_WEBSOCKET_ORDER_ENTRY) != fieldSet.end()) {
      rj::Document document;
      document.SetObject();
      auto& allocator = document.GetAllocator();
      std::string channel = this->isDerivatives ? "futures.login" : "spot.login";
      std::string timestamp = std::to_string(time);
      auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, "api\n" + channel + "\n\n" + timestamp, true);
      document.AddMember("time", rj::Value(time), allocator);
      document.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
      document.AddMember("event", "api", allocator);
      rj::Value payload(rj::kObjectType);
      payload.AddMember("req_id", "login", allocator);
      payload.AddMember("api_key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
      payload.AddMember("signature", rj::Value(signature.c_str(), allocator).Move(), allocator);
      payload.AddMember("timestamp", rj::Value(timestamp.c_str(), allocator).Move(), allocator);
      document.AddMember("payload", payload, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      return {stringBuffer.GetString()};
    }
    for (const auto& field : fieldSet) {
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      document.AddMember("time", rj::Value(time).Move(), allocator);
      std::string channel;
      if (field == CCAPI_EM_PRIVATE_TRADE) {
        channel = this->websocketChannelUserTrades;
      } else if (field == CCAPI_EM_ORDER_UPDATE) {
        channel = this->websocketChannelOrders;
      } else if (field == CCAPI_EM_BALANCE_UPDATE) {
        channel = this->isDerivatives ? "futures.balances" : "spot.balances";
      } else if (field == CCAPI_EM_POSITION_UPDATE && this->isDerivatives) {
        channel = "futures.positions";
      } else {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "Unsupported Gate.io subscription field: " + field,
                      {subscription.getCorrelationId()});
        continue;
      }
      document.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
      document.AddMember("event", rj::Value("subscribe").Move(), allocator);
      if (field != CCAPI_EM_BALANCE_UPDATE || this->isDerivatives) {
        rj::Value payload(rj::kArrayType);
        if (this->isDerivatives && (field == CCAPI_EM_BALANCE_UPDATE || field == CCAPI_EM_POSITION_UPDATE)) {
          auto userId = mapGetWithDefault(subscription.getOptionMap(), std::string(CCAPI_EM_ACCOUNT_ID));
          if (userId.empty()) {
            if (field == CCAPI_EM_BALANCE_UPDATE) {
              this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                            "Gate.io futures balances require the ACCOUNT_ID subscription option", {subscription.getCorrelationId()});
              continue;
            }
            // The positions channel retains the deprecated user ID as a placeholder.
            userId = "0";
          }
          payload.PushBack(rj::Value(userId.c_str(), allocator).Move(), allocator);
        }
        if (field != CCAPI_EM_BALANCE_UPDATE) {
          if (instrumentSet.empty()) {
            payload.PushBack(rj::Value("!all").Move(), allocator);
          } else {
            for (const auto& instrument : instrumentSet) {
              payload.PushBack(rj::Value(instrument.c_str(), allocator).Move(), allocator);
            }
          }
        }
        document.AddMember("payload", payload, allocator);
      }
      rj::Value auth(rj::kObjectType);
      auth.AddMember("method", rj::Value("api_key").Move(), allocator);
      auth.AddMember("KEY", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
      std::string preSignedText = "channel=" + channel + "&event=subscribe&time=" + std::to_string(time);
      auto signature = Hmac::hmac(Hmac::ShaVersion::SHA512, apiSecret, preSignedText, true);
      auth.AddMember("SIGN", rj::Value(signature.c_str(), allocator).Move(), allocator);
      document.AddMember("auth", auth, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      std::string sendString = stringBuffer.GetString();
      sendStringList.push_back(sendString);
    }
    return sendStringList;
  }

  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    if (document.HasParseError() || !document.IsObject()) {
      return;
    }
    Event event = this->createEvent(wsConnectionPtr, subscription, textMessageView, document, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }

  Event createEventForWebsocketApi(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                   boost::beast::string_view textMessageView, const rj::Document& document, const TimePoint& timeReceived) {
    Event event;
    const auto& header = document["header"];
    if (!header.IsObject() || !header.HasMember("channel") || !document.HasMember("data") || !document["data"].IsObject()) {
      return event;
    }
    const auto& data = document["data"];
    const std::string channel = header["channel"].GetString();
    const std::string prefix = this->isDerivatives ? "futures." : "spot.";
    bool hasError =
        (data.HasMember("errs") && !data["errs"].IsNull()) || (header.HasMember("status") && std::string_view(header["status"].GetString()) != "200");
    if (document.HasMember("ack") && document["ack"].IsBool() && document["ack"].GetBool() && !hasError) {
      return event;
    }
    Message message;
    message.setTimeReceived(timeReceived);
    if (channel == prefix + "login") {
      event.setType(Event::Type::AUTHORIZATION_STATUS);
      hasError = hasError || !data.HasMember("result") || !data["result"].IsObject();
      message.setType(hasError ? Message::Type::AUTHORIZATION_FAILURE : Message::Type::AUTHORIZATION_SUCCESS);
      message.setCorrelationIdList({subscription.getCorrelationId()});
      Element element;
      element.insert(CCAPI_CONNECTION_ID, wsConnectionPtr->id);
      element.insert(CCAPI_CONNECTION_URL, wsConnectionPtr->url);
      element.insert(hasError ? CCAPI_ERROR_MESSAGE : CCAPI_INFO_MESSAGE, textMessageView);
      message.setElementList({element});
    } else {
      Request::Operation operation;
      if (channel == prefix + "order_place") {
        operation = Request::Operation::CREATE_ORDER;
      } else if (channel == prefix + "order_amend") {
        operation = Request::Operation::AMEND_ORDER;
      } else if (channel == prefix + "order_cancel") {
        operation = Request::Operation::CANCEL_ORDER;
      } else {
        return event;
      }
      auto connectionIt = this->requestCorrelationIdByWsRequestIdByConnectionIdMap.find(wsConnectionPtr->id);
      if (connectionIt == this->requestCorrelationIdByWsRequestIdByConnectionIdMap.end() || !document.HasMember("request_id")) {
        return event;
      }
      unsigned long wsRequestId;
      try {
        std::size_t pos;
        const std::string requestId = document["request_id"].GetString();
        wsRequestId = std::stoul(requestId, &pos);
        if (pos != requestId.size()) {
          return event;
        }
      } catch (const std::exception&) {
        return event;
      }
      auto requestIt = connectionIt->second.find(wsRequestId);
      if (requestIt == connectionIt->second.end() || requestIt->first != wsRequestId) {
        return event;
      }
      event.setType(Event::Type::RESPONSE);
      message.setCorrelationIdList({requestIt->second});
      if (hasError || !data.HasMember("result") || !data["result"].IsObject()) {
        message.setType(Message::Type::RESPONSE_ERROR);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
        message.setElementList({element});
      } else {
        message.setType(this->requestOperationToMessageTypeMap.at(operation));
        rj::Document result;
        result.CopyFrom(data["result"], result.GetAllocator());
        std::vector<Element> elementList;
        Request request(operation, this->exchangeName);
        this->extractOrderInfoFromRequest(elementList, request, operation, result);
        message.setElementList(elementList);
      }
      connectionIt->second.erase(requestIt);
    }
    event.setMessageList({message});
    return event;
  }

  static std::string getStringOrEmpty(const rj::Value& value, const char* key) {
    auto it = value.FindMember(key);
    return it != value.MemberEnd() && it->value.IsString() ? it->value.GetString() : "";
  }

  static TimePoint getAccountUpdateTime(const rj::Value& value, const rj::Document& document) {
    for (const auto* source : {&value, static_cast<const rj::Value*>(&document)}) {
      for (const auto* key : {"timestamp_ms", "time_ms"}) {
        auto timestamp = getStringOrEmpty(*source, key);
        if (!timestamp.empty()) {
          return UtilTime::makeTimePointMilli(UtilTime::divideMilli(timestamp));
        }
      }
      for (const auto* key : {"timestamp", "time"}) {
        auto timestamp = getStringOrEmpty(*source, key);
        if (!timestamp.empty()) {
          return UtilTime::makeTimePoint(UtilTime::divide(timestamp));
        }
      }
    }
    return TimePoint();
  }

  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    if (document.HasMember("header")) {
      return this->createEventForWebsocketApi(wsConnectionPtr, subscription, textMessageView, document, timeReceived);
    }
    Event event;
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({subscription.getCorrelationId()});
    if (!document.HasMember("event")) {
      return event;
    }
    std::string eventType = document["event"].GetString();
    if (eventType == "update") {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      const auto& fieldSet = subscription.getFieldSet();
      const auto& instrumentSet = subscription.getInstrumentSet();
      std::string channel = document["channel"].GetString();
      std::string field;
      if (channel == this->websocketChannelUserTrades) {
        field = CCAPI_EM_PRIVATE_TRADE;
      } else if (channel == this->websocketChannelOrders) {
        field = CCAPI_EM_ORDER_UPDATE;
      } else if (channel == (this->isDerivatives ? "futures.balances" : "spot.balances")) {
        field = CCAPI_EM_BALANCE_UPDATE;
      } else if (this->isDerivatives && channel == "futures.positions") {
        field = CCAPI_EM_POSITION_UPDATE;
      }
      if (fieldSet.find(field) != fieldSet.end()) {
        for (const auto& x : document["result"].GetArray()) {
          if (field == CCAPI_EM_BALANCE_UPDATE) {
            Message message;
            message.setTime(getAccountUpdateTime(x, document));
            message.setTimeReceived(timeReceived);
            message.setCorrelationIdList({subscription.getCorrelationId()});
            message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE);
            Element element;
            element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
            element.insert(CCAPI_EM_QUANTITY_TOTAL, x[this->isDerivatives ? "balance" : "total"].GetString());
            if (!this->isDerivatives) {
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
            }
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
            continue;
          }
          std::string instrument = x[this->symbolName.c_str()].GetString();
          if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
            if (field == CCAPI_EM_PRIVATE_TRADE) {
              Message message;
              message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["create_time_ms"].GetString())));
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
              std::vector<Element> elementList;
              Element element;
              element.insert(CCAPI_TRADE_ID, x["id"].GetString());
              element.insert(CCAPI_EM_ORDER_ID, x["order_id"].GetString());
              element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["text"].GetString());
              element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(x["side"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["price"].GetString());
              element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x[this->amountName.c_str()].GetString());
              std::string takerSide = x["side"].GetString();
              element.insert(CCAPI_IS_MAKER, std::string_view(x["role"].GetString()) == "maker" ? "1" : "0");
              element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
              elementList.emplace_back(std::move(element));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            } else if (field == CCAPI_EM_ORDER_UPDATE) {
              Message message;
              message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["update_time_ms"].GetString())));
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
              const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
                  {CCAPI_EM_ORDER_ID, std::make_pair("id", JsonDataType::STRING)},
                  {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("text", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_QUANTITY, std::make_pair(this->amountName, JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_REMAINING_QUANTITY, std::make_pair("left", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_total", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_STATUS, std::make_pair("event", JsonDataType::STRING)},
                  {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(this->symbolName, JsonDataType::STRING)},
              };
              Element info;
              this->extractOrderInfo(info, x, extractionFieldNameMap);
              std::vector<Element> elementList;
              elementList.emplace_back(std::move(info));
              message.setElementList(elementList);
              messageList.emplace_back(std::move(message));
            } else if (field == CCAPI_EM_POSITION_UPDATE) {
              Message message;
              message.setTime(getAccountUpdateTime(x, document));
              message.setTimeReceived(timeReceived);
              message.setCorrelationIdList({subscription.getCorrelationId()});
              message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE);
              Element element;
              element.insert(CCAPI_INSTRUMENT, instrument);
              std::string size = x["size"].GetString();
              element.insert(CCAPI_EM_POSITION_QUANTITY, size);
              std::string mode = getStringOrEmpty(x, "mode");
              element.insert(CCAPI_EM_POSITION_SIDE, mode == "dual_long"    ? "long"
                                                     : mode == "dual_short" ? "short"
                                                     : std::stod(size) > 0  ? "long"
                                                     : std::stod(size) < 0  ? "short"
                                                                            : "net");
              auto entryPrice = getStringOrEmpty(x, "entry_price");
              if (!entryPrice.empty()) {
                element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, entryPrice);
              }
              auto leverage = getStringOrEmpty(x, "lever");
              if (leverage.empty()) {
                leverage = getStringOrEmpty(x, "leverage");
              }
              if (!leverage.empty()) {
                element.insert(CCAPI_EM_POSITION_LEVERAGE, leverage);
              }
              auto marginMode = getStringOrEmpty(x, "pos_margin_mode");
              auto legacyLeverage = getStringOrEmpty(x, "leverage");
              if (marginMode == "cross" || marginMode == "isolated" || !legacyLeverage.empty()) {
                bool isCross = marginMode == "cross" || (marginMode.empty() && std::stod(legacyLeverage) == 0);
                element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, isCross ? CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN : CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
              }
              for (const auto& asset : {"btc", "usdt", "usd1"}) {
                if (UtilString::endsWith(wsConnectionPtr->url, std::string("/") + asset)) {
                  element.insert(CCAPI_MARGIN_ASSET, UtilString::toUpper(asset));
                  break;
                }
              }
              message.setElementList({element});
              messageList.emplace_back(std::move(message));
            }
          }
        }
      }
    } else if (eventType == "subscribe") {
      bool hasError = document.HasMember("error") && !document["error"].IsNull();
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(hasError ? Message::Type::SUBSCRIPTION_FAILURE : Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(hasError ? CCAPI_ERROR_MESSAGE : CCAPI_INFO_MESSAGE, textMessageView);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    }
    event.setMessageList(messageList);
    return event;
  }

  bool isDerivatives{};
  std::string symbolName;
  std::string websocketChannelUserTrades;
  std::string websocketChannelOrders;
  std::string amountName;
};

} /* namespace ccapi */
#endif
#endif
