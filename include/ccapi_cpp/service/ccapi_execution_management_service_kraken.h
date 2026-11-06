#pragma once

#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "openssl/evp.h"

namespace ccapi {

class ExecutionManagementServiceKraken : public ExecutionManagementService {
 public:
  ExecutionManagementServiceKraken(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KRAKEN;
    this->baseUrlWs = CCAPI_KRAKEN_URL_WS_BASE_PRIVATE;
    this->baseUrlWsOrderEntry = sessionConfigs.getUrlWebsocketOrderEntryBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_KRAKEN_API_KEY;
    this->apiSecretName = CCAPI_KRAKEN_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/0/private";
    this->createOrderTarget = prefix + "/AddOrder";
    this->amendOrderTarget = prefix + "/AmendOrder";
    this->cancelOrderTarget = prefix + "/CancelOrder";
    this->getOrderTarget = prefix + "/QueryOrders";
    this->getOpenOrdersTarget = prefix + "/OpenOrders";
    this->cancelOpenOrdersTarget = prefix + "/CancelAll";
    this->getAccountBalancesTarget = prefix + "/Balance";
    this->getAccountPositionsTarget = prefix + "/OpenPositions";
    this->getWebSocketsTokenTarget = prefix + "/GetWebSocketsToken";
  }

  virtual ~ExecutionManagementServiceKraken() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif

