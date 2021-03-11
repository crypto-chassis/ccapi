#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
#include "ccapi_cpp/ccapi_macro.h"

// start: enable exchanges for market data
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/service/ccapi_market_data_service_coinbase.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_market_data_service_gemini.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_market_data_service_kraken.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/service/ccapi_market_data_service_bitstamp.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
#include "ccapi_cpp/service/ccapi_market_data_service_bitfinex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
#include "ccapi_cpp/service/ccapi_market_data_service_bitmex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
#include "ccapi_cpp/service/ccapi_market_data_service_binance_us.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/service/ccapi_market_data_service_binance.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_binance_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_market_data_service_huobi.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "ccapi_cpp/service/ccapi_market_data_service_okex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_market_data_service_erisx.h"
#endif
#endif
// end: enable exchanges for market data

// start: enable exchanges for execution management
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/service/ccapi_execution_management_service_coinbase.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_execution_management_service_gemini.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
#include "ccapi_cpp/service/ccapi_execution_management_service_bitmex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_us.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/service/ccapi_execution_management_service_binance.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_execution_management_service_erisx.h"
#endif
#endif
// end: enable exchanges for execution management

#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include <string>
#include <utility>
#include <vector>
#include <map>
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_event_dispatcher.h"
#include "ccapi_cpp/ccapi_event_handler.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/service/ccapi_service.h"
namespace ccapi {
class Session CCAPI_FINAL {
 public:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session(const SessionOptions& sessionOptions = SessionOptions(), const SessionConfigs& sessionConfigs = SessionConfigs(),
          EventHandler* eventHandler = nullptr, EventDispatcher* eventDispatcher = nullptr)
      : sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs),
        eventHandler(eventHandler),
        eventDispatcher(eventDispatcher),
        eventQueue(sessionOptions.maxEventQueueSize),
        serviceContextPtr(new ServiceContext()) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->eventHandler) {
      if (!this->eventDispatcher) {
        this->eventDispatcher = new EventDispatcher();
        this->useInternalEventDispatcher = true;
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
    std::thread t([this](){
      this->serviceContextPtr->start();
    });
    this->t = std::move(t);
    std::function<void(Event& event)> eventHandler = std::bind(&Session::onEvent, this, std::placeholders::_1, &eventQueue);
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_COINBASE] = std::make_shared<MarketDataServiceCoinbase>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_GEMINI] = std::make_shared<MarketDataServiceGemini>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KRAKEN] = std::make_shared<MarketDataServiceKraken>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITSTAMP] = std::make_shared<MarketDataServiceBitstamp>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITFINEX] = std::make_shared<MarketDataServiceBitfinex>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITMEX] = std::make_shared<MarketDataServiceBitmex>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_US] = std::make_shared<MarketDataServiceBinanceUs>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE] = std::make_shared<MarketDataServiceBinance>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_FUTURES] = std::make_shared<MarketDataServiceBinanceFutures>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI] = std::make_shared<MarketDataServiceHuobi>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_OKEX] = std::make_shared<MarketDataServiceOkex>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_ERISX] = std::make_shared<MarketDataServiceErisx>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#endif
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_COINBASE] = std::make_shared<ExecutionManagementServiceCoinbase>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_GEMINI] = std::make_shared<ExecutionManagementServiceGemini>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITMEX] = std::make_shared<ExecutionManagementServiceBitmex>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_US] = std::make_shared<ExecutionManagementServiceBinanceUs>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE] = std::make_shared<ExecutionManagementServiceBinance>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_FUTURES] = std::make_shared<ExecutionManagementServiceBinanceFutures>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI] = std::make_shared<ExecutionManagementServiceHuobi>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_ERISX] = std::make_shared<ExecutionManagementServiceErisx>(eventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#endif
    for (const auto& x : this->serviceByServiceNameExchangeMap) {
      auto serviceName = x.first;
      for (const auto& y : x.second) {
        auto exchange = y.first;
        CCAPI_LOGGER_INFO("enabled service: " + serviceName + ", exchange: " + exchange);
      }
    }
  }
  void stop() {
    if (this->useInternalEventDispatcher) {
      this->eventDispatcher->stop();
      delete this->eventDispatcher;
    }
    for (const auto & x : this->serviceByServiceNameExchangeMap) {
      for (const auto & y : x.second) {
        y.second->stop();
      }
    }
    this->serviceContextPtr->stop();
    this->t.join();
  }
  void subscribe(const Subscription& subscription) {
    std::vector<Subscription> subscriptionList;
    subscriptionList.push_back(subscription);
    this->subscribe(subscriptionList);
  }
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
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "please enable service: "+serviceName+", and the exchanges that you want");
        return;
      }
      if (serviceName == CCAPI_MARKET_DATA) {
        std::set<std::string> correlationIdSet;
        std::set<std::string> duplicateCorrelationIdSet;
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
          auto field = subscription.getField();
          auto optionMap = subscription.getOptionMap();
            CCAPI_LOGGER_DEBUG("field = "+field);
            if (exchangeFieldMap.find(exchange) == exchangeFieldMap.end()
                || std::find(exchangeFieldMap.find(exchange)->second.begin(), exchangeFieldMap.find(exchange)->second.end(),
                             field) == exchangeFieldMap.find(exchange)->second.end()) {
              CCAPI_LOGGER_DEBUG("unsupported exchange " + exchange + ", field = "+field);
              unsupportedExchangeFieldSet.insert(exchange + "|" + field);
            }
            if (field == CCAPI_MARKET_DEPTH) {
              auto depth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
              if (((exchange == CCAPI_EXCHANGE_NAME_KRAKEN || exchange == CCAPI_EXCHANGE_NAME_BITSTAMP || exchange == CCAPI_EXCHANGE_NAME_BITFINEX || exchange == CCAPI_EXCHANGE_NAME_HUOBI || exchange == CCAPI_EXCHANGE_NAME_OKEX)
                  && depth > this->sessionConfigs.getWebsocketAvailableMarketDepth().at(exchange).back())
                      ) {
                CCAPI_LOGGER_DEBUG("unsupported exchange " + exchange + ", field = "+field);
                unsupportedExchangeMarketDepthSet.insert(exchange + "|" + toString(depth));
              }
            }
          subscriptionListByExchangeMap[exchange].push_back(subscription);
        }
        if (!duplicateCorrelationIdSet.empty()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "duplicated correlation ids: " + toString(duplicateCorrelationIdSet));
          return;
        }
        if (!unsupportedExchangeFieldSet.empty()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "unsupported exchange fields: " + toString(unsupportedExchangeFieldSet));
          return;
        }
        if (!unsupportedExchangeMarketDepthSet.empty()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "unsupported exchange market depth: " + toString(unsupportedExchangeMarketDepthSet)
                                    + ", exceeded max market depth available");
          return;
        }
        CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = "+toString(subscriptionListByExchangeMap));
        for (auto & subscriptionListByExchange : subscriptionListByExchangeMap) {
          auto exchange = subscriptionListByExchange.first;
          auto subscriptionList = subscriptionListByExchange.second;
          std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
          if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "please enable exchange: " + exchange);
            return;
          }
          serviceByExchangeMap.at(exchange)->subscribe(subscriptionList);
        }
      }
      if (serviceName == CCAPI_EXECUTION_MANAGEMENT) {
        // TODO(cryptochassis): implement
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onEvent(Event& event, Queue<Event> *eventQueue) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("event = "+toString(event));
    if (this->eventHandler) {
      CCAPI_LOGGER_TRACE("handle event in immediate mode");
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
      CCAPI_LOGGER_TRACE("handle event in batching mode");
      if (eventQueue) {
        eventQueue->pushBack(std::move(event));
      } else {
        this->eventQueue.pushBack(std::move(event));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void sendRequest(const Request& request, Queue<Event> *eventQueuePtr = nullptr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<Request> requestList;
    requestList.push_back(request);
    this->sendRequest(requestList, eventQueuePtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void sendRequest(const std::vector<Request>& requestList, Queue<Event> *eventQueuePtr = nullptr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<std::shared_ptr<std::future<void> > > futurePtrList;
    std::set<std::string> serviceNameExchangeSet;
    for (const auto& request : requestList) {
      auto serviceName = request.getServiceName();
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable service: "+serviceName+", and the exchanges that you want");
        return;
      }
      std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
      auto exchange = request.getExchange();
      if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable exchange: " + exchange);
        return;
      }
      std::shared_ptr<Service>& servicePtr = serviceByExchangeMap.at(exchange);
      std::string key = serviceName + exchange;
      if (eventQueuePtr && serviceNameExchangeSet.find(key) == serviceNameExchangeSet.end()) {
        servicePtr->setEventHandler(std::bind(&Session::onEvent, this, std::placeholders::_1, eventQueuePtr));
        serviceNameExchangeSet.insert(key);
      }
      auto now = UtilTime::now();
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
  Queue<Event>& getEventQueue() {
    return eventQueue;
  }
  void onError(const Event::Type eventType, const Message::Type messageType, const std::string& errorMessage) {
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    Event event;
    event.setType(eventType);
    Message message;
    auto now = UtilTime::now();
    message.setTimeReceived(now);
    message.setTime(now);
    message.setType(messageType);
    Element element;
    element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler->processEvent(event, this);
  }

 private:
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  EventHandler* eventHandler;
  EventDispatcher* eventDispatcher;
  bool useInternalEventDispatcher{};
  wspp::lib::shared_ptr<ServiceContext> serviceContextPtr;
  std::map<std::string, std::map<std::string, wspp::lib::shared_ptr<Service> > > serviceByServiceNameExchangeMap;
  std::thread t;
  Queue<Event> eventQueue;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
