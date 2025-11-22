#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_EDGEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_EDGEX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_EDGEX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "ccapi_cpp/crypto/keccak.h"
#include "ccapi_cpp/crypto/secp256k1_ecdsa.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace ccapi {

class ExecutionManagementServiceEdgex : public ExecutionManagementService {
 public:
  ExecutionManagementServiceEdgex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                  ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_EDGEX;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->apiKeyName = CCAPI_EDGEX_API_KEY;
    this->apiSecretName = CCAPI_EDGEX_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->createOrderTarget = "/api/v1/private/order/create";
    this->cancelOrderTarget = "/api/v1/private/order/cancel";
    this->getOrderTarget = "/api/v1/private/order/query";
    this->getOpenOrdersTarget = "/api/v1/private/order/open";
    this->getAccountBalancesTarget = "/api/v1/private/account/balance";
    this->getAccountPositionsTarget = "/api/v1/private/account/position";
  }

  virtual ~ExecutionManagementServiceEdgex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override {
    return bodyView.find("\"code\":") != boost::beast::string_view::npos && bodyView.find("\"code\":0") == boost::beast::string_view::npos;
  }

  std::string buildSignatureContent(const std::string& timestamp, const std::string& method, const std::string& path,
                                    const std::string& queryString, const std::string& body) {
    std::string content = timestamp + method + path;
    if (!queryString.empty()) {
      content += "?" + queryString;
    }
    if (!body.empty()) {
      content += body;
    }
    return content;
  }

  std::string sortAndStringifyParams(const std::map<std::string, std::string>& params) {
    std::vector<std::pair<std::string, std::string>> sortedParams(params.begin(), params.end());
    std::sort(sortedParams.begin(), sortedParams.end());
    std::ostringstream oss;
    bool first = true;
    for (const auto& pair : sortedParams) {
      if (!first) {
        oss << "&";
      }
      oss << pair.first << "=" << pair.second;
      first = false;
    }
    return oss.str();
  }

  void signRequest(http::request<http::string_body>& req, const std::string& method, const std::string& path,
                   const std::string& queryString, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    
    auto now = UtilTime::now();
    auto timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    
    std::string signatureContent = this->buildSignatureContent(timestamp, method, path, queryString, body);
    auto hash = ethash_keccak256(reinterpret_cast<const uint8_t*>(signatureContent.data()), signatureContent.size());
    
    std::vector<uint8_t> hashBytes(hash.bytes, hash.bytes + 32);
    auto signature = signDigestSecp256k1(hashBytes, apiSecret);
    
    // Remove "0x" prefix from r and s, then concatenate
    std::string r = signature.r.substr(2);
    std::string s = signature.s.substr(2);
    std::string signatureHex = r + s;
    
    req.set("X-API-KEY", apiKey);
    req.set("X-TIMESTAMP", timestamp);
    req.set("X-SIGNATURE", signatureHex);
  }

  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    this->signRequest(req, methodString, path, queryString, body, credential);
  }

  void appendParam(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {
                       {CCAPI_EM_ORDER_SIDE, "side"},
                       {CCAPI_EM_ORDER_QUANTITY, "quantity"},
                       {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                       {CCAPI_EM_CLIENT_ORDER_ID, "clientOrderId"},
                       {CCAPI_SYMBOL_ID, "contractId"},
                       {CCAPI_EM_ORDER_ID, "orderId"},
                   }) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (key == "side") {
        value = (value == CCAPI_EM_ORDER_SIDE_BUY || value == "buy") ? "BUY" : "SELL";
      }
      rjValue.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }

  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    rjValue.AddMember("contractId", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
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
        if (!symbolId.empty()) {
          this->appendSymbolId(document, allocator, symbolId);
        }
        if (param.find("orderType") == param.end()) {
          document.AddMember("orderType", rj::Value("LIMIT").Move(), allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        this->signRequest(req, "POST", this->createOrderTarget, "", body, credential);
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
        this->signRequest(req, "POST", this->cancelOrderTarget, "", body, credential);
      } break;
      case Request::Operation::GET_ORDER: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        if (param.find(CCAPI_EM_ORDER_ID) != param.end()) {
          queryString += "orderId=" + Url::urlEncode(param.at(CCAPI_EM_ORDER_ID));
        } else if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
          queryString += "clientOrderId=" + Url::urlEncode(param.at(CCAPI_EM_CLIENT_ORDER_ID));
        }
        if (!symbolId.empty()) {
          if (!queryString.empty()) {
            queryString += "&";
          }
          queryString += "contractId=" + Url::urlEncode(symbolId);
        }
        req.target(queryString.empty() ? this->getOrderTarget : this->getOrderTarget + "?" + queryString);
        this->signRequest(req, "GET", this->getOrderTarget, queryString, "", credential);
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::get);
        std::string queryString;
        if (!symbolId.empty()) {
          queryString += "contractId=" + Url::urlEncode(symbolId);
        }
        req.target(queryString.empty() ? this->getOpenOrdersTarget : this->getOpenOrdersTarget + "?" + queryString);
        this->signRequest(req, "GET", this->getOpenOrdersTarget, queryString, "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::get);
        req.target(this->getAccountBalancesTarget);
        this->signRequest(req, "GET", this->getAccountBalancesTarget, "", "", credential);
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::get);
        std::string queryString;
        if (!symbolId.empty()) {
          queryString += "contractId=" + Url::urlEncode(symbolId);
        }
        req.target(queryString.empty() ? this->getAccountPositionsTarget : this->getAccountPositionsTarget + "?" + queryString);
        this->signRequest(req, "GET", this->getAccountPositionsTarget, queryString, "", credential);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rj::Document& document) override {
    std::map<std::string_view, std::pair<std::string_view, JsonDataType>> extractionFieldNameMap = {
        {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::STRING)},
        {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_QUANTITY, std::make_pair("quantity", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
        {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("contractId", JsonDataType::STRING)},
    };
    const rj::Value& data = document.HasMember("data") ? document["data"] : document;
    if (data.IsObject()) {
      Element element;
      this->extractOrderInfo(element, data, extractionFieldNameMap);
      elementList.emplace_back(std::move(element));
    } else if (data.IsArray()) {
      for (const auto& x : data.GetArray()) {
        Element element;
        this->extractOrderInfo(element, x, extractionFieldNameMap);
        elementList.emplace_back(std::move(element));
      }
    }
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                    const rj::Document& document) override {
    switch (operation) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        const rj::Value& data = document.HasMember("data") ? document["data"] : document;
        if (data.IsArray()) {
          for (const auto& x : data.GetArray()) {
            Element element;
            if (x.HasMember("asset")) {
              element.insert(CCAPI_EM_ASSET, x["asset"].GetString());
            }
            if (x.HasMember("available")) {
              element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
            }
            if (x.HasMember("total")) {
              element.insert(CCAPI_EM_QUANTITY_TOTAL, x["total"].GetString());
            }
            elementList.emplace_back(std::move(element));
          }
        } else if (data.IsObject()) {
          Element element;
          if (data.HasMember("asset")) {
            element.insert(CCAPI_EM_ASSET, data["asset"].GetString());
          }
          if (data.HasMember("available")) {
            element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, data["available"].GetString());
          }
          if (data.HasMember("total")) {
            element.insert(CCAPI_EM_QUANTITY_TOTAL, data["total"].GetString());
          }
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        const rj::Value& data = document.HasMember("data") ? document["data"] : document;
        if (data.IsArray()) {
          for (const auto& x : data.GetArray()) {
            Element element;
            if (x.HasMember("contractId")) {
              element.insert(CCAPI_INSTRUMENT, x["contractId"].GetString());
            }
            if (x.HasMember("side")) {
              element.insert(CCAPI_EM_POSITION_SIDE, x["side"].GetString());
            }
            if (x.HasMember("size")) {
              element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
            }
            if (x.HasMember("entryPrice")) {
              element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["entryPrice"].GetString());
            }
            elementList.emplace_back(std::move(element));
          }
        } else if (data.IsObject()) {
          Element element;
          if (data.HasMember("contractId")) {
            element.insert(CCAPI_INSTRUMENT, data["contractId"].GetString());
          }
          if (data.HasMember("side")) {
            element.insert(CCAPI_EM_POSITION_SIDE, data["side"].GetString());
          }
          if (data.HasMember("size")) {
            element.insert(CCAPI_EM_POSITION_QUANTITY, data["size"].GetString());
          }
          if (data.HasMember("entryPrice")) {
            element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, data["entryPrice"].GetString());
          }
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
};

} /* namespace ccapi */
#endif
#endif
#endif /* INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_EDGEX_H_ */

