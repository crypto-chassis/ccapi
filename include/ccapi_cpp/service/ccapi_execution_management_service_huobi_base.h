#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceHuobiBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceHuobiBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~ExecutionManagementServiceHuobiBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void createSignature(std::string& signature, std::string& queryString, const std::string& reqMethod, const std::string& host, const std::string& path,
                       const std::map<std::string, std::string>& queryParamMap, const std::map<std::string, std::string>& credential) {
    std::string preSignedText;
    preSignedText += reqMethod;
    preSignedText += "\n";
    preSignedText += host;
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    int i = 0;
    for (const auto& kv : queryParamMap) {
      queryString += kv.first;
      queryString += "=";
      queryString += kv.second;
      if (i < queryParamMap.size() - 1) {
        queryString += "&";
      }
      i++;
    }
    preSignedText += queryString;
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
  }
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    std::map<std::string, std::string> queryParamMap;
    if (!queryString.empty()) {
      for (const auto& x : UtilString::split(queryString, "&")) {
        auto y = UtilString::split(x, "=");
        queryParamMap.insert(std::make_pair(y.at(0), y.at(1)));
      }
    }
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    queryParamMap.insert(std::make_pair("AccessKeyId", apiKey));
    queryParamMap.insert(std::make_pair("SignatureMethod", "HmacSHA256"));
    queryParamMap.insert(std::make_pair("SignatureVersion", "2"));
    std::string timestamp = UtilTime::getISOTimestamp<std::chrono::seconds>(now, "%FT%T");
    queryParamMap.insert(std::make_pair("Timestamp", Url::urlEncode(timestamp)));
    std::string signature;
    this->createSignature(signature, queryString, methodString, this->hostRest, path, queryParamMap, credential);
    if (!queryString.empty()) {
      queryString += "&";
    }
    queryString += "Signature=";
    queryString += Url::urlEncode(signature);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::map<std::string, std::string>& queryParamMap,
                   const std::map<std::string, std::string>& credential) {
    std::string signature;
    std::string queryString;
    this->createSignature(signature, queryString, std::string(req.method_string()), this->hostRest, path, queryParamMap, credential);
    queryString += "&Signature=";
    queryString += Url::urlEncode(signature);
    req.target(path + "?" + queryString);
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (this->isDerivatives) {
        if (key == "direction") {
          value = UtilString::toLower(value);
        }
      } else {
        if (key == "type") {
          if (value == CCAPI_EM_ORDER_SIDE_BUY)
            value = "buy-limit";
          else if (value == CCAPI_EM_ORDER_SIDE_SELL)
            value = "sell-limit";
        }
      }
      document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }
  void appendParam(std::map<std::string, std::string>& queryParamMap, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      queryParamMap.insert(std::make_pair(standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first,
                                          Url::urlEncode(kv.second)));
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId, const std::string symbolIdCalled) {
    document.AddMember(rj::Value(symbolIdCalled.c_str(), allocator).Move(), rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::map<std::string, std::string>& queryParamMap, const std::string& symbolId, const std::string symbolIdCalled) {
    queryParamMap.insert(std::make_pair(symbolIdCalled, Url::urlEncode(symbolId)));
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    std::map<std::string, std::string> queryParamMap;
    queryParamMap.insert(std::make_pair("AccessKeyId", apiKey));
    queryParamMap.insert(std::make_pair("SignatureMethod", "HmacSHA256"));
    queryParamMap.insert(std::make_pair("SignatureVersion", "2"));
    std::string timestamp = UtilTime::getISOTimestamp<std::chrono::seconds>(now, "%FT%T");
    queryParamMap.insert(std::make_pair("Timestamp", Url::urlEncode(timestamp)));
    this->convertReqDetail(req, request, now, symbolId, credential, queryParamMap);
  }
  virtual void convertReqDetail(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                                const std::map<std::string, std::string>& credential, std::map<std::string, std::string>& queryParamMap) {}
  bool isDerivatives{};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_BASE_H_
