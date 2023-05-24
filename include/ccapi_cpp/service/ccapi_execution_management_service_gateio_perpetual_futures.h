#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_PERPETUAL_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_PERPETUAL_FUTURES_H_
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
    this->apiKeyName = CCAPI_GATEIO_PERPETUAL_FUTURES_API_KEY;
    this->apiSecretName = CCAPI_GATEIO_PERPETUAL_FUTURES_API_SECRET;
    this->setupCredential({this->apiKeyName, this->apiSecretName});
    std::string prefix = "/api/v4";
    this->createOrderTarget = prefix + "/futures/{settle}/orders";
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
          element.insert(CCAPI_EM_POSITION_COST, std::to_string(std::stod(x["entry_price"].GetString()) * std::stod(x["size"].GetString())));
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
        boost::asio::post(*this->serviceContextPtr->ioContextPtr,
                          [that = shared_from_base<ExecutionManagementServiceGateioPerpetualFutures>(), subscription]() mutable {
                            auto now = UtilTime::now();
                            subscription.setTimeSent(now);
                            const auto& instrumentSet = subscription.getInstrumentSet();
                            auto it = instrumentSet.begin();
                            if (it != instrumentSet.end()) {
                              std::string settle;
                              std::string symbolId = *it;
                              if (UtilString::endsWith(symbolId, "_USD")) {
                                settle = "btc";
                              } else if (UtilString::endsWith(symbolId, "_USDT")) {
                                settle = "usdt";
                              }
                              auto credential = subscription.getCredential();
                              if (credential.empty()) {
                                credential = that->credentialDefault;
                              }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
                              WsConnection wsConnection(that->baseUrlWs + settle, "", {subscription}, credential);
                              that->prepareConnect(wsConnection);
#else
                              std::shared_ptr<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> streamPtr(nullptr);
                                try {
                                  streamPtr = that->createStream<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(that->serviceContextPtr->ioContextPtr, that->serviceContextPtr->sslContextPtr, that->hostWs);
                                } catch (const beast::error_code& ec) {
                                  CCAPI_LOGGER_TRACE("fail");
                                  that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "create stream", {subscription.getCorrelationId()});
                                  return;
                                }
                                std::shared_ptr<WsConnection> wsConnectionPtr(new WsConnection(that->baseUrlWs + settle, "", {subscription}, credential, streamPtr));
                                CCAPI_LOGGER_WARN("about to subscribe with new wsConnectionPtr " + toString(*wsConnectionPtr));
                                that->prepareConnect(wsConnectionPtr);
#endif
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_GATEIO_PERPETUAL_FUTURES_H_