  // websocket v2 is only used for order entry; the deprecated v1 endpoint is still used for the push channels
  bool isWebsocketV2(std::shared_ptr<WsConnection> wsConnectionPtr) { return UtilString::endsWith(wsConnectionPtr->url, "/v2"); }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    if (this->isWebsocketV2(wsConnectionPtr)) {
      this->send(wsConnectionPtr, "{\"req_id\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"method\":\"ping\"}", ec);
    } else {
      this->send(wsConnectionPtr, "{\"reqid\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"event\":\"ping\"}", ec);
    }
  }

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override { return bodyView.find(R"("error":[])") == std::string::npos; }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto noncePlusBody = std::string(req.base().at("Nonce")) + body;
    auto target = path;
    if (!queryString.empty()) {
      target += queryString;
    }
    std::string preSignedText = target;
    std::string noncePlusBodySha256 = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256, noncePlusBody);
    preSignedText += noncePlusBodySha256;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "API-Sign:" + signature;
  }

  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential,
                   const std::string& nonce) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto noncePlusBody = nonce + body;
    std::string preSignedText = std::string(req.target());
    std::string noncePlusBodySha256 = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256, noncePlusBody);
    preSignedText += noncePlusBodySha256;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA512, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    req.set("API-Sign", signature);
    req.body() = body;
    req.prepare_payload();
  }

  void appendParam(std::string& body, const std::map<std::string, std::string>& param, const std::string& nonce,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "type"},
                       {CCAPI_EM_ORDER_QUANTITY, "volume"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "userref"},
                       {CCAPI_EM_ORDER_TYPE, "ordertype"},
                       {CCAPI_EM_ORDER_LEVERAGE, "leverage"},
                       {CCAPI_EM_ORDER_ID, "txid"},
                   }) {
    body += "nonce=";
    body += nonce;
    body += "&";
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "type") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "buy" : "sell";
      }
      body += Url::urlEncode(key);
      body += "=";
      body += Url::urlEncode(value);
      body += "&";
    }
  }

  void appendSymbolId(std::string& body, const std::string& symbolId) {
    body += "pair=";
    body += Url::urlEncode(symbolId);
    body += "&";
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/x-www-form-urlencoded; charset=utf-8");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("API-Key", apiKey);
    std::string nonce = std::to_string(this->generateNonce(now, request.getIndex()));
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->createOrderTarget);
        std::string body;
        this->appendParam(body, param, nonce);
        if (param.find(CCAPI_EM_ORDER_TYPE) == param.end() && param.find("ordertype") == param.end()) {
          body += "ordertype=limit&";
        }
        this->appendSymbolId(body, symbolId);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::AMEND_ORDER:
      case Request::Operation::AMEND_ORDER_KEEP_PRIORITY: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->amendOrderTarget);
        std::string body;
        this->appendParam(body, param, nonce,
                          {
                              {CCAPI_EM_ORDER_QUANTITY, "order_qty"},
                              {CCAPI_EM_ORDER_LIMIT_PRICE, "limit_price"},
                              {CCAPI_EM_CLIENT_ORDER_ID, "cl_ord_id"},
                              {CCAPI_EM_ORDER_ID, "txid"},
                          });
        this->appendSymbolId(body, symbolId);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOrderTarget);
        std::string body;
        this->appendParam(body, param, nonce,
                          {
                              {CCAPI_EM_CLIENT_ORDER_ID, "txid"},
                              {CCAPI_EM_ORDER_ID, "txid"},
                          });
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOrderTarget);
        std::string body;
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getOpenOrdersTarget);
        std::string body;
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::CANCEL_OPEN_ORDERS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->cancelOpenOrdersTarget);
        std::string body;
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getAccountBalancesTarget);
        std::string body;
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::post);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        req.target(this->getAccountPositionsTarget);
        std::string body;
        this->appendParam(body, param, nonce);
        body.pop_back();
        this->signRequest(req, body, credential, nonce);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("userref", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("vol", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("vol_exec", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
    };
    if (operation == Request::Operation::CREATE_ORDER) {
      for (const auto& x : document["result"]["txid"].GetArray()) {
        Element element;
        element.insert(CCAPI_EM_ORDER_ID, x.GetString());
        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::GET_ORDER || operation == Request::Operation::GET_OPEN_ORDERS) {
      const rj::Value& orders = operation == Request::Operation::GET_ORDER ? document["result"] : document["result"]["open"];
      for (auto itr = orders.MemberBegin(); itr != orders.MemberEnd(); ++itr) {
        Element element;
        this->extractOrderInfo(element, itr->value, extractionFieldNameMap);
        const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionMoreFieldNameMap = {
            {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
            {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
            {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("pair", JsonDataType::STRING)}};
        this->extractOrderInfo(element, itr->value["descr"], extractionMoreFieldNameMap);
        auto it1 = itr->value.FindMember("vol_exec");
        auto it2 = itr->value.FindMember("price");
        if (it1 != itr->value.MemberEnd() && it2 != itr->value.MemberEnd()) {
          element.insert(
              CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY,
              ConvertDecimalToString(Decimal(UtilString::printDoubleScientific(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())))));
        }
        element.insert(CCAPI_EM_ORDER_ID, itr->name.GetString());
        elementList.emplace_back(std::move(element));
      }
    }
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        auto resultItr = document.FindMember("result");
        if (resultItr != document.MemberEnd()) {
          const rj::Value& result = resultItr->value;
          for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
            Element element;
            element.insert(CCAPI_EM_ASSET, itr->name.GetString());
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, itr->value.GetString());
            elementList.emplace_back(std::move(element));
          }
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        auto resultItr = document.FindMember("result");
        if (resultItr != document.MemberEnd()) {
          const rj::Value& result = resultItr->value;
          for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
            Element element;
            element.insert(CCAPI_INSTRUMENT, itr->value["pair"].GetString());
            element.insert(CCAPI_EM_POSITION_QUANTITY,
                           ConvertDecimalToString(Decimal(itr->value["vol"].GetString()) - (Decimal(itr->value["vol_closed"].GetString()))));
            element.insert(CCAPI_EM_POSITION_COST, itr->value["cost"].GetString());
            elementList.emplace_back(std::move(element));
          }
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
    req.set("API-Key", apiKey);
    req.set(beast::http::field::content_type, "application/x-www-form-urlencoded; charset=utf-8");
    std::string body;
    std::string nonce = std::to_string(this->generateNonce(now));
    this->appendParam(body, {}, nonce);
    body.pop_back();
    this->signRequest(req, body, credential, nonce);
    this->sendRequest(
        req, [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceKraken>()](const beast::error_code& ec) { that->onFail_(wsConnectionPtr); },
        [wsConnectionPtr, that = shared_from_base<ExecutionManagementServiceKraken>()](const http::response<http::string_body>& res) {
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            try {
              that->jsonDocumentAllocator.Clear();
              rj::Document document(&that->jsonDocumentAllocator);
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              if (document.HasMember("result") && document["result"].HasMember("token")) {
                std::string token = document["result"]["token"].GetString();
                that->connect(wsConnectionPtr);
                that->extraPropertyByConnectionIdMap[wsConnectionPtr->id].insert({
                    {"token", token},
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
    std::vector<std::string> sendStringList;
    if (this->isWebsocketV2(wsConnectionPtr)) {
      // the websocket v2 connection is dedicated to order entry; push subscriptions are served by the v1 connection
      return sendStringList;
    }
    const auto& fieldSet = subscription.getFieldSetWithoutWsOrderEntry();
    for (const auto& field : fieldSet) {
      std::string name;
      if (field == CCAPI_EM_ORDER_UPDATE) {
        name = "openOrders";
      } else if (field == CCAPI_EM_PRIVATE_TRADE) {
        name = "ownTrades";
      }
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      document.AddMember("event", rj::Value("subscribe").Move(), allocator);
      rj::Value subscription(rj::kObjectType);
      subscription.AddMember("name", rj::Value(name.c_str(), allocator).Move(), allocator);
      subscription.AddMember("token", rj::Value(this->extraPropertyByConnectionIdMap.at(wsConnectionPtr->id).at("token").c_str(), allocator).Move(), allocator);
      document.AddMember("subscription", subscription, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      std::string sendString = stringBuffer.GetString();
      sendStringList.push_back(sendString);
    }
    return sendStringList;
  }

  static bool isNumberString(const std::string& value) {
    if (value.empty()) {
      return false;
    }
    if (value.find_first_not_of("0123456789+-.eE") != std::string::npos) {
      return false;
    }
    try {
      size_t pos = 0;
      auto parsed = std::stod(value, &pos);
      (void)parsed;
      return pos == value.size();
    } catch (const std::exception&) {
      return false;
    }
  }

  static void addNumberOrStringMember(rj::Value& object, rj::Document::AllocatorType& allocator, const std::string& key, const std::string& value) {
    if (isNumberString(value)) {
      object.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(std::stod(value)), allocator);
    } else {
      object.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }

  std::string getToken(std::shared_ptr<WsConnection> wsConnectionPtr) {
    auto it = this->extraPropertyByConnectionIdMap.find(wsConnectionPtr->id);
    return it != this->extraPropertyByConnectionIdMap.end() ? mapGetWithDefault(it->second, std::string("token")) : "";
  }

  void convertRequestForWebsocket(rj::Document& document, rj::Document::AllocatorType& allocator, std::shared_ptr<WsConnection> wsConnectionPtr,
                                  const Request& request, unsigned long wsRequestId, const TimePoint& now, const std::string& symbolId,
                                  const std::map<std::string, std::string>& credential) override {
    if (!this->isWebsocketV2(wsConnectionPtr)) {
      this->convertRequestForWebsocketCustom(document, allocator, wsConnectionPtr, request, wsRequestId, now, symbolId, credential);
      return;
    }
    document.SetObject();
    this->requestCorrelationIdByWsRequestIdByConnectionIdMap[wsConnectionPtr->id][wsRequestId] = request.getCorrelationId();
    this->requestOperationByWsRequestIdByConnectionIdMap[wsConnectionPtr->id][wsRequestId] = request.getOperation();
    Request::Operation operation = request.getOperation();
    const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
    std::map<std::string, std::string> extraParam = param;
    for (const auto& key :
         {std::string(CCAPI_EM_ORDER_ID), std::string(CCAPI_EM_CLIENT_ORDER_ID), std::string(CCAPI_EM_ORDER_SIDE), std::string(CCAPI_EM_ORDER_QUANTITY),
          std::string(CCAPI_EM_ORDER_LIMIT_PRICE), std::string(CCAPI_EM_ORDER_TYPE), std::string("IS_POST_ONLY"), std::string("IS_IOC")}) {
      extraParam.erase(key);
    }
    rj::Value params(rj::kObjectType);
    switch (operation) {
      case Request::Operation::CREATE_ORDER: {
        document.AddMember("method", rj::Value("add_order").Move(), allocator);
        if (param.find(CCAPI_EM_ORDER_TYPE) != param.end()) {
          addNumberOrStringMember(params, allocator, "order_type", param.at(CCAPI_EM_ORDER_TYPE));
        } else if (param.find("order_type") != param.end()) {
          addNumberOrStringMember(params, allocator, "order_type", param.at("order_type"));
        } else {
          params.AddMember("order_type", rj::Value("limit").Move(), allocator);
        }
        if (param.find(CCAPI_EM_ORDER_SIDE) != param.end()) {
          addNumberOrStringMember(params, allocator, "side", UtilString::toLower(param.at(CCAPI_EM_ORDER_SIDE)).rfind("buy", 0) == 0 ? "buy" : "sell");
        }
        addNumberOrStringMember(params, allocator, "symbol", symbolId);
        if (param.find(CCAPI_EM_ORDER_QUANTITY) != param.end()) {
          addNumberOrStringMember(params, allocator, "order_qty", param.at(CCAPI_EM_ORDER_QUANTITY));
        }
        if (param.find(CCAPI_EM_ORDER_LIMIT_PRICE) != param.end()) {
          addNumberOrStringMember(params, allocator, "limit_price", param.at(CCAPI_EM_ORDER_LIMIT_PRICE));
        }
        if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
          addNumberOrStringMember(params, allocator, "cl_ord_id", param.at(CCAPI_EM_CLIENT_ORDER_ID));
        } else if (param.find("cl_ord_id") != param.end()) {
          addNumberOrStringMember(params, allocator, "cl_ord_id", param.at("cl_ord_id"));
        }
        if (param.find("IS_POST_ONLY") != param.end() && UtilString::toLower(param.at("IS_POST_ONLY")) == "true") {
          params.AddMember("post_only", true, allocator);
        }
        if (param.find("IS_IOC") != param.end() && UtilString::toLower(param.at("IS_IOC")) == "true") {
          params.AddMember("time_in_force", rj::Value("ioc").Move(), allocator);
        }
        for (const auto& kv : extraParam) {
          if (params.HasMember(kv.first.c_str())) {
            continue;
          }
          addNumberOrStringMember(params, allocator, kv.first, kv.second);
        }
      } break;
      case Request::Operation::AMEND_ORDER:
      case Request::Operation::AMEND_ORDER_KEEP_PRIORITY: {
        document.AddMember("method", rj::Value("amend_order").Move(), allocator);
        if (param.find(CCAPI_EM_ORDER_ID) != param.end()) {
          addNumberOrStringMember(params, allocator, "order_id", param.at(CCAPI_EM_ORDER_ID));
        }
        if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
          addNumberOrStringMember(params, allocator, "cl_ord_id", param.at(CCAPI_EM_CLIENT_ORDER_ID));
        }
        if (!symbolId.empty()) {
          addNumberOrStringMember(params, allocator, "symbol", symbolId);
        }
        if (param.find(CCAPI_EM_ORDER_QUANTITY) != param.end()) {
          addNumberOrStringMember(params, allocator, "order_qty", param.at(CCAPI_EM_ORDER_QUANTITY));
        }
        if (param.find(CCAPI_EM_ORDER_LIMIT_PRICE) != param.end()) {
          addNumberOrStringMember(params, allocator, "limit_price", param.at(CCAPI_EM_ORDER_LIMIT_PRICE));
        }
        if (param.find("IS_POST_ONLY") != param.end() && UtilString::toLower(param.at("IS_POST_ONLY")) == "true") {
          params.AddMember("post_only", true, allocator);
        }
        for (const auto& kv : extraParam) {
          if (params.HasMember(kv.first.c_str())) {
            continue;
          }
          addNumberOrStringMember(params, allocator, kv.first, kv.second);
        }
      } break;
      case Request::Operation::CANCEL_ORDER: {
        document.AddMember("method", rj::Value("cancel_order").Move(), allocator);
        if (param.find(CCAPI_EM_ORDER_ID) != param.end()) {
          rj::Value orderIdList(rj::kArrayType);
          orderIdList.PushBack(rj::Value(param.at(CCAPI_EM_ORDER_ID).c_str(), allocator).Move(), allocator);
          params.AddMember("order_id", orderIdList, allocator);
        } else if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
          rj::Value clientOrderIdList(rj::kArrayType);
          clientOrderIdList.PushBack(rj::Value(param.at(CCAPI_EM_CLIENT_ORDER_ID).c_str(), allocator).Move(), allocator);
          params.AddMember("cl_ord_id", clientOrderIdList, allocator);
        }
      } break;
      default:
        this->convertRequestForWebsocketCustom(document, allocator, wsConnectionPtr, request, wsRequestId, now, symbolId, credential);
        return;
    }
    params.AddMember("token", rj::Value(this->getToken(wsConnectionPtr).c_str(), allocator).Move(), allocator);
    document.AddMember("params", params, allocator);
    document.AddMember("req_id", rj::Value().SetUint64(wsRequestId), allocator);
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

  Event createEventFromWebsocketV2(const std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const rj::Document& document,
                                   const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;
    auto methodIt = document.FindMember("method");
    std::string method = methodIt != document.MemberEnd() && methodIt->value.IsString() ? methodIt->value.GetString() : "";
    if (method == "add_order" || method == "amend_order" || method == "cancel_order") {
      unsigned long wsRequestId{};
      bool hasWsRequestId{};
      auto reqIdIt = document.FindMember("req_id");
      if (reqIdIt != document.MemberEnd()) {
        try {
          if (reqIdIt->value.IsString()) {
            wsRequestId = std::stoul(reqIdIt->value.GetString());
            hasWsRequestId = true;
          } else if (reqIdIt->value.IsUint64()) {
            wsRequestId = reqIdIt->value.GetUint64();
            hasWsRequestId = true;
          }
        } catch (const std::exception&) {
          hasWsRequestId = false;
        }
      }
      if (!hasWsRequestId) {
        CCAPI_LOGGER_ERROR("received a websocket response without req_id: " + std::string(textMessageView));
        return event;
      }
      const auto& requestInfo = this->eraseWebsocketRequestInfo(wsConnectionPtr->id, wsRequestId);
      if (requestInfo.first.empty()) {
        CCAPI_LOGGER_ERROR("received a websocket response for an unknown req_id: " + std::string(textMessageView));
        return event;
      }
      event.setType(Event::Type::RESPONSE);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({requestInfo.first});
      auto successIt = document.FindMember("success");
      if (successIt == document.MemberEnd() || !successIt->value.IsBool() || !successIt->value.GetBool()) {
        message.setType(Message::Type::RESPONSE_ERROR);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, std::string(textMessageView));
        message.setElementList({element});
      } else {
        if (method == "add_order") {
          message.setType(Message::Type::CREATE_ORDER);
        } else if (method == "amend_order") {
          message.setType(requestInfo.second == Request::Operation::AMEND_ORDER_KEEP_PRIORITY ? Message::Type::AMEND_ORDER_KEEP_PRIORITY
                                                                                              : Message::Type::AMEND_ORDER);
        } else {
          message.setType(Message::Type::CANCEL_ORDER);
        }
        Element element;
        auto resultIt = document.FindMember("result");
        if (resultIt != document.MemberEnd() && resultIt->value.IsObject()) {
          const rj::Value& result = resultIt->value;
          auto orderIdIt = result.FindMember("order_id");
          if (orderIdIt != result.MemberEnd() && orderIdIt->value.IsString()) {
            element.insert(CCAPI_EM_ORDER_ID, orderIdIt->value.GetString());
          }
          auto clientOrderIdIt = result.FindMember("cl_ord_id");
          if (clientOrderIdIt != result.MemberEnd() && clientOrderIdIt->value.IsString()) {
            element.insert(CCAPI_EM_CLIENT_ORDER_ID, clientOrderIdIt->value.GetString());
          }
        }
        message.setElementList({element});
      }
      messageList.emplace_back(std::move(message));
    }
    event.setMessageList(messageList);
    return event;
  }

  Event createEvent(const std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                    const rj::Document& document, const TimePoint& timeReceived) {
    if (this->isWebsocketV2(wsConnectionPtr)) {
      return document.IsObject() ? this->createEventFromWebsocketV2(wsConnectionPtr, textMessageView, document, timeReceived) : Event();
    }
    Event event;
    std::vector<Message> messageList;
    if (document.IsArray() && document.Size() == 3) {
      std::string channelName = document[1].GetString();
      if (channelName == "ownTrades" || channelName == "openOrders") {
        event.setType(Event::Type::SUBSCRIPTION_DATA);
        const auto& fieldSet = subscription.getFieldSet();
        const auto& instrumentSet = subscription.getInstrumentSet();
        if (channelName == "ownTrades" && fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
          for (const auto& x : document[0].GetArray()) {
            for (auto itr = x.MemberBegin(); itr != x.MemberEnd(); ++itr) {
              std::string instrument = itr->value["pair"].GetString();
              if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                Message message;
                message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
                auto timePair = UtilTime::divide(std::string(itr->value["time"].GetString()));
                auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
                tp += std::chrono::nanoseconds(timePair.second);
                message.setTime(tp);
                message.setTimeReceived(timeReceived);
                message.setCorrelationIdList({subscription.getCorrelationId()});
                std::vector<Element> elementList;
                Element element;
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, itr->value["price"].GetString());
                element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, itr->value["vol"].GetString());
                element.insert(CCAPI_EM_ORDER_SIDE,
                               std::string_view(itr->value["type"].GetString()) == "buy" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
                element.insert(CCAPI_EM_ORDER_ID, itr->value["ordertxid"].GetString());
                element.insert(CCAPI_EM_ORDER_INSTRUMENT, instrument);
                element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, itr->value["fee"].GetString());
                elementList.emplace_back(std::move(element));
                message.setElementList(elementList);
                messageList.emplace_back(std::move(message));
              }
            }
          }
        }
        if (channelName == "openOrders" && fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
          Message message;
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          message.setTime(timeReceived);
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          std::vector<Element> elementList;
          const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("userref", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("vol", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("vol_exec", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
          };
          for (const auto& x : document[0].GetArray()) {
            for (auto itr = x.MemberBegin(); itr != x.MemberEnd(); ++itr) {
              if (itr->value.HasMember("descr")) {
                const rj::Value& descr = itr->value["descr"];
                std::string instrument = descr["pair"].GetString();
                if (instrumentSet.empty() || instrumentSet.find(instrument) != instrumentSet.end()) {
                  Element element;
                  this->extractOrderInfo(element, itr->value, extractionFieldNameMap);
                  const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionMoreFieldNameMap = {
                      {CCAPI_EM_ORDER_SIDE, std::make_pair("type", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
                      {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("pair", JsonDataType::STRING)}};
                  this->extractOrderInfo(element, descr, extractionMoreFieldNameMap);
                  auto it1 = itr->value.FindMember("vol_exec");
                  auto it2 = itr->value.FindMember("avg_price");
                  if (it1 != itr->value.MemberEnd() && it2 != itr->value.MemberEnd()) {
                    element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY,
                                   ConvertDecimalToString(
                                       Decimal(UtilString::printDoubleScientific(std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())))));
                  }
                  element.insert(CCAPI_EM_ORDER_ID, itr->name.GetString());
                  elementList.emplace_back(std::move(element));
                }
              } else {
                Element element;
                this->extractOrderInfo(element, itr->value, extractionFieldNameMap);
                auto it1 = itr->value.FindMember("vol_exec");
                auto it2 = itr->value.FindMember("avg_price");
                if (it1 != itr->value.MemberEnd() && it2 != itr->value.MemberEnd()) {
                  element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, ConvertDecimalToString(Decimal(UtilString::printDoubleScientific(
                                                                                      std::stod(it1->value.GetString()) * std::stod(it2->value.GetString())))));
                }
                element.insert(CCAPI_EM_ORDER_ID, itr->name.GetString());
                elementList.emplace_back(std::move(element));
              }
            }
          }
          if (!elementList.empty()) {
            message.setElementList(elementList);
            messageList.emplace_back(std::move(message));
          }
        }
      }
    } else if (document.IsObject() && document.HasMember("event")) {
      std::string eventPayload = std::string(document["event"].GetString());
      if (eventPayload == "heartbeat") {
      } else if (eventPayload == "subscriptionStatus") {
        std::string status = document["status"].GetString();
        if (status == "subscribed" || status == "error") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          message.setType(status == "subscribed" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(status == "subscribed" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessageView);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
        }
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
