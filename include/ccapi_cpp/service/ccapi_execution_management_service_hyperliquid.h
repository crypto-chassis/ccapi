#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HYPERLIQUID_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HYPERLIQUID_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HYPERLIQUID
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "ccapi_cpp/crypto/keccak.h"
#include "ccapi_cpp/crypto/secp256k1_ecdsa.h"
#include <cmath>
#include <openssl/bn.h>
#include <msgpack.hpp>
#include <optional>
#include <string_view>
namespace ccapi {
class ExecutionManagementServiceHyperliquid : public ExecutionManagementService {
 public:
  ExecutionManagementServiceHyperliquid(std::function<void(Event& event, Queue<Event>* eventQueue)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr): 
    ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HYPERLIQUID;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->apiWalletAddressName = CCAPI_HYPERLIQUID_API_WALLET_ADDRESS;
    this->apiPrivateKeyName = CCAPI_HYPERLIQUID_API_PRIVATE_KEY;
    this->apiVaultAddressName = CCAPI_HYPERLIQUID_API_VAULT_ADDRESS;
    this->accountAddressName = CCAPI_HYPERLIQUID_ACCOUNT_ADDRESS;
    this->setupCredential({this->apiWalletAddressName, this->apiPrivateKeyName, this->apiVaultAddressName, this->accountAddressName});
    this->createOrderTarget = "/exchange";
    this->cancelOrderTarget = "/exchange";
    this->getOrderTarget = "/info";
    this->getOpenOrdersTarget = "/info";
    this->cancelOpenOrdersTarget = "/exchange";
    this->getAccountBalancesTarget = "/info";
    std::string baseUrlLower = UtilString::toLower(this->baseUrlRest);
    this->isMainnetEnvironment = baseUrlLower.find("test") == std::string::npos;
  }

  virtual ~ExecutionManagementServiceHyperliquid() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
 
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    auto payload = "{\"method\":\"ping\",\"params\":[],\"time\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) + "}";
    this->send(hdl, payload, wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    auto payload = "{\"method\":\"ping\",\"params\":[],\"time\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) + "}";
    this->send(wsConnectionPtr, payload, ec);
  }
