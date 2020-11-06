#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
#ifdef ENABLE_MARKET_DATA_SERVICE
#ifdef ENABLE_COINBASE
#include "ccapi_cpp/service/ccapi_market_data_service_coinbase.h"
#endif
#ifdef ENABLE_GEMINI
#include "ccapi_cpp/service/ccapi_market_data_service_gemini.h"
#endif
#ifdef ENABLE_KRAKEN
#include "ccapi_cpp/service/ccapi_market_data_service_kraken.h"
#endif
#ifdef ENABLE_BITSTAMP
#include "ccapi_cpp/service/ccapi_market_data_service_bitstamp.h"
#endif
#ifdef ENABLE_BITFINEX
#include "ccapi_cpp/service/ccapi_market_data_service_bitfinex.h"
#endif
#ifdef ENABLE_BITMEX
#include "ccapi_cpp/service/ccapi_market_data_service_bitmex.h"
#endif
#ifdef ENABLE_BINANCE_US
#include "ccapi_cpp/service/ccapi_market_data_service_binance_us.h"
#endif
#ifdef ENABLE_BINANCE
#include "ccapi_cpp/service/ccapi_market_data_service_binance.h"
#endif
#ifdef ENABLE_BINANCE_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_binance_futures.h"
#endif
#ifdef ENABLE_HUOBI
#include "ccapi_cpp/service/ccapi_market_data_service_huobi.h"
#endif
#ifdef ENABLE_OKEX
#include "ccapi_cpp/service/ccapi_market_data_service_okex.h"
#endif
#endif
#ifdef ENABLE_EXECUTION_MANAGEMENT_SERVICE
#ifdef ENABLE_BINANCE_US
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_us.h"
#endif
#endif
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include <string>
#include <utility>
#include <vector>
#include <map>
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_event_dispatcher.h"
#include "ccapi_cpp/ccapi_event_handler.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/service/ccapi_service.h"
namespace ccapi {
class Session final {
 public:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(const SessionOptions& sessionOptions = SessionOptions(), const SessionConfigs& sessionConfigs = SessionConfigs(),
          EventHandler* eventHandler = 0, EventDispatcher* eventDispatcher = 0)
      : sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs),
        eventHandler(eventHandler),
        eventDispatcher(eventDispatcher),
        eventQueue(sessionOptions.maxEventQueueSize),
        serviceContextPtr(new ServiceContext()) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->eventHandler) {
      if (!this->eventDispatcher) {
        this->eventDispatcher = &this->defaultEventDispatcher;
      }
    } else {
      if (this->eventDispatcher) {
        throw std::runtime_error("undefined behavior");
      }
    }
    this->start();
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  ~Session() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void start() {
    if (this->eventDispatcher && this->eventDispatcher == &this->defaultEventDispatcher) {
      this->eventDispatcher->start();
    }
    std::thread t([this](){
      this->serviceContextPtr->start();
    });
    this->t = std::move(t);
    std::function<void(Event& event)> serviceEventHandler = std::bind(&Session::onEvent, this, std::placeholders::_1, &eventQueue);
    std::map<std::string, std::vector<std::string> > exchanges;
#ifdef ENABLE_MARKET_DATA_SERVICE
    exchanges[CCAPI_EXCHANGE_NAME_MARKET_DATA] = { CCAPI_EXCHANGE_NAME_COINBASE,CCAPI_EXCHANGE_NAME_GEMINI };
#endif
#ifdef ENABLE_EXECUTION_MANAGEMENT_SERVICE
    exchanges[CCAPI_EXCHANGE_NAME_EXECUTION_MANAGEMENT] = { CCAPI_EXCHANGE_NAME_BINANCE_US };
#endif
    CCAPI_LOGGER_TRACE("exchanges = "+toString(exchanges));
    for (const auto& kv : exchanges) {
      auto serviceName = kv.first;
      auto exchangeList = kv.second;
      for (const auto& exchange : exchangeList) {
        std::shared_ptr<Service> servicePtr(nullptr);
        CCAPI_LOGGER_TRACE("serviceName = "+serviceName);
        CCAPI_LOGGER_TRACE("exchange = "+exchange);
#ifdef ENABLE_MARKET_DATA_SERVICE
        if (serviceName == CCAPI_EXCHANGE_NAME_MARKET_DATA) {
#ifdef ENABLE_COINBASE
          if (exchange == CCAPI_EXCHANGE_NAME_COINBASE) {
            servicePtr = std::make_shared<MarketDataServiceCoinbase>(serviceEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
          }
#endif
#ifdef ENABLE_GEMINI
          if (exchange == CCAPI_EXCHANGE_NAME_GEMINI) {
            servicePtr = std::make_shared<MarketDataServiceGemini>(serviceEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
          }
#endif
        }
#endif
#ifdef ENABLE_EXECUTION_MANAGEMENT_SERVICE
        if (serviceName == CCAPI_EXCHANGE_NAME_EXECUTION_MANAGEMENT) {
#ifdef ENABLE_BINANCE_US
          if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_US) {
            servicePtr = std::make_shared<ExecutionManagementServiceBinanceUs>(serviceEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
          }
#endif
        }
#endif
        if (servicePtr) {
          CCAPI_LOGGER_TRACE("add service "+serviceName+" and exchange "+exchange);
          this->serviceByServiceNameExchangeMap[serviceName][exchange] = servicePtr;
        }
      }
    }
  }
  void stop() {
    if (this->eventDispatcher && this->eventDispatcher == &this->defaultEventDispatcher) {
      this->eventDispatcher->stop();
    }
    for (const auto & x : this->serviceByServiceNameExchangeMap) {
      for (const auto & y : x.second) {
        y.second->stop();
      }
    }
    this->serviceContextPtr->stop();
    this->t.join();
  }
//  bool openService(std::string serviceName = "") {
//    CCAPI_LOGGER_FUNCTION_ENTER;
//    if (serviceName.empty()) {
//      this->serviceName = this->sessionOptions.defaultSubscriptionService;
//    }
//    if (this->serviceName != this->sessionOptions.defaultSubscriptionService) {
//      CCAPI_LOGGER_ERROR("please enable service: " + this->serviceName);
//    }
//    CCAPI_LOGGER_FUNCTION_EXIT;
//    return true;
//  }
  void subscribe(const std::vector<Subscription>& subscriptionList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::map<std::string, std::vector<Subscription> > subscriptionListByServiceNameMap;
    for (const auto & subscription : subscriptionList) {
      auto serviceName = subscription.getServiceName();
      subscriptionListByServiceNameMap[serviceName].push_back(subscription);
    }
    for (const auto & x : subscriptionListByServiceNameMap) {
      auto serviceName = x.first;
      auto subscriptionList = x.second;
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        CCAPI_LOGGER_ERROR("please enable service: "+serviceName+", and the exchanges that you want");
        return;
      }
      if (serviceName == CCAPI_EXCHANGE_NAME_MARKET_DATA) {
        std::set<std::string> correlationIdSet;
        std::set<std::string> duplicateCorrelationIdSet;
    //    std::unordered_set<std::string> unsupportedExchangeInstrumentSet;
        std::unordered_set<std::string> unsupportedExchangeFieldSet;
        std::map<std::string, std::vector<Subscription> > subscriptionListByExchangeMap;
        std::unordered_set<std::string> unsupportedExchangeMarketDepthSet;
        auto exchangeInstrumentMap = this->sessionConfigs.getExchangeInstrumentMap();
        CCAPI_LOGGER_DEBUG("exchangeInstrumentMap = "+toString(exchangeInstrumentMap));
        auto exchangeFieldMap = this->sessionConfigs.getExchangeFieldMap();
        CCAPI_LOGGER_DEBUG("exchangeFieldMap = "+toString(exchangeFieldMap));
        for (const auto & subscription : subscriptionList) {
          auto correlationId = subscription.getCorrelationId();
          if (correlationIdSet.find(correlationId) != correlationIdSet.end()) {
            duplicateCorrelationIdSet.insert(correlationId);
          } else {
            correlationIdSet.insert(correlationId);
          }
          auto exchange = subscription.getExchange();
          CCAPI_LOGGER_DEBUG("exchange = "+exchange);
    //      auto instrument = subscription.getInstrument();
    //      CCAPI_LOGGER_DEBUG("instrument = "+instrument);
          auto field = subscription.getField();
          auto optionMap = subscription.getOptionMap();
    //      if (exchangeInstrumentMap.find(exchange) == exchangeInstrumentMap.end()
    //          || std::find(exchangeInstrumentMap.find(exchange)->second.begin(), exchangeInstrumentMap.find(exchange)->second.end(),
    //                       instrument) == exchangeInstrumentMap.find(exchange)->second.end()) {
    //        unsupportedExchangeInstrumentSet.insert(exchange + "|" + instrument);
    //      }
    //      for (auto & field : fieldSet) {
            CCAPI_LOGGER_DEBUG("field = "+field);
            if (exchangeFieldMap.find(exchange) == exchangeFieldMap.end()
                || std::find(exchangeFieldMap.find(exchange)->second.begin(), exchangeFieldMap.find(exchange)->second.end(),
                             field) == exchangeFieldMap.find(exchange)->second.end()) {
              unsupportedExchangeFieldSet.insert(exchange + "|" + field);
            }
            if (field == CCAPI_EXCHANGE_NAME_MARKET_DEPTH) {
              auto depth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
              if (((exchange == CCAPI_EXCHANGE_NAME_KRAKEN || exchange == CCAPI_EXCHANGE_NAME_BITSTAMP || exchange == CCAPI_EXCHANGE_NAME_BITFINEX || exchange == CCAPI_EXCHANGE_NAME_HUOBI || exchange == CCAPI_EXCHANGE_NAME_OKEX)
                  && depth > this->sessionConfigs.getWebsocketAvailableMarketDepth().at(exchange).back())
    //              || (exchange == CCAPI_EXCHANGE_NAME_BITSTAMP
    //                  && depth > this->sessionConfigs.getWebsocketMaxAvailableMarketDepth().at(exchange))
    //              || (exchange == CCAPI_EXCHANGE_NAME_BITFINEX
    //                  && depth > this->sessionConfigs.getWebsocketAvailableMarketDepth().at(exchange).back())
                      ) {
                unsupportedExchangeMarketDepthSet.insert(exchange + "|" + toString(depth));
              }
            }
    //      }
          subscriptionListByExchangeMap[exchange].push_back(subscription);
        }
        if (!duplicateCorrelationIdSet.empty()) {
          CCAPI_LOGGER_ERROR("duplicated correlation ids: " + toString(duplicateCorrelationIdSet));
          return;
        }
    //    if (!unsupportedExchangeInstrumentSet.empty()) {
    //      CCAPI_LOGGER_ERROR("unsupported exchange instruments: " + toString(unsupportedExchangeInstrumentSet));
    //    }
        if (!unsupportedExchangeFieldSet.empty()) {
          CCAPI_LOGGER_ERROR("unsupported exchange fields: " + toString(unsupportedExchangeFieldSet));
          return;
        }
        if (!unsupportedExchangeMarketDepthSet.empty()) {
          CCAPI_LOGGER_ERROR(
              "unsupported exchange market depth: " + toString(unsupportedExchangeMarketDepthSet)
                  + ", exceeded max market depth available");
          return;
        }
        CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = "+toString(subscriptionListByExchangeMap));
        for (auto & subscriptionListByExchange : subscriptionListByExchangeMap) {
          auto exchange = subscriptionListByExchange.first;
          auto subscriptionList = subscriptionListByExchange.second;
          std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
          if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
            CCAPI_LOGGER_ERROR("please enable exchange: "+exchange);
            return;
          }
          serviceByExchangeMap.at(exchange)->subscribe(subscriptionList);
        }
      }
      if (serviceName == CCAPI_EXCHANGE_NAME_EXECUTION_MANAGEMENT) {
        // TODO(cryptochassis): implement
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onEvent(Event& event, Queue<Event> *eventQueue) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("event = "+toString(event));
    if (this->eventHandler && !eventQueue) {
      CCAPI_LOGGER_TRACE("handle event asynchronously");
      this->eventDispatcher->dispatch([&, event] {
        bool shouldContinue = true;
        try {
          shouldContinue = this->eventHandler->processEvent(event, this);
        } catch(const std::runtime_error& e) {
          CCAPI_LOGGER_ERROR(e.what());
        }
        if (!shouldContinue) {
          CCAPI_LOGGER_DEBUG("about to pause the event dispatcher");
          this->eventDispatcher->pause();
        }
      });
    } else {
      CCAPI_LOGGER_TRACE("handle event synchronously");
      if (eventQueue) {
        eventQueue->pushBack(std::move(event));
      } else {
        this->eventQueue.pushBack(std::move(event));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void sendRequest(const Request& request, Queue<Event> *eventQueuePtr = 0) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<Request> requestList;
    requestList.push_back(request);
    this->sendRequest(requestList, eventQueuePtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void sendRequest(const std::vector<Request>& requestList, Queue<Event> *eventQueuePtr = 0) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<std::shared_ptr<std::future<void> > > futurePtrList;
    std::set<std::string> serviceNameExchangeSet;
    for (const auto& request : requestList) {
      auto serviceName = request.getServiceName();
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        CCAPI_LOGGER_ERROR("please enable service: "+serviceName+", and the exchanges that you want");
        return;
      }
      std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
      auto exchange = request.getExchange();
      if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
        CCAPI_LOGGER_ERROR("please enable exchange: "+exchange);
        return;
      }
      std::shared_ptr<Service>& servicePtr = serviceByExchangeMap.at(exchange);
      std::string key = serviceName + exchange;
      if (eventQueuePtr && serviceNameExchangeSet.find(key) != serviceNameExchangeSet.end()) {
        servicePtr->setEventHandler(std::bind(&Session::onEvent, this, std::placeholders::_1, eventQueuePtr));
        serviceNameExchangeSet.insert(key);
      }
      auto now = std::chrono::system_clock::now();
      auto futurePtr = servicePtr->sendRequest(request, !!eventQueuePtr, now);
      if (eventQueuePtr) {
        futurePtrList.push_back(futurePtr);
      }
    }
    if (eventQueuePtr) {
      for (auto& futurePtr : futurePtrList) {
        CCAPI_LOGGER_TRACE("before future wait");
        futurePtr->wait();
        CCAPI_LOGGER_TRACE("after future wait");
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  Queue<Event> eventQueue;

 private:
//  std::string serviceName;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  EventHandler* eventHandler;
  EventDispatcher* eventDispatcher;
  EventDispatcher defaultEventDispatcher;
  wspp::lib::shared_ptr<ServiceContext> serviceContextPtr;
  std::map<std::string, std::map<std::string, wspp::lib::shared_ptr<Service> > > serviceByServiceNameExchangeMap;
  std::thread t;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
