#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/service/ccapi_fix_service_ftx_base.h"
namespace ccapi {
class FixServiceFtx : public FixServiceFtxBase {
 public:
  FixServiceFtx(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                ServiceContextPtr serviceContextPtr)
      : FixServiceFtxBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrlFix = this->sessionConfigs.getUrlFixBase().at(this->exchangeName);
    this->setHostFixFromUrlFix(this->hostFix, this->portFix, this->baseUrlFix);
    this->apiKeyName = CCAPI_FTX_API_KEY;
    this->apiSecretName = CCAPI_FTX_API_SECRET;
    this->apiSubaccountName = CCAPI_FTX_API_SUBACCOUNT;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiSubaccountName});
    try {
      this->tcpResolverResultsFix = this->resolver.resolve(this->hostFix, this->portFix);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->protocolVersion = CCAPI_FIX_PROTOCOL_VERSION_FTX;
    this->targetCompID = "FTX";
  }
  virtual ~FixServiceFtx() {}
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_H_
