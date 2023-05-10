#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/service/ccapi_execution_management_service_kucoin_base.h"
namespace ccapi {
class ExecutionManagementServiceKucoin : public ExecutionManagementServiceKucoinBase {
 public:
  ExecutionManagementServiceKucoin(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                   ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceKucoinBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KUCOIN;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    // #ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->apiKeyName = CCAPI_KUCOIN_API_KEY;
    this->apiSecretName = CCAPI_KUCOIN_API_SECRET;
    this->apiPassphraseName = CCAPI_KUCOIN_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->createOrderTarget = "/api/v1/orders";
    this->cancelOrderTarget = "/api/v1/orders/<id>";
    this->getOrderTarget = "/api/v1/orders/<id>";
    this->getOrderByClientOrderIdTarget = "/api/v1/order/client-order/<id>";
    this->getOpenOrdersTarget = "/api/v1/orders";
    this->cancelOpenOrdersTarget = "/api/v1/orders";
    this->getAccountsTarget = "/api/v1/accounts";
    this->getAccountBalancesTarget = "/api/v1/accounts/<accountId>";
    this->topicTradeOrders = "/spotMarket/tradeOrders";
    this->createOrderMarginTarget = "/api/v1/margin/order";
  }
  virtual ~ExecutionManagementServiceKucoin() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    const auto& data = document["data"];
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNTS: {
        for (const auto& x : data.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ACCOUNT_ID, x["id"].GetString());
          element.insert(CCAPI_EM_ACCOUNT_TYPE, x["type"].GetString());
          element.insert(CCAPI_EM_ASSET, x["currency"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["balance"].GetString());
          element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, x["available"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        element.insert(CCAPI_EM_ASSET, data["currency"].GetString());
        element.insert(CCAPI_EM_QUANTITY_TOTAL, data["balance"].GetString());
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, data["available"].GetString());
        elementList.emplace_back(std::move(element));
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_KUCOIN_H_
