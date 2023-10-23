#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_DERIVATIVES_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_DERIVATIVES_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_base.h"
namespace ccapi {
class ExecutionManagementServiceBinanceDerivativesBase : public ExecutionManagementServiceBinanceBase {
 public:
  ExecutionManagementServiceBinanceDerivativesBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions,
                                                   SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->isDerivatives = true;
  }
  virtual ~ExecutionManagementServiceBinanceDerivativesBase() {}
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        this->prepareReq(req, credential);
        req.method(http::verb::get);
        std::string queryString;
        this->appendParam(queryString, {});
        this->signRequest(queryString, {}, now, credential);
        req.target(this->getAccountPositionsTarget + "?" + queryString);
      } break;
      default:
        ExecutionManagementServiceBinanceBase::convertRequestForRest(req, request, now, symbolId, credential);
    }
  }
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          std::cout << __LINE__ << std::endl;
          element.insert(CCAPI_EM_POSITION_SIDE, x["positionSide"].GetString());
          std::cout << __LINE__ << std::endl;
          std::string positionAmt;
          std::cout << __LINE__ << std::endl;
          auto it = x.FindMember("positionAmt");
          std::cout << __LINE__ << std::endl;
          if (it != x.MemberEnd()) {
            std::cout << __LINE__ << std::endl;
            positionAmt = it->value.GetString();
            std::cout << __LINE__ << std::endl;
          } else {
            std::cout << __LINE__ << std::endl;
            positionAmt = x["maxQty"].GetString();
            std::cout << __LINE__ << std::endl;
          }
          element.insert(CCAPI_EM_POSITION_QUANTITY, positionAmt);
          std::cout << __LINE__ << std::endl;
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["entryPrice"].GetString());
          std::cout << __LINE__ << std::endl;
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["leverage"].GetString());
          std::cout << __LINE__ << std::endl;
          element.insert(CCAPI_EM_UNREALIZED_PNL, x["unrealizedProfit"].GetString());
          std::cout << __LINE__ << std::endl;
          element.insert(CCAPI_LAST_UPDATED_TIME_SECONDS, UtilTime::convertMillisecondsStrToSecondsStr(x["updateTime"].GetString()));
          std::cout << __LINE__ << std::endl;
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        ExecutionManagementServiceBinanceBase::extractAccountInfoFromRequest(elementList, request, operation, document);
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_DERIVATIVES_BASE_H_
