#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_BYBIT) || defined(CCAPI_ENABLE_EXCHANGE_BYBIT_DERIVATIVES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBybitBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBybitBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                      ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~ExecutionManagementServiceBybitBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("retCode":0)") == std::string::npos; }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, R"({"op":"ping"})", ec); }
#endif
  void signReqeustForRestGenericPrivateRequest(http::request<http::string_body>& req, const Request& request, std::string& methodString,
                                               std::string& headerString, std::string& path, std::string& queryString, std::string& body, const TimePoint& now,
                                               const std::map<std::string, std::string>& credential) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("X-BAPI-TIMESTAMP").to_string();
    preSignedText += apiKey;
    preSignedText += req.base().at("X-BAPI-RECV-WINDOW").to_string();
    std::string aString;
    if (methodString == "GET") {
      aString = queryString;
    } else if (methodString == "POST") {
      aString = body;
    }
    preSignedText += aString;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("X-BAPI-SIGN", signature);
  }
  void signRequest(http::request<http::string_body>& req, const std::string aString, const TimePoint& now,
                   const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("X-BAPI-TIMESTAMP").to_string();
    preSignedText += apiKey;
    preSignedText += req.base().at("X-BAPI-RECV-WINDOW").to_string();
    preSignedText += aString;
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    req.set("X-BAPI-SIGN", signature);
  }
  void appendParamToQueryString(std::string& queryString, const std::map<std::string, std::string>& param,
                                const std::map<std::string, std::string> standardizationMap = {}) {
    for (const auto& kv : param) {
      queryString += standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += Url::urlEncode(kv.second);
      queryString += "&";
    }
  }
  // void appendSymbolId(std::string& queryString, const std::string& symbolId) {
  //   queryString += "symbol=";
  //   queryString += Url::urlEncode(symbolId);
  //   queryString += "&";
  // }
  // void appendSymbolId(rj::Value& rjValue, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
  //   rjValue.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  // }
  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("X-BAPI-SIGN-TYPE", "2");
    req.set("X-BAPI-API-KEY", apiKey);
    req.set("X-BAPI-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    req.set("X-BAPI-RECV-WINDOW", std::to_string(CCAPI_BYBIT_BASE_API_RECEIVE_WINDOW_MILLISECONDS));
    req.set("Referer", CCAPI_BYBIT_API_BROKER_ID);
  }
  std::vector<std::string> createSendStringListFromSubscription(const WsConnection& wsConnection, const Subscription& subscription, const TimePoint& now,
                                                                const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("auth").Move(), allocator);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto expires = std::chrono::duration_cast<std::chrono::milliseconds>(
                       (now + std::chrono::milliseconds(CCAPI_BYBIT_BASE_API_RECEIVE_WINDOW_MILLISECONDS)).time_since_epoch())
                       .count();
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string preSignedText = "GET";
    preSignedText += "/realtime";
    preSignedText += std::to_string(expires);
    auto signature = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText, true);
    rj::Value args(rj::kArrayType);
    args.PushBack(rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    args.PushBack(rj::Value(expires).Move(), allocator);
    args.PushBack(rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
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
  virtual Event createEvent(const WsConnection& wsConnection, wspp::connection_hdl hdl, const Subscription& subscription, const std::string& textMessage,
                            const rj::Document& document, const TimePoint& timeReceived) {
    return {};
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
  virtual Event createEvent(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                            const rj::Document& document, const TimePoint& timeReceived) {
    return {};
  }
#endif
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BYBIT_BASE_H_
