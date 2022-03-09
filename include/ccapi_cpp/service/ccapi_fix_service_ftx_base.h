#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#if defined(CCAPI_ENABLE_EXCHANGE_FTX) || defined(CCAPI_ENABLE_EXCHANGE_FTX_US)
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/service/ccapi_fix_service.h"
namespace ccapi {
class FixServiceFtxBase : public FixService<beast::ssl_stream<beast::tcp_stream>> {
 public:
  FixServiceFtxBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                    ServiceContextPtr serviceContextPtr)
      : FixService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~FixServiceFtxBase() {}
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
    param.push_back({hff::tag::HeartBtInt, "30"});
    auto msgSeqNum = std::to_string(this->sequenceSentByConnectionIdMap[connectionId] + 1);
    auto credential = this->credentialByConnectionIdMap[connectionId];
    auto senderCompID = mapGetWithDefault(credential, this->apiKeyName);
    auto targetCompID = this->targetCompID;
    std::vector<std::string> prehashFieldList{nowFixTimeStr, msgType, msgSeqNum, senderCompID, targetCompID};
    auto prehashStr = UtilString::join(prehashFieldList, "\x01");
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string rawData = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, prehashStr, true);
    param.push_back({hff::tag::RawData, rawData});
    for (const auto& x : logonOptionMap) {
      param.push_back({x.first, x.second});
    }
    auto apiSubaccountName = mapGetWithDefault(credential, this->apiSubaccountName);
    if (!apiSubaccountName.empty()) {
      param.push_back({hff::tag::Account, apiSubaccountName});
    }
    return param;
  }
  std::string apiSubaccountName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_BASE_H_
