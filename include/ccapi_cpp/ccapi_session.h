#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_MARKET_DATA_SERVICE
#ifdef ENABLE_COINBASE
#include "ccapi_cpp/ccapi_market_data_service_coinbase.h"
#endif
#ifdef ENABLE_GEMINI
#include "ccapi_cpp/ccapi_market_data_service_gemini.h"
#endif
#ifdef ENABLE_KRAKEN
#include "ccapi_cpp/ccapi_market_data_service_kraken.h"
#endif
#ifdef ENABLE_BITSTAMP
#include "ccapi_cpp/ccapi_market_data_service_bitstamp.h"
#endif
#ifdef ENABLE_BITFINEX
#include "ccapi_cpp/ccapi_market_data_service_bitfinex.h"
#endif
#ifdef ENABLE_BITMEX
#include "ccapi_cpp/ccapi_market_data_service_bitmex.h"
#endif
#ifdef ENABLE_BINANCE_US
#include "ccapi_cpp/ccapi_market_data_service_binance_us.h"
#endif
#ifdef ENABLE_BINANCE
#include "ccapi_cpp/ccapi_market_data_service_binance.h"
#endif
#ifdef ENABLE_BINANCE_FUTURES
#include "ccapi_cpp/ccapi_market_data_service_binance_futures.h"
#endif
#ifdef ENABLE_HUOBI
#include "ccapi_cpp/ccapi_market_data_service_huobi.h"
#endif
#ifdef ENABLE_OKEX
#include "ccapi_cpp/ccapi_market_data_service_okex.h"
#endif
#endif
#ifdef ENABLE_EXECUTION_MANAGEMENT_SERVICE
#ifdef ENABLE_BINANCE_US
#include "ccapi_cpp/ccapi_execution_management_service_binance_us.h"
#endif
#endif
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_subscription_list.h"
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <unordered_set>
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_event_dispatcher.h"
#include "ccapi_cpp/ccapi_event_handler.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_service_context.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_service.h"
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
    if (this->eventDispatcher) {
      this->eventDispatcher->start();
    }
    std::thread t([this](){
      this->serviceContextPtr->run();
    });
    this->t = std::move(t);
    std::function<void(Event& event)> serviceEventHandler = std::bind(&Session::onEvent, this, std::placeholders::_1, &eventQueue);
    std::map<std::string, std::vector<std::string> > exchanges;
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
#ifdef ENABLE_EXECUTION_MANAGEMENT_SERVICE
        if (serviceName == CCAPI_EXCHANGE_NAME_EXECUTION_MANAGEMENT) {
#ifdef ENABLE_BINANCE_US
          if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_US) {
            servicePtr = std::make_shared<ExecutionManagementServiceBinanceUs>(serviceEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
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
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  ~Session() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->t.join();
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
//  bool openService(std::string serviceName = "") {
//    CCAPI_LOGGER_FUNCTION_ENTER;
//    if (serviceName.empty()) {
//      this->serviceName = this->sessionOptions.defaultSubscriptionService;
//    }
//    if (this->serviceName != this->sessionOptions.defaultSubscriptionService) {
//      CCAPI_LOGGER_FATAL("unsupported service: " + this->serviceName);
//    }
//    CCAPI_LOGGER_FUNCTION_EXIT;
//    return true;
//  }
#ifdef ENABLE_MARKET_DATA_SERVICE
  void subscribe(const SubscriptionList& subscriptionList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::unordered_set<CorrelationId, CorrelationIdHash> correlationIdSet;
    std::unordered_set<CorrelationId, CorrelationIdHash> duplicateCorrelationIdSet;
    std::unordered_set<std::string> unsupportedExchangeInstrumentSet;
    std::unordered_set<std::string> unsupportedExchangeFieldSet;
    std::map<std::string, SubscriptionList> subscriptionListByExchangeMap;
    std::unordered_set<std::string> unsupportedExchangeMarketDepthSet;
    auto exchangeInstrumentMap = this->sessionConfigs.getExchangeInstrumentMap();
    CCAPI_LOGGER_DEBUG("exchangeInstrumentMap = "+toString(exchangeInstrumentMap));
    auto exchangeFieldMap = this->sessionConfigs.getExchangeFieldMap();
    CCAPI_LOGGER_DEBUG("exchangeFieldMap = "+toString(exchangeFieldMap));
    for (auto & subscription : subscriptionList.getSubscriptionList()) {
      auto correlationId = subscription.getCorrelationId();
      if (correlationIdSet.find(correlationId) != correlationIdSet.end()) {
        duplicateCorrelationIdSet.insert(correlationId);
      } else {
        correlationIdSet.insert(correlationId);
      }
      auto exchange = subscription.getExchange();
      CCAPI_LOGGER_DEBUG("exchange = "+exchange);
      auto instrument = subscription.getInstrument();
      CCAPI_LOGGER_DEBUG("instrument = "+instrument);
      auto fieldSet = subscription.getFieldSet();
      auto optionMap = subscription.getOptionMap();
      if (exchangeInstrumentMap.find(exchange) == exchangeInstrumentMap.end()
          || std::find(exchangeInstrumentMap.find(exchange)->second.begin(), exchangeInstrumentMap.find(exchange)->second.end(),
                       instrument) == exchangeInstrumentMap.find(exchange)->second.end()) {
        unsupportedExchangeInstrumentSet.insert(exchange + "|" + instrument);
      }
      for (auto & field : fieldSet) {
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
      }
      subscriptionListByExchangeMap[exchange].add(subscription);
    }
    if (!duplicateCorrelationIdSet.empty()) {
      CCAPI_LOGGER_FATAL("duplicated correlation ids: " + toString(duplicateCorrelationIdSet));
    }
    if (!unsupportedExchangeInstrumentSet.empty()) {
      CCAPI_LOGGER_FATAL("unsupported exchange instruments: " + toString(unsupportedExchangeInstrumentSet));
    }
    if (!unsupportedExchangeFieldSet.empty()) {
      CCAPI_LOGGER_FATAL("unsupported exchange fields: " + toString(unsupportedExchangeFieldSet));
    }
    if (!unsupportedExchangeMarketDepthSet.empty()) {
      CCAPI_LOGGER_FATAL(
          "unsupported exchange market depth: " + toString(unsupportedExchangeMarketDepthSet)
              + ", exceeded max market depth available");
    }
    CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = "+toString(subscriptionListByExchangeMap));
    std::function<void(Event& event)> wsEventHandler = std::bind(&Session::onEvent, this, std::placeholders::_1, nullptr);
    auto sessionOptions = this->sessionOptions;
    auto sessionConfigs = this->sessionConfigs;
    CCAPI_LOGGER_TRACE("sessionOptions.enableOneIoContextPerExchange = "+toString(sessionOptions.enableOneIoContextPerExchange));
    if (sessionOptions.enableOneIoContextPerExchange) {
      std::vector<std::thread> sessionWsThreads;
      for (auto & subscriptionListByExchange : subscriptionListByExchangeMap) {
        auto exchange = subscriptionListByExchange.first;
        auto subscriptionList = subscriptionListByExchange.second;
        sessionWsThreads.push_back(std::thread([=](){
          bool found = false;
          std::shared_ptr<ServiceContext> serviceContextPtr(new ServiceContext());
//          serviceContextPtr->initialize();
          std::shared_ptr<MarketDataService> wsPtr;
#ifdef ENABLE_COINBASE
          if (exchange == CCAPI_EXCHANGE_NAME_COINBASE) {
            wsPtr = std::make_shared<MarketDataServiceCoinbase>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_GEMINI
          if (exchange == CCAPI_EXCHANGE_NAME_GEMINI) {
            wsPtr = std::make_shared<MarketDataServiceGemini>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_KRAKEN
          if (exchange == CCAPI_EXCHANGE_NAME_KRAKEN) {
            wsPtr = std::make_shared<MarketDataServiceKraken>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_BITSTAMP
          if (exchange == CCAPI_EXCHANGE_NAME_BITSTAMP) {
            wsPtr = std::make_shared<MarketDataServiceBitstamp>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_BITFINEX
          if (exchange == CCAPI_EXCHANGE_NAME_BITFINEX) {
            wsPtr = std::make_shared<MarketDataServiceBitfinex>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_BITMEX
          if (exchange == CCAPI_EXCHANGE_NAME_BITMEX) {
            wsPtr = std::make_shared<MarketDataServiceBitmex>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_BINANCE_US
          if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_US) {
            wsPtr = std::make_shared<MarketDataServiceBinanceUs>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_BINANCE
          if (exchange == CCAPI_EXCHANGE_NAME_BINANCE) {
            wsPtr = std::make_shared<MarketDataServiceBinance>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_BINANCE_FUTURES
          if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_FUTURES) {
            wsPtr = std::make_shared<MarketDataServiceBinanceFutures>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_HUOBI
          if (exchange == CCAPI_EXCHANGE_NAME_HUOBI) {
            wsPtr = std::make_shared<MarketDataServiceHuobi>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
#ifdef ENABLE_OKEX
          if (exchange == CCAPI_EXCHANGE_NAME_OKEX) {
            wsPtr = std::make_shared<MarketDataServiceOkex>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
            found = true;
          }
#endif
          if (!found) {
            CCAPI_LOGGER_ERROR("unsupported exchange: "+exchange);
            return;
          }
          wsPtr->connect();
          serviceContextPtr->run();
        }));
      }
      for (auto& sessionWsThread : sessionWsThreads) {
        sessionWsThread.join();
        CCAPI_LOGGER_TRACE("this thread has joined");
      }
    } else {
      std::shared_ptr<ServiceContext> serviceContextPtr(new ServiceContext());
//      serviceContextPtr->initialize();
      for (auto & subscriptionListByExchange : subscriptionListByExchangeMap) {
        auto exchange = subscriptionListByExchange.first;
        auto subscriptionList = subscriptionListByExchange.second;
        CCAPI_LOGGER_DEBUG("exchange = "+exchange);
        CCAPI_LOGGER_DEBUG("subscriptionList = "+toString(subscriptionList));
        std::shared_ptr<MarketDataService> wsPtr;
        bool found = false;
#ifdef ENABLE_COINBASE
        if (exchange == CCAPI_EXCHANGE_NAME_COINBASE) {
          wsPtr = std::make_shared<MarketDataServiceCoinbase>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_GEMINI
        if (exchange == CCAPI_EXCHANGE_NAME_GEMINI) {
          wsPtr = std::make_shared<MarketDataServiceGemini>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_KRAKEN
        if (exchange == CCAPI_EXCHANGE_NAME_KRAKEN) {
          wsPtr = std::make_shared<MarketDataServiceKraken>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_BITSTAMP
        if (exchange == CCAPI_EXCHANGE_NAME_BITSTAMP) {
          wsPtr = std::make_shared<MarketDataServiceBitstamp>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_BITFINEX
        if (exchange == CCAPI_EXCHANGE_NAME_BITFINEX) {
          wsPtr = std::make_shared<MarketDataServiceBitfinex>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_BITMEX
        if (exchange == CCAPI_EXCHANGE_NAME_BITMEX) {
          wsPtr = std::make_shared<MarketDataServiceBitmex>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_BINANCE_US
        if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_US) {
          wsPtr = std::make_shared<MarketDataServiceBinanceUs>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_BINANCE
        if (exchange == CCAPI_EXCHANGE_NAME_BINANCE) {
          wsPtr = std::make_shared<MarketDataServiceBinance>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_BINANCE_FUTURES
        if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_FUTURES) {
          wsPtr = std::make_shared<MarketDataServiceBinanceFutures>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_HUOBI
        if (exchange == CCAPI_EXCHANGE_NAME_HUOBI) {
          wsPtr = std::make_shared<MarketDataServiceHuobi>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
#ifdef ENABLE_OKEX
        if (exchange == CCAPI_EXCHANGE_NAME_OKEX) {
          wsPtr = std::make_shared<MarketDataServiceOkex>(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr);
          found = true;
        }
#endif
        if (!found) {
          CCAPI_LOGGER_ERROR("unsupported exchange: "+exchange);
          return;
        }
        wsPtr->connect();
      }
      serviceContextPtr->run();
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
#endif
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
    auto serviceName = request.getServiceName();
    if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
      CCAPI_LOGGER_ERROR("unsupported service: "+serviceName);
      return;
    }
    std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
    auto exchange = request.getExchange();
    if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
      CCAPI_LOGGER_ERROR("unsupported exchange: "+exchange);
      return;
    }
    std::shared_ptr<Service>& servicePtr = serviceByExchangeMap.at(exchange);
    if (eventQueuePtr) {
      servicePtr->setEventHandler(std::bind(&Session::onEvent, this, std::placeholders::_1, eventQueuePtr));
    }
    servicePtr->sendRequest(request, !!eventQueuePtr);
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