#endif

  bool doesHttpBodyContainError(boost::beast::string_view body) override {
    return body.find("error") != boost::beast::string_view::npos;
  }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    // Not implemented for Hyperliquid
  }

  void signRequest(http::request<http::string_body>& req, rj::Document& document, const std::map<std::string, std::string>& credential,
                   const std::optional<std::string>& vaultAddress, const std::optional<uint64_t>& expiresAfter) {
    auto privateKey = mapGetWithDefault(credential, this->apiPrivateKeyName);
    auto nonce = this->generateNonce(UtilTime::now());

    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("nonce", nonce, allocator);

    if (vaultAddress && !vaultAddress->empty()) {
      document.AddMember("vaultAddress", rj::Value(vaultAddress->c_str(), allocator).Move(), allocator);
    }
    if (expiresAfter) {
      document.AddMember("expiresAfter", static_cast<int64_t>(*expiresAfter), allocator);
    }

    auto signatureMap = this->signMessage(privateKey, document["action"], nonce, vaultAddress, expiresAfter, this->isMainnetEnvironment);

    rj::Value signature(rj::kObjectType);
    signature.AddMember("r", rj::Value(signatureMap["r"].c_str(), allocator).Move(), allocator);
    signature.AddMember("s", rj::Value(signatureMap["s"].c_str(), allocator).Move(), allocator);
    signature.AddMember("v", std::stoi(signatureMap["v"]), allocator);

    document.AddMember("signature", signature, allocator);
    
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string body = stringBuffer.GetString();

    req.body() = body;
    req.prepare_payload();
  }

  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                    {CCAPI_EM_ORDER_SIDE, "b"}, 
                    {CCAPI_EM_ORDER_QUANTITY, "s"},
                    {CCAPI_EM_ORDER_LIMIT_PRICE, "p"},
                    {CCAPI_EM_CLIENT_ORDER_ID, "c"}
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "b") {
        bool isBuy = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy");
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), isBuy, allocator);
      } else {
        rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
      }
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        req.set(beast::http::field::content_type, "application/json");
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto vaultAddress = this->resolveVaultAddress(param, credential);
        const auto expiresAfter = this->resolveExpiresAfter(param);

        std::string actionType = "noop";
        auto it = param.find("action");
        if (it != param.end() && !it->second.empty()) {
          actionType = it->second;
        }

        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        rj::Value action(rj::kObjectType);
        action.AddMember("type", rj::Value(actionType.c_str(), allocator).Move(), allocator);
        document.AddMember("action", action, allocator);
        this->signRequest(req, document, credential, vaultAddress, expiresAfter);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.target(this->createOrderTarget);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto vaultAddress = this->resolveVaultAddress(param, credential);
        const auto expiresAfter = this->resolveExpiresAfter(param);
        const auto actionParam = this->filterActionParameters(param);
        req.set(beast::http::field::content_type, "application/json");
        
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        
        rj::Value action(rj::kObjectType);
        rj::Value orders(rj::kArrayType);
        rj::Value order(rj::kObjectType);
        action.AddMember("type", rj::Value("order").Move(), allocator);
        order.AddMember("a", std::stoi(symbolId), allocator);
        this->appendParam(order, allocator, actionParam);

        // Add the 't' field for limit orders
        rj::Value tValue(rj::kObjectType);
        rj::Value limitValue(rj::kObjectType);
        // Default to GTC if not specified
        std::string tif = param.find("timeInForce") != param.end() ? param.at("timeInForce") : "Gtc";
        // Default to not reduceOnly if not specified
        std::string reduceOnly = param.find("reduceOnly") != param.end() ? param.at("reduceOnly") :  "false";
        order.AddMember("r", reduceOnly == "true", allocator);
        
        limitValue.AddMember("tif", rj::Value(tif.c_str(), allocator).Move(), allocator);
        tValue.AddMember("limit", limitValue, allocator);
        order.AddMember("t", tValue, allocator);

        rj::Value sortedOrder(rj::kObjectType);
        const char* orderKeys[] = {"a", "b", "p", "s", "r", "t", "c"};
        for (const char* key : orderKeys) {
          if (order.HasMember(key)) {
            sortedOrder.AddMember(rj::Value(key, allocator).Move(), order[key], allocator);
          }
        }
        
        orders.PushBack(sortedOrder, allocator);
        action.AddMember("orders", orders, allocator);
        action.AddMember("grouping", rj::Value("na").Move(), allocator);
        document.AddMember("action", action, allocator);
        
        this->signRequest(req, document, credential, vaultAddress, expiresAfter);
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.target(this->cancelOrderTarget);
        req.set(beast::http::field::content_type, "application/json");
        
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        const auto vaultAddress = this->resolveVaultAddress(param, credential);
        const auto expiresAfter = this->resolveExpiresAfter(param);
        
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        rj::Value action(rj::kObjectType);
        rj::Value cancels(rj::kArrayType);
        rj::Value cancel(rj::kObjectType);
        if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
          action.AddMember("type", rj::Value("cancelByCloid").Move(), allocator);
          cancel.AddMember("asset", std::stoi(symbolId), allocator);
          cancel.AddMember("cloid", rj::Value(param.at(CCAPI_EM_CLIENT_ORDER_ID).c_str(), allocator).Move(), allocator);
        } else {
          action.AddMember("type", rj::Value("cancel").Move(), allocator);
          cancel.AddMember("a", std::stoi(symbolId), allocator);
          int64_t orderId = std::stoll(param.at(CCAPI_EM_ORDER_ID));
          cancel.AddMember("o", orderId, allocator);
        }
        cancels.PushBack(cancel, allocator);
        action.AddMember("cancels", cancels, allocator);
        document.AddMember("action", action, allocator);
        
        this->signRequest(req, document, credential, vaultAddress, expiresAfter);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::post);
        req.target(this->getOrderTarget);
        req.set(beast::http::field::content_type, "application/json");
        
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        
        document.AddMember("type", rj::Value("orderStatus").Move(), allocator);
        auto accountAddress = this->resolveAccountAddress(param, credential);
        document.AddMember("user", rj::Value(accountAddress.c_str(), allocator).Move(), allocator);
        if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
          document.AddMember("oid", rj::Value(param.at(CCAPI_EM_CLIENT_ORDER_ID).c_str(), allocator).Move(), allocator);
        } else {
          int64_t orderId = std::stoll(param.at(CCAPI_EM_ORDER_ID));
          document.AddMember("oid", orderId, allocator);
        }

        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->getOpenOrdersTarget);
        req.set(beast::http::field::content_type, "application/json");
        const std::map<std::string, std::string> param;  // empty param map for potential overrides
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        
        document.AddMember("type", rj::Value("openOrders").Move(), allocator);
        auto accountAddress = this->resolveAccountAddress(param, credential);
        document.AddMember("user", rj::Value(accountAddress.c_str(), allocator).Move(), allocator);
        
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS:
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        req.target(this->getAccountBalancesTarget);
        req.set(beast::http::field::content_type, "application/json");
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        
        document.AddMember("type", rj::Value("clearinghouseState").Move(), allocator);
        auto accountAddress = this->resolveAccountAddress(param, credential);
        document.AddMember("user", rj::Value(accountAddress.c_str(), allocator).Move(), allocator);
        
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      case Request::Operation::CANCEL_ORDER: {
        this->extractOrderInfoFromCreateOrCancelOrderRequest(elementList, document);
      } break;
      case Request::Operation::GET_ORDER: {
        this->extractOrderInfoFromGetOrderRequest(elementList, document);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        this->extractOrderInfoFromGetOpenOrdersRequest(elementList, document);
      } break;
      default:
        break;
    }
  }

  void extractOrderInfoFromCreateOrCancelOrderRequest(std::vector<Element>& elementList, const rj::Document& document) {
    const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair(std::string_view("oid"), JsonDataType::INTEGER)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair(std::string_view("cloid"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair(std::string_view("side"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair(std::string_view("origSz"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair(std::string_view("limitPx"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair(std::string_view("totalSz"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, std::make_pair(std::string_view("avgPx"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(std::string_view("asset"), JsonDataType::STRING)}};
        // {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
    
    const rj::Value& response = document["response"];
    const std::string& type = response["type"].GetString();
    if (type == "order") {
      const rj::Value& statuses = response["data"]["statuses"];
      
      for (const auto& status : statuses.GetArray()) {
        Element element;
        if (status.HasMember("resting")) {
          const rj::Value& resting = status["resting"];
          this->extractOrderInfo(element, resting, extractionFieldNameMap);
          element.insert(CCAPI_EM_ORDER_STATUS, "resting");
        } else if (status.HasMember("filled")) {
          const rj::Value& filled = status["filled"];
          this->extractOrderInfo(element, filled, extractionFieldNameMap);
          element.insert(CCAPI_EM_ORDER_STATUS, "filled");
        } else if (status.HasMember("error")) {
          element.insert(CCAPI_EM_ORDER_STATUS, "error");
          element.insert(CCAPI_ERROR_MESSAGE, status["error"].GetString());
        }
        elementList.emplace_back(std::move(element));
      }
    } else {
      // Handle other response types if necessary
    }  
  }

  void extractOrderInfoFromGetOpenOrdersRequest(std::vector<Element>& elementList, const rj::Document& document) {
    const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair(std::string_view("oid"), JsonDataType::INTEGER)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair(std::string_view("cloid"), JsonDataType::STRING)},
        // {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair(std::string_view("origSz"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair(std::string_view("limitPx"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair(std::string_view("totalSz"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, std::make_pair(std::string_view("avgPx"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(std::string_view("coin"), JsonDataType::STRING)}};

    for (const auto& order : document.GetArray()) {
      Element element;
      this->extractOrderInfo(element, order, extractionFieldNameMap);
      const char* sideStr = order["side"].GetString();
      bool isBuy = (std::strcmp(sideStr, "B") == 0);
      element.insert(CCAPI_EM_ORDER_SIDE, isBuy ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
      elementList.emplace_back(std::move(element));
    }
  }

  void extractOrderInfoFromGetOrderRequest(std::vector<Element>& elementList, const rj::Document& document) {
    if (!document.HasMember("order") || !document["order"].IsObject()) {
      return;
    }
    const auto& orderWrapper = document["order"];
    if (!orderWrapper.HasMember("order") || !orderWrapper["order"].IsObject()) {
      return;
    }
    const auto& order = orderWrapper["order"];
    const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair(std::string_view("oid"), JsonDataType::INTEGER)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair(std::string_view("cloid"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair(std::string_view("origSz"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair(std::string_view("limitPx"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair(std::string_view("totalSz"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, std::make_pair(std::string_view("avgPx"), JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(std::string_view("coin"), JsonDataType::STRING)}};
    Element element;
    this->extractOrderInfo(element, order, extractionFieldNameMap);
    if (order.HasMember("side")) {
      bool isBuy = std::strcmp(order["side"].GetString(), "B") == 0;
      element.insert(CCAPI_EM_ORDER_SIDE, isBuy ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
    }
    if (orderWrapper.HasMember("status") && orderWrapper["status"].IsString()) {
      element.insert(CCAPI_EM_ORDER_STATUS, orderWrapper["status"].GetString());
    }
    elementList.emplace_back(std::move(element));
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (operation) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        if (document.HasMember("marginSummary")) {
          const rj::Value& marginSummary = document["marginSummary"];
          Element element;
          element.insert(CCAPI_EM_ASSET, "USDC");
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, marginSummary["accountValue"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, marginSummary["totalRawUsd"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        if (document.HasMember("assetPositions")) {
          const rj::Value& assetPositions = document["assetPositions"];
          for (const auto& position : assetPositions.GetArray()) {
            Element element;
            const auto& pos = position["position"];
            if (pos.HasMember("coin")) {
              element.insert(CCAPI_EM_POSITION_ASSET, pos["coin"].GetString());
            }
            if (pos.HasMember("szi")) {
              std::string sziStr = pos["szi"].IsString() ? pos["szi"].GetString() : std::to_string(pos["szi"].GetDouble());
              double szi = std::stod(sziStr);
              element.insert(CCAPI_EM_POSITION_QUANTITY, UtilString::normalizeDecimalString(sziStr));
              element.insert(CCAPI_EM_POSITION_SIDE, szi >= 0 ? "LONG" : "SHORT");
            }
            if (pos.HasMember("positionValue")) {
              element.insert(CCAPI_EM_POSITION_COST, pos["positionValue"].GetString());
            }
            if (pos.HasMember("entryPx") && !pos["entryPx"].IsNull()) {
              element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, pos["entryPx"].GetString());
            }
            if (pos.HasMember("leverage") && pos["leverage"].IsObject()) {
              const auto& leverage = pos["leverage"];
              if (leverage.HasMember("value")) {
                std::string leverageValue =
                    leverage["value"].IsString() ? leverage["value"].GetString() : std::to_string(leverage["value"].GetDouble());
                element.insert(CCAPI_EM_POSITION_LEVERAGE, UtilString::normalizeDecimalString(leverageValue));
              }
              if (leverage.HasMember("type")) {
                auto type = UtilString::toLower(leverage["type"].GetString());
                if (type == "cross") {
                  element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN);
                } else if (type == "isolated") {
                  element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
                } else {
                  element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, leverage["type"].GetString());
                }
              }
            }
            elementList.emplace_back(std::move(element));
          }
        }
      } break;
      default:
        break;
    }
  }

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();

    document.AddMember("method", rj::Value("subscribe").Move(), allocator);

    rj::Value subscribe(rj::kObjectType);

    auto fieldSet = subscription.getFieldSet();
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
      subscribe.AddMember("type", rj::Value("orderUpdates").Move(), allocator);
    } else if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      subscribe.AddMember("type", rj::Value("userFills").Move(), allocator);
    }
    auto accountAddress = this->resolveAccountAddress({}, credential);
    subscribe.AddMember("user", rj::Value(accountAddress.c_str(), allocator).Move(), allocator);

    document.AddMember("subscription", subscribe, allocator);

    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();

    std::vector<std::string> sendStringList;
    sendStringList.push_back(sendString);
    return sendStringList;
  }

  void onTextMessage(
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      const WsConnection& wsConnection, const Subscription& subscription, const std::string& textMessage
#else
      std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView
#endif
      ,
      const TimePoint& timeReceived) override {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
    std::string textMessage(textMessageView);
#endif
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    auto channel = std::string(document["channel"].GetString());
    Event event = this->createEvent(subscription, textMessage, document, channel, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }

  Event createEvent(const Subscription& subscription, const std::string& textMessage, const rj::Document& document, const std::string& channel, const TimePoint& timeReceived) {
    Event event;
    std::vector<Message> messageList;

    if (channel == "orderUpdates") {
      const rj::Value& data = document["data"];
      for (const auto& order : data.GetArray()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
        message.setCorrelationIdList({subscription.getCorrelationId()});
        std::vector<Element> elementList;
        const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
            {CCAPI_EM_ORDER_ID, std::make_pair(std::string_view("oid"), JsonDataType::INTEGER)},
            {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair(std::string_view("cloid"), JsonDataType::STRING)},
            {CCAPI_EM_ORDER_QUANTITY, std::make_pair(std::string_view("origSz"), JsonDataType::STRING)},
            {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair(std::string_view("limitPx"), JsonDataType::STRING)},
            {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair(std::string_view("sz"), JsonDataType::STRING)},
            {CCAPI_EM_ORDER_STATUS, std::make_pair(std::string_view("state"), JsonDataType::STRING)},
            {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair(std::string_view("coin"), JsonDataType::STRING)}};
        Element element;
        this->extractOrderInfo(element, order["order"], extractionFieldNameMap);
        element.insert(CCAPI_EM_ORDER_STATUS, std::string(order["status"].GetString()));
        element.insert(CCAPI_EM_ORDER_SIDE, std::strcmp(order["order"]["side"].GetString(), "B") == 0 ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
        elementList.emplace_back(std::move(element));
        message.setElementList(elementList);
        messageList.emplace_back(std::move(message));
      }
    } else if (channel == "userFills") {
      const rj::Value& data = document["data"];
      if (!(data.HasMember("isSnapshot") && data["isSnapshot"].GetBool())) {
        for (const auto& fill : data["fills"].GetArray()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, std::string(fill["tid"].GetString()));
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, fill["px"].GetString());
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, fill["sz"].GetString());
          element.insert(CCAPI_EM_ORDER_SIDE, std::strcmp(fill["side"].GetString(), "B") == 0 ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_EM_POSITION_SIDE, fill["dir"].GetString());
          element.insert(CCAPI_IS_MAKER, fill["crossed"].GetBool() ? "0" : "1");
          element.insert(CCAPI_EM_ORDER_ID, std::string(fill["oid"].GetString()));
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, std::string(fill["coin"].GetString()));
          element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, std::string(fill["fee"].GetString()));
          element.insert(CCAPI_EM_ORDER_FEE_ASSET, std::string(fill["feeToken"].GetString()));
          elementList.emplace_back(std::move(element));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    } else if (channel == "post") {
      const rj::Value& data = document["data"];
      if (data.IsObject() && data.HasMember("response")) {
        const rj::Value& response = data["response"];
        std::string type = response["type"].GetString();
        if (type == "error") {
          std::string errorMessage = response["payload"].GetString();
          CCAPI_LOGGER_ERROR("Received error message: " + errorMessage);
          Event event;
          event.setType(Event::Type::RESPONSE);
          Message message;
          message.setType(Message::Type::RESPONSE_ERROR);
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
        }
      }
    }

    event.setType(Event::Type::SUBSCRIPTION_DATA);
    event.addMessages(messageList);
    return event;
  }

  int64_t generateNonce(const TimePoint& now) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  std::string toHex(const uint8_t* data, size_t len) {
      std::string input(reinterpret_cast<const char*>(data), len);
      std::string hexString = UtilAlgorithm::stringToHex(input);
      return "0x" + hexString;
  }

  void convertToMsgpack(const rj::Value& value, msgpack::packer<msgpack::sbuffer>& packer) {
      switch (value.GetType()) {
          case rj::kNullType: packer.pack_nil(); break;
          case rj::kFalseType: packer.pack_false(); break;
          case rj::kTrueType: packer.pack_true(); break;
          case rj::kObjectType:
              packer.pack_map(value.MemberCount());
              for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) {
                  packer.pack(it->name.GetString());
                  convertToMsgpack(it->value, packer);
              }
              break;
          case rj::kArrayType:
              packer.pack_array(value.Size());
              for (auto it = value.Begin(); it != value.End(); ++it) {
                  convertToMsgpack(*it, packer);
              }
              break;
          case rj::kStringType: packer.pack(value.GetString()); break;
          case rj::kNumberType:
              if (value.IsInt()) packer.pack(value.GetInt());
              else if (value.IsUint()) packer.pack(value.GetUint());
              else if (value.IsInt64()) packer.pack(value.GetInt64());
              else if (value.IsUint64()) packer.pack(value.GetUint64());
              else if (value.IsDouble()) packer.pack(value.GetDouble());
              break;
      }
  }

  std::vector<uint8_t> calculateConnectionId(const rj::Value& action, uint64_t nonce, const std::optional<std::string>& vaultAddress,
                                             const std::optional<uint64_t>& expiresAfter) {
      msgpack::sbuffer sbuf;
      msgpack::packer<msgpack::sbuffer> packer(&sbuf);
      convertToMsgpack(action, packer);
      std::vector<uint8_t> data(sbuf.data(), sbuf.data() + sbuf.size());
      for (int i = 7; i >= 0; --i) {
          data.push_back((nonce >> (i * 8)) & 0xFF);
      }
      if (vaultAddress && !vaultAddress->empty()) {
          data.push_back(0x01);
          std::string normalized = *vaultAddress;
          if (normalized.rfind("0x", 0) == 0) {
              normalized = normalized.substr(2);
          }
          std::string addressBytes = UtilAlgorithm::hexToString(normalized);
          if (addressBytes.size() != 20) {
              throw std::runtime_error("Hyperliquid vault address must represent 20 bytes");
          }
          data.insert(data.end(), addressBytes.begin(), addressBytes.end());
      } else {
          data.push_back(0x00);
      }
      if (expiresAfter) {
          data.push_back(0x00);
          for (int i = 7; i >= 0; --i) {
              data.push_back((*expiresAfter >> (i * 8)) & 0xFF);
          }
      }
      std::vector<uint8_t> connection_id(32);
      auto hash = ethash_keccak256(data.data(), data.size());
      std::memcpy(connection_id.data(), hash.word64s, 32);
      return connection_id;
  }

  std::string encodeType(const std::string& primary_type, const std::map<std::string, std::vector<std::map<std::string, std::string>>>& types) {
      std::string encoded = primary_type + "(";
      for (const auto& field : types.at(primary_type)) {
          if (&field != &types.at(primary_type)[0]) encoded += ",";
          encoded += field.at("type") + " " + field.at("name");
      }
      encoded += ")";
      return encoded;
  }

  std::vector<uint8_t> hashType(const std::string& primary_type, const std::map<std::string, std::vector<std::map<std::string, std::string>>>& types) {
      std::string encoded_type = encodeType(primary_type, types);
      std::vector<uint8_t> type_hash(32);
      auto hash = ethash_keccak256((const uint8_t*)encoded_type.c_str(), encoded_type.length());
      std::memcpy(type_hash.data(), hash.word64s, 32);
      return type_hash;
  }

  std::vector<uint8_t> encodeData(const std::string& primary_type, 
                                   const std::map<std::string, std::vector<std::map<std::string, std::string>>>& types, 
                                   const std::map<std::string, std::string>& data) {
      std::vector<uint8_t> encoded;
      std::vector<uint8_t> type_hash = hashType(primary_type, types);
      encoded.insert(encoded.end(), type_hash.begin(), type_hash.end());

      for (const auto& field : types.at(primary_type)) {
          std::string name = field.at("name");
          std::string type = field.at("type");
          std::string value = data.at(name);
          std::vector<uint8_t> field_value(32, 0);

          if (type == "string") {
              auto hash = ethash_keccak256((const uint8_t*)value.c_str(), value.length());
              std::memcpy(field_value.data(), hash.word64s, 32);
          } else if (type == "uint256") {
              BIGNUM* bn = BN_new();
              BN_dec2bn(&bn, value.c_str());
              BN_bn2binpad(bn, field_value.data(), 32);
              BN_free(bn);
          } else if (type == "address") {
              std::string address = value.substr(0, 2) == "0x" ? value.substr(2) : value;
              for (size_t i = 0; i < 20; ++i) {
                  field_value[12 + i] = std::stoi(address.substr(i * 2, 2), nullptr, 16);
              }
          } else if (type == "bytes32") {
              std::copy(value.begin(), value.end(), field_value.begin());
          }
          encoded.insert(encoded.end(), field_value.begin(), field_value.end());
      }
      return encoded;
  }

  std::vector<uint8_t> hashData(const std::vector<uint8_t>& data) {
      std::vector<uint8_t> hashed(32);
      auto hash = ethash_keccak256(data.data(), data.size());
      std::memcpy(hashed.data(), hash.word64s, 32);
      return hashed;
  }

  std::string vectorToPythonByteString(const std::vector<uint8_t>& v) {
      std::stringstream ss;
      ss << "b'";
      for (const auto& byte : v) {
          if (std::isprint(byte) && byte != '\\' && byte != '\'') {
              ss << static_cast<char>(byte);
          } else {
              ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
          }
      }
      ss << "'";
      return ss.str();
  }

  std::map<std::string, std::string> signMessage(const std::string& private_key_hex, const rj::Value& action, uint64_t nonce,
                                                 const std::optional<std::string>& vaultAddress, const std::optional<uint64_t>& expiresAfter,
                                                 bool isMainnet) {
      if (private_key_hex.empty()) {
        throw std::runtime_error("Private key not exist");
      }

      std::vector<uint8_t> connection_id = calculateConnectionId(action, nonce, vaultAddress, expiresAfter);
      std::map<std::string, std::string> domain = {
          {"name", "Exchange"},
          {"version", "1"},
          {"chainId", "1337"},
          {"verifyingContract", "0x0000000000000000000000000000000000000000"}
      };

      std::map<std::string, std::vector<std::map<std::string, std::string>>> types = {
          {"EIP712Domain", {
              {{"name", "name"}, {"type", "string"}},
              {{"name", "version"}, {"type", "string"}},
              {{"name", "chainId"}, {"type", "uint256"}},
              {{"name", "verifyingContract"}, {"type", "address"}}
          }},
          {"Agent", {
              {{"name", "source"}, {"type", "string"}},
              {{"name", "connectionId"}, {"type", "bytes32"}}
          }}
      };

      std::map<std::string, std::string> message = {
          {"source", isMainnet ? "a" : "b"},
          {"connectionId", std::string(connection_id.begin(), connection_id.end())}
      };

      std::vector<uint8_t> domain_separator = hashData(encodeData("EIP712Domain", types, domain));
      std::vector<uint8_t> message_hash = hashData(encodeData("Agent", types, message));

      std::vector<uint8_t> eip191_hash(32);
      std::vector<uint8_t> eip191_prefix = {0x19, 0x01};
      std::vector<uint8_t> eip191_data;
      eip191_data.insert(eip191_data.end(), eip191_prefix.begin(), eip191_prefix.end());
      eip191_data.insert(eip191_data.end(), domain_separator.begin(), domain_separator.end());
      eip191_data.insert(eip191_data.end(), message_hash.begin(), message_hash.end());
      auto final_hash = ethash_keccak256(eip191_data.data(), eip191_data.size());
      std::memcpy(eip191_hash.data(), final_hash.word64s, 32);

      auto secpSignature = signDigestSecp256k1(eip191_hash, private_key_hex);
      return {
          {"r", secpSignature.r},
          {"s", secpSignature.s},
          {"v", std::to_string(secpSignature.recoveryId)}
      };
  }

 private:
  std::string apiWalletAddressName;
  std::string apiPrivateKeyName;
  std::string apiVaultAddressName;
  std::string accountAddressName;
  bool isMainnetEnvironment;

  std::optional<std::string> resolveVaultAddress(const std::map<std::string, std::string>& param,
                                                 const std::map<std::string, std::string>& credential) const {
    auto paramIt = param.find(CCAPI_EM_HYPERLIQUID_VAULT_ADDRESS);
    if (paramIt != param.end()) {
      auto trimmed = UtilString::trim(paramIt->second);
      if (!trimmed.empty()) {
        return this->normalizeAddress(trimmed);
      }
    }
    auto credentialIt = credential.find(this->apiVaultAddressName);
    if (credentialIt != credential.end()) {
      auto trimmed = UtilString::trim(credentialIt->second);
      if (!trimmed.empty()) {
        return this->normalizeAddress(trimmed);
      }
    }
    return std::nullopt;
  }

  std::string resolveAccountAddress(const std::map<std::string, std::string>& param,
                                    const std::map<std::string, std::string>& credential) const {
    auto paramIt = param.find(CCAPI_EM_HYPERLIQUID_ACCOUNT_ADDRESS);
    if (paramIt != param.end()) {
      auto trimmed = UtilString::trim(paramIt->second);
      if (!trimmed.empty()) {
        return this->normalizeAddress(trimmed);
      }
    }
    auto credentialIt = credential.find(this->accountAddressName);
    if (credentialIt != credential.end()) {
      auto trimmed = UtilString::trim(credentialIt->second);
      if (!trimmed.empty()) {
        return this->normalizeAddress(trimmed);
      }
    }
    auto fallbackIt = credential.find(this->apiWalletAddressName);
    if (fallbackIt != credential.end()) {
      auto trimmed = UtilString::trim(fallbackIt->second);
      if (!trimmed.empty()) {
        return this->normalizeAddress(trimmed);
      }
    }
    throw std::runtime_error("Hyperliquid account address is missing. Set HYPERLIQUID_ACCOUNT_ADDRESS or "
                             "pass CCAPI_EM_HYPERLIQUID_ACCOUNT_ADDRESS.");
  }

  std::optional<uint64_t> resolveExpiresAfter(const std::map<std::string, std::string>& param) const {
    auto it = param.find(CCAPI_EM_HYPERLIQUID_EXPIRES_AFTER);
    if (it != param.end()) {
      auto trimmed = UtilString::trim(it->second);
      if (!trimmed.empty()) {
        return std::stoull(trimmed);
      }
    }
    return std::nullopt;
  }

  std::map<std::string, std::string> filterActionParameters(const std::map<std::string, std::string>& param) const {
    std::map<std::string, std::string> filtered;
    for (const auto& kv : param) {
      if (kv.first == CCAPI_EM_HYPERLIQUID_VAULT_ADDRESS || kv.first == CCAPI_EM_HYPERLIQUID_EXPIRES_AFTER) {
        continue;
      }
      filtered.insert(kv);
    }
    return filtered;
  }

  std::string normalizeAddress(const std::string& value) const {
    auto trimmed = UtilString::trim(value);
    if (trimmed.empty()) {
      throw std::runtime_error("Hyperliquid address cannot be empty");
    }
    auto lower = UtilString::toLower(trimmed);
    if (lower.rfind("0x", 0) == 0) {
      lower = lower.substr(2);
    }
    if (lower.size() != 40) {
      throw std::runtime_error("Hyperliquid address must have 40 hex characters");
    }
    return "0x" + lower;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HYPERLIQUID_H_
