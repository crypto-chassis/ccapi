#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_BINANCE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_BINANCE_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/service/ccapi_fix_service.h"

namespace ccapi {

class FixServiceBinance : public FixService {
 public:
  FixServiceBinance(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                    ServiceContextPtr serviceContextPtr)
      : FixService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BINANCE;
    this->baseUrlFix = this->sessionConfigs.getUrlFixBase().at(this->exchangeName);
    this->baseUrlFixMarketData = sessionConfigs.getUrlFixMarketDataBase().at(this->exchangeName);
    this->setHostFixFromUrlFix(this->hostFix, this->portFix, this->baseUrlFix);
    this->setHostFixFromUrlFix(this->hostFixMarketData, this->portFixMarketData, this->baseUrlFixMarketData);
    this->fixApiKeyName = CCAPI_BINANCE_FIX_API_KEY;
    this->fixApiPrivateKeyPathName = CCAPI_BINANCE_FIX_API_PRIVATE_KEY_PATH;
    this->fixApiPrivateKeyPasswordName = CCAPI_BINANCE_FIX_API_PRIVATE_KEY_PASSWORD;
    this->setupCredential({this->fixApiKeyName, this->fixApiPrivateKeyPathName, this->fixApiPrivateKeyPasswordName});
    this->protocolVersion = CCAPI_FIX_PROTOCOL_VERSION_BINANCE;
    this->targetCompId = "SPOT";
  }

  virtual ~FixServiceBinance() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  std::string createSenderCompId() {
    static uint8_t nextSenderCompId = 0;
    this->senderCompId = std::to_string(++nextSenderCompId);
    return this->senderCompId;
  }

  std::vector<std::pair<int, std::string>> createCommonParam(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& nowFixTimeStr) override {
    return {
        {hff::tag::SenderCompID, this->senderCompId},
        {hff::tag::TargetCompID, this->targetCompId},
        {hff::tag::MsgSeqNum, std::to_string(++this->fixMsgSeqNumByConnectionIdMap[fixConnectionPtr->id])},
        {hff::tag::SendingTime, nowFixTimeStr},
    };
  }

  std::vector<std::pair<int, std::string>> createLogonParam(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& nowFixTimeStr,
                                                            const std::map<int, std::string> logonOptionMap = {}) override {
    std::vector<std::pair<int, std::string>> param;
    std::string msgType = "A";
    param.push_back({hff::tag::MsgType, msgType});
    param.push_back({hff::tag::EncryptMethod, "0"});
    param.push_back({hff::tag::HeartBtInt, std::to_string(this->sessionOptions.heartbeatFixIntervalMilliseconds / 1000)});
    const auto& credential = fixConnectionPtr->credential;
    const auto& msgSeqNum = std::to_string(1);
    this->senderCompId = this->createSenderCompId();
    std::vector<std::string> prehashFieldList{msgType, this->senderCompId, this->targetCompId, msgSeqNum, nowFixTimeStr};
    const auto& payload = UtilString::join(prehashFieldList, "\x01");

    auto it = credential.find(this->fixApiPrivateKeyPathName);
    if (it == credential.end()) {
      throw std::runtime_error("Missing credential: " + this->fixApiPrivateKeyPathName);
    }
    std::string password;
    if (auto it = credential.find(this->fixApiPrivateKeyPasswordName); it != credential.end()) {
      password = it->second;
    }
    EVP_PKEY* pkey = UtilAlgorithm::loadPrivateKey(UtilAlgorithm::readFile(it->second), password);
    std::string signature = UtilAlgorithm::signPayload(pkey, payload);
    param.push_back({hff::tag::RawDataLength, std::to_string(signature.length())});
    param.push_back({hff::tag::RawData, signature});
    param.push_back({hff::tag::ResetSeqNumFlag, "Y"});
    param.push_back({hff::tag::Username, credential.at(this->fixApiKeyName)});
    param.push_back({kMessageHandlingTag, "1"});
    for (const auto& x : logonOptionMap) {
      param.push_back({x.first, x.second});
    }
    return param;
  }

#ifndef CCAPI_EXPOSE_INTERNAL
 protected:
#endif
  static constexpr int kMessageHandlingTag = 25035;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_BINANCE_H_
