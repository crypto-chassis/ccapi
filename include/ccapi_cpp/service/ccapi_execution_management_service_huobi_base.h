#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceHuobiBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceHuobiBase(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
  }
  virtual ~ExecutionManagementServiceHuobiBase() {
  }

 protected:
  void signRequest(http::request<http::string_body>& req, const std::string& path, const std::map<std::string, std::string>& queryParamMap, const std::map<std::string, std::string>& credential) {
    std::string preSignedText;
    preSignedText += std::string(req.method_string());
    preSignedText += "\n";
    preSignedText += this->isDerivatives ? this->hostRest : req.base().at(http::field::host).to_string();
    preSignedText += "\n";
    preSignedText += path;
    preSignedText += "\n";
    std::string queryString;
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
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    queryString += "&Signature=";
    queryString += Url::urlEncode(signature);
    req.target(path + "?" + queryString);
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> regularizationMap = {}) {
    for (const auto& kv : param) {
      auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (this->isDerivatives) {
        if (key == "direction") {
          value = UtilString::toLower(value);
        }
      } else {
        if (key == "type") {
          value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy-limit" : "sell-limit";
        }
      }
      document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }
  void appendParam(std::map<std::string, std::string>& queryParamMap, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> regularizationMap = {}) {
    for (const auto& kv : param) {
      queryParamMap.insert(std::make_pair(regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first,
                         Url::urlEncode(kv.second)));
    }
  }
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId, const std::string symbolIdCalled) {
    document.AddMember(rj::Value(symbolIdCalled.c_str(), allocator).Move(), rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::map<std::string, std::string>& queryParamMap, const std::string& symbolId, const std::string symbolIdCalled) {
    queryParamMap.insert(std::make_pair(symbolIdCalled, Url::urlEncode(symbolId)));
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    std::map<std::string, std::string> queryParamMap;
    queryParamMap.insert(std::make_pair("AccessKeyId", apiKey));
    queryParamMap.insert(std::make_pair("SignatureMethod", "HmacSHA256"));
    queryParamMap.insert(std::make_pair("SignatureVersion", "2"));
    std::string timestamp = UtilTime::getISOTimestamp<std::chrono::seconds>(now, "%FT%T");
    queryParamMap.insert(std::make_pair("Timestamp", Url::urlEncode(timestamp)));
    this->convertReqDetail(req, request, operation, now, symbolId, credential, queryParamMap);
  }
  virtual void convertReqDetail(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential, std::map<std::string, std::string>& queryParamMap) = 0;
  bool isDerivatives{};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_BASE_H_
