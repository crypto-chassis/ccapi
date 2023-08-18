#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_BITGET) || defined(CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBitgetBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBitgetBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                       ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->hostHttpHeaderValueIgnorePort = true;
  }
  virtual ~ExecutionManagementServiceBitgetBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, "ping", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"00000\"")); }
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("ACCESS-TIMESTAMP").to_string();
    preSignedText += methodString;
    auto target = path;
    if (!queryString.empty()) {
      target += "?";
      target += queryString;
    }
    preSignedText += target;
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    if (!headerString.empty()) {
      headerString += "\r\n";
    }
    headerString += "ACCESS-SIGN:" + signature;
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("ACCESS-TIMESTAMP").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("ACCESS-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    rjValue.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void appendSymbolId(std::string& queryString, const std::string& symbolId) {
    queryString += "symbol=";
    queryString += Url::urlEncode(symbolId);
    queryString += "&";
  }
  void prepareReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    req.set("locale", "en-US");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("ACCESS-KEY", apiKey);
    req.set("ACCESS-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    req.set("ACCESS-PASSPHRASE", apiPassphrase);
  }
  std::string apiPassphraseName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BITGET_BASE_H_
