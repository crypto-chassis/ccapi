#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_DERIBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_DERIBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/service/ccapi_fix_service.h"
namespace ccapi {
class FixServiceDeribit : public FixService<beast::ssl_stream<beast::tcp_stream>>{
 public:
  FixServiceDeribit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                     ServiceContextPtr serviceContextPtr)
      : FixService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_DERIBIT;
    this->baseUrlFix = this->sessionConfigs.getUrlFixBase().at(this->exchangeName);
    this->setHostFixFromUrlFix(this->baseUrlFix);
    try {
      this->tcpResolverResultsFix = this->resolver.resolve(this->hostFix, this->portFix);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->apiKeyName = CCAPI_DERIBIT_CLIENT_ID;
    this->apiSecretName = CCAPI_DERIBIT_CLIENT_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    this->protocolVersion = CCAPI_FIX_PROTOCOL_VERSION_DERIBIT;
    this->targetCompID = "Deribit";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~FixServiceDeribit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  virtual std::vector<std::pair<int, std::string>> createCommonParam(const std::string& connectionId, const std::string& nowFixTimeStr) {
    return {
        {hff::tag::SenderCompID, mapGetWithDefault(this->credentialByConnectionIdMap[connectionId], this->apiKeyName)},
        {hff::tag::TargetCompID, this->targetCompID},
        {hff::tag::MsgSeqNum, std::to_string(++this->sequenceSentByConnectionIdMap[connectionId])},
        {hff::tag::SendingTime, nowFixTimeStr},
    };
  }
  virtual std::vector<std::pair<int, std::string>> createLogonParam(const std::string& connectionId, const std::string& nowFixTimeStr,
                                                                    const std::map<int, std::string> logonOptionMap = {}) {
    std::vector<std::pair<int, std::string>> param;
    auto msgType = "A";
    param.push_back({hff::tag::MsgType, msgType});
    param.push_back({hff::tag::EncryptMethod, "0"});
    param.push_back({hff::tag::HeartBtInt, std::to_string(this->sessionOptions.heartbeatFixIntervalMilliSeconds / 1000)});
    auto credential = this->credentialByConnectionIdMap[connectionId];
    auto msgSeqNum = std::to_string(this->sequenceSentByConnectionIdMap[connectionId] + 1);
    auto senderCompID = mapGetWithDefault(credential, this->apiKeyName);
    auto targetCompID = this->targetCompID;
    std::vector<std::string> prehashFieldList{nowFixTimeStr, msgType, msgSeqNum, senderCompID, targetCompID};
    auto prehashStr = UtilString::join(prehashFieldList, "\x01");
    CCAPI_LOGGER_TRACE("prehashStr = " + printableString(prehashStr));
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto rawData = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), prehashStr));
    CCAPI_LOGGER_TRACE("rawData = " + rawData);
    param.push_back({hff::tag::RawData, rawData});
    for (const auto& x : logonOptionMap) {
      param.push_back({x.first, x.second});
    }
    return param;
  }

};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_DERIBIT_H_
