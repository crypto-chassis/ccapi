#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_execution_management_service_ftx_base.h"
namespace ccapi {
class ExecutionManagementServiceFtx : public ExecutionManagementServiceFtxBase {
 public:
  ExecutionManagementServiceFtx(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceFtxBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->apiKeyName = CCAPI_FTX_API_KEY;
    this->apiSecretName = CCAPI_FTX_API_SECRET;
    this->apiSubaccountName = CCAPI_FTX_API_SUBACCOUNT;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiSubaccountName});
    this->getAccountPositionsTarget = "/api/positions";
    this->ftx = "FTX";
  }
  virtual ~ExecutionManagementServiceFtx() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        this->prepareReq(req, now, credential);
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        auto target = this->getAccountPositionsTarget;
        req.target(target);
        this->signRequest(req, "", credential);
      } break;
      default:
        ExecutionManagementServiceFtxBase::convertRequestForRest(req, request, now, symbolId, credential);
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document["result"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["future"].GetString());
          element.insert(CCAPI_EM_POSITION_SIDE, x["side"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
          element.insert(CCAPI_EM_POSITION_COST, x["cost"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        ExecutionManagementServiceFtxBase::extractAccountInfoFromRequest(elementList, request, operation, document);
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
