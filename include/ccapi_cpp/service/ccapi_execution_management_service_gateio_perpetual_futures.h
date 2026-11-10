#pragma once

#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio_base.h"

namespace ccapi {

class ExecutionManagementServiceGateioPerpetualFutures : public ExecutionManagementServiceGateioBase {
 public:
  ExecutionManagementServiceGateioPerpetualFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions,
                                                   SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementServiceGateioBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v4/ws/";
    this->baseUrlWsOrderEntry = sessionConfigs.getUrlWebsocketOrderEntryBase().at(this->exchangeName) + "/v4/ws/";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_GATEIO_PERPETUAL_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_GATEIO_PERPETUAL_FUTURES_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/api/v4";
    this->createOrderTarget = prefix + "/futures/{settle}/orders";
    this->amendOrderTarget = prefix + "/futures/{settle}/orders/{order_id}";
    this->cancelOrderTarget = prefix + "/futures/{settle}/orders/{order_id}";
    this->getOrderTarget = prefix + "/futures/{settle}/orders/{order_id}";
    this->getOpenOrdersTarget = prefix + "/futures/{settle}/orders";
    this->cancelOpenOrdersTarget = prefix + "/futures/{settle}/orders";
    this->getAccountsTarget = prefix + "/futures/{settle}/accounts";
    this->getAccountPositionsTarget = prefix + "/futures/{settle}/positions";
    this->symbolName = "contract";
    this->websocketChannelUserTrades = "futures.usertrades";
    this->websocketChannelOrders = "futures.orders";
    this->isDerivatives = true;
    this->amountName = "size";
  }

  virtual ~ExecutionManagementServiceGateioPerpetualFutures() {}

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        this->prepareReq(req, now, credential);
        req.method(http::verb::get);
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        std::string queryString;
        this->appendParam(queryString, param,
                          {
                              {CCAPI_EM_ASSET, "currency"},
                          });
        if (!queryString.empty()) {
          queryString.pop_back();
        }
        this->signRequest(req, this->getAccountsTarget, queryString, "", credential);
      } break;
      default:
        ExecutionManagementServiceGateioBase::convertRequestForRest(req, request, now, symbolId, credential);
    }
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rj::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["contract"].GetString());
          element.insert(CCAPI_EM_POSITION_QUANTITY, x["size"].GetString());
          element.insert(
              CCAPI_EM_POSITION_COST,
              ConvertDecimalToString(Decimal(UtilString::printDoubleScientific(std::stod(x["entry_price"].GetString()) * std::stod(x["size"].GetString())))));
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        ExecutionManagementServiceGateioBase::extractAccountInfoFromRequest(elementList, request, operation, document);
    }
  }

  void subscribe(std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrlWs = " + this->baseUrlWs);
    if (this->shouldContinue.load()) {
      for (auto& subscription : subscriptionList) {
        boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<ExecutionManagementServiceGateioPerpetualFutures>(),
                                                                   subscription]() mutable {
          auto now = UtilTime::now();
          subscription.setTimeSent(now);
          const auto& instrumentSet = subscription.getInstrumentSet();
          auto it = instrumentSet.begin();
          const auto& fieldSet = subscription.getFieldSet();
          const bool isOrderEntry = fieldSet.find(CCAPI_EM_WEBSOCKET_ORDER_ENTRY) != fieldSet.end();
          const bool isAccountUpdate = fieldSet.find(CCAPI_EM_BALANCE_UPDATE) != fieldSet.end() || fieldSet.find(CCAPI_EM_POSITION_UPDATE) != fieldSet.end();
          if (it != instrumentSet.end() || isOrderEntry || isAccountUpdate) {
            std::string settle = isOrderEntry || isAccountUpdate ? "usdt" : "";
            std::string symbolId = it != instrumentSet.end() ? *it : "";
            if (UtilString::endsWith(symbolId, "_USD")) {
              settle = "btc";
            } else if (UtilString::endsWith(symbolId, "_USDT")) {
              settle = "usdt";
            } else if (UtilString::endsWith(symbolId, "_USD1")) {
              settle = "usd1";
            }
            auto settleOption = mapGetWithDefault(subscription.getOptionMap(), std::string(CCAPI_SETTLE_ASSET));
            if (!settleOption.empty()) {
              settle = UtilString::toLower(settleOption);
              if (settle != "btc" && settle != "usdt" && settle != "usd1") {
                that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "Gate.io SETTLE_ASSET must be btc, usdt, or usd1",
                              {subscription.getCorrelationId()});
                return;
              }
            }
            auto credential = subscription.getCredential();
            if (credential.empty()) {
              credential = that->credentialDefault;
            }
            const auto& proxyUrl = subscription.getProxyUrl();

            auto wsConnectionPtr = std::make_shared<WsConnection>((isOrderEntry ? that->baseUrlWsOrderEntry : that->baseUrlWs) + settle, "",
                                                                  std::vector<Subscription>{subscription}, credential, proxyUrl);
            that->setWsConnectionStream(wsConnectionPtr);
            CCAPI_LOGGER_WARN("about to subscribe with new wsConnectionPtr " + toString(*wsConnectionPtr));
            that->prepareConnect(wsConnectionPtr);
          }
        });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
};

} /* namespace ccapi */
#endif
#endif
