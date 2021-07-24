#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_GEMINI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_GEMINI_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_fix_service.h"
namespace ccapi {
class FixServiceGemini : public FixService<beast::tcp_stream> {
 public:
  FixServiceGemini(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                   ServiceContextPtr serviceContextPtr)
      : FixService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_GEMINI;
    this->hostFix = CCAPI_GEMINI_URL_FIX_HOST;
    CCAPI_LOGGER_INFO(this->baseUrlFix);
    this->portFix = CCAPI_GEMINI_URL_FIX_PORT;
    // this->setHostFixFromUrlFix(this->baseUrlFix);CCAPI_LOGGER_INFO(this->hostFix);CCAPI_LOGGER_INFO(this->portFix);
    try {
      this->tcpResolverResultsFix = this->resolver.resolve(this->hostFix, this->portFix);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    // this->apiKeyName = CCAPI_GEMINI_API_KEY;
    // this->apiSecretName = CCAPI_GEMINI_API_SECRET;
    // this->apiPassphraseName = CCAPI_GEMINI_API_PASSPHRASE;
    // this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->protocolVersion = CCAPI_FIX_PROTOCOL_VERSION_GEMINI;
    this->senderCompID = CCAPI_GEMINI_API_SENDER_COMP_ID;
    this->targetCompID = CCAPI_GEMINI_API_TARGET_COMP_ID;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~FixServiceGemini() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  virtual std::vector<std::pair<int, std::string>> createCommonParam(const std::string& connectionId, const std::string& nowFixTimeStr) {
    return {
        {hff::tag::SenderCompID, CCAPI_GEMINI_API_SENDER_COMP_ID},
        {hff::tag::TargetCompID, CCAPI_GEMINI_API_TARGET_COMP_ID},
        {hff::tag::MsgSeqNum, std::to_string(++this->sequenceSentByConnectionIdMap[connectionId])},
        {hff::tag::SendingTime, nowFixTimeStr},
    };
  }
  virtual std::vector<std::pair<int, std::string>> createLogonParam(const std::string& connectionId, const std::string& nowFixTimeStr,
                                                                    const std::map<int, std::string> logonOptionMap = {}) {
    std::vector<std::pair<int, std::string>> param;
    auto msgType = "A";
    param.push_back({hff::tag::MsgType, msgType});
    param.push_back({hff::tag::ResetSeqNumFlag, "Y"});
    param.push_back({hff::tag::EncryptMethod, "0"});
    param.push_back({hff::tag::HeartBtInt, std::to_string(this->sessionOptions.heartbeatFixIntervalMilliSeconds / 1000)});
    for (const auto& x : logonOptionMap) {
      param.push_back({x.first, x.second});
    }
    return param;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_GEMINI_H_
