#pragma once

#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_kucoin_base.h"

namespace ccapi {

class ExecutionManagementServiceKucoinFutures : public ExecutionManagementServiceKucoinBase {
 public:
  ExecutionManagementServiceKucoinFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceKucoinBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->apiKeyName = CCAPI_KUCOIN_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_KUCOIN_FUTURES_API_SECRET;
    this->apiPassphraseName = CCAPI_KUCOIN_FUTURES_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->createOrderTarget = "/api/v1/orders";
    this->cancelOrderTarget = "/api/v1/orders/<id>";
    this->getOrderTarget = "/api/v1/orders/<id>";
    this->getOrderByClientOrderIdTarget = "/api/v1/orders/byClientOid?clientOid=<id>";
    this->getOpenOrdersTarget = "/api/v1/orders";
    this->cancelOpenOrdersTarget = "/api/v1/orders";
    this->getAccountBalancesTarget = "/api/v1/account-overview";
    this->getAccountPositionsTarget = "/api/v1/positions";
    this->topicTradeOrders = "/contractMarket/tradeOrders";
    this->isDerivatives = true;
  }

  virtual ~ExecutionManagementServiceKucoinFutures() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    const auto& data = document["data"];
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        Element element;
        element.insert(CCAPI_EM_ASSET, data["currency"].GetString());
        element.insert(CCAPI_EM_QUANTITY_TOTAL, data["accountEquity"].GetString());
        element.insert(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING, data["availableBalance"].GetString());
        elementList.emplace_back(std::move(element));
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : data.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_SETTLE_ASSET, x["settleCurrency"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["currentQty"].GetString());
          element.insert(CCAPI_EM_POSITION_COST, x["posCost"].GetString());
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, x["avgEntryPrice"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, x["realLeverage"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  void extractOrderInfo(Element& element, const rj::Value& x,
                        const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap,
                        const std::map<std::string_view, std::function<std::string(const std::string&)>> conversionMap = {}) override {
    ExecutionManagementServiceKucoinBase::extractOrderInfo(element, x, extractionFieldNameMap);
    {
      {
        auto it = x.FindMember("dealValue");
        if (it != x.MemberEnd()) {
          element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY, it->value.GetString());
        }
      }
      {
        auto it = x.FindMember("avgDealPrice");
        if (it != x.MemberEnd()) {
          element.insert(CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, it->value.GetString());
        }
      }
    }
  }
};

} /* namespace ccapi */
#endif
#endif
