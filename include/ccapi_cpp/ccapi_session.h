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
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_kraken_futures.h"
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
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_binance_usds_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_binance_coin_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_market_data_service_huobi.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#include "ccapi_cpp/service/ccapi_market_data_service_huobi_usdt_swap.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#include "ccapi_cpp/service/ccapi_market_data_service_huobi_coin_swap.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "ccapi_cpp/service/ccapi_market_data_service_okex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_market_data_service_erisx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/service/ccapi_market_data_service_kucoin.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_market_data_service_ftx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
#include "ccapi_cpp/service/ccapi_market_data_service_ftx_us.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/service/ccapi_market_data_service_deribit.h"
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
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_execution_management_service_kraken.h"
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
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_usds_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_coin_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_usdt_swap.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#include "ccapi_cpp/service/ccapi_execution_management_service_huobi_coin_swap.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "ccapi_cpp/service/ccapi_execution_management_service_okex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_execution_management_service_erisx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/service/ccapi_execution_management_service_kucoin.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_execution_management_service_ftx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
#include "ccapi_cpp/service/ccapi_execution_management_service_ftx_us.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/service/ccapi_execution_management_service_deribit.h"
#endif
#endif
// end: enable exchanges for execution management

// start: enable exchanges for FIX
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/service/ccapi_fix_service_coinbase.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_fix_service_gemini.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_fix_service_ftx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
#include "ccapi_cpp/service/ccapi_fix_service_ftx_us.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/service/ccapi_fix_service_deribit.h"
#endif
#endif
// end: enable exchanges for FIX

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_event_dispatcher.h"
#include "ccapi_cpp/ccapi_event_handler.h"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/service/ccapi_service.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
using steady_timer = boost::asio::steady_timer;
namespace ccapi {
class Session {
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
  virtual ~Session() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    delete this->eventDispatcher;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void start() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::thread t([this]() { this->serviceContextPtr->start(); });
    this->t = std::move(t);
    this->internalEventHandler = std::bind(&Session::onEvent, this, std::placeholders::_1, std::placeholders::_2);
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_COINBASE] =
        std::make_shared<MarketDataServiceCoinbase>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_GEMINI] =
        std::make_shared<MarketDataServiceGemini>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KRAKEN] =
        std::make_shared<MarketDataServiceKraken>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES] =
        std::make_shared<MarketDataServiceKrakenFutures>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITSTAMP] =
        std::make_shared<MarketDataServiceBitstamp>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITFINEX] =
        std::make_shared<MarketDataServiceBitfinex>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITMEX] =
        std::make_shared<MarketDataServiceBitmex>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_US] =
        std::make_shared<MarketDataServiceBinanceUs>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE] =
        std::make_shared<MarketDataServiceBinance>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES] =
        std::make_shared<MarketDataServiceBinanceUsdsFutures>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES] =
        std::make_shared<MarketDataServiceBinanceCoinFutures>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI] =
        std::make_shared<MarketDataServiceHuobi>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP] =
        std::make_shared<MarketDataServiceHuobiUsdtSwap>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP] =
        std::make_shared<MarketDataServiceHuobiCoinSwap>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_OKEX] =
        std::make_shared<MarketDataServiceOkex>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_ERISX] =
        std::make_shared<MarketDataServiceErisx>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KUCOIN] =
        std::make_shared<MarketDataServiceKucoin>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_FTX] =
        std::make_shared<MarketDataServiceFtx>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_FTX_US] =
        std::make_shared<MarketDataServiceFtxUs>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_DERIBIT] =
        std::make_shared<MarketDataServiceDeribit>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#endif
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_COINBASE] =
        std::make_shared<ExecutionManagementServiceCoinbase>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_GEMINI] =
        std::make_shared<ExecutionManagementServiceGemini>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_KRAKEN] =
        std::make_shared<ExecutionManagementServiceKraken>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITMEX] =
        std::make_shared<ExecutionManagementServiceBitmex>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_US] =
        std::make_shared<ExecutionManagementServiceBinanceUs>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE] =
        std::make_shared<ExecutionManagementServiceBinance>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES] =
        std::make_shared<ExecutionManagementServiceBinanceUsdsFutures>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES] =
        std::make_shared<ExecutionManagementServiceBinanceCoinFutures>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI] =
        std::make_shared<ExecutionManagementServiceHuobi>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP] =
        std::make_shared<ExecutionManagementServiceHuobiUsdtSwap>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP] =
        std::make_shared<ExecutionManagementServiceHuobiCoinSwap>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_OKEX] =
        std::make_shared<ExecutionManagementServiceOkex>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_ERISX] =
        std::make_shared<ExecutionManagementServiceErisx>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_KUCOIN] =
        std::make_shared<ExecutionManagementServiceKucoin>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_FTX] =
        std::make_shared<ExecutionManagementServiceFtx>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_FTX_US] =
        std::make_shared<ExecutionManagementServiceFtxUs>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_DERIBIT] =
        std::make_shared<ExecutionManagementServiceDeribit>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#endif
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_COINBASE] =
        std::make_shared<FixServiceCoinbase>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_GEMINI] =
        std::make_shared<FixServiceGemini>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
    this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_FTX] =
        std::make_shared<FixServiceFtx>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
    this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_FTX_US] =
        std::make_shared<FixServiceFtxUs>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
    this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_DERIBIT] =
        std::make_shared<FixServiceDeribit>(this->internalEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#endif
    for (const auto& x : this->serviceByServiceNameExchangeMap) {
      auto serviceName = x.first;
      for (const auto& y : x.second) {
        auto exchange = y.first;
        CCAPI_LOGGER_INFO("enabled service: " + serviceName + ", exchange: " + exchange);
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void stop() {
    if (this->useInternalEventDispatcher) {
      this->eventDispatcher->stop();
    }
    for (const auto& x : this->serviceByServiceNameExchangeMap) {
      for (const auto& y : x.second) {
        y.second->stop();
      }
    }
    this->serviceContextPtr->stop();
    this->t.join();
  }
  virtual void subscribe(Subscription& subscription) {
    std::vector<Subscription> subscriptionList;
    subscriptionList.push_back(subscription);
    this->subscribe(subscriptionList);
  }
  virtual void subscribe(std::vector<Subscription>& subscriptionList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::map<std::string, std::vector<Subscription> > subscriptionListByServiceNameMap;
    for (const auto& subscription : subscriptionList) {
      auto serviceName = subscription.getServiceName();
      subscriptionListByServiceNameMap[serviceName].push_back(subscription);
    }
    for (const auto& x : subscriptionListByServiceNameMap) {
      auto serviceName = x.first;
      auto subscriptionList = x.second;
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                      "please enable service: " + serviceName + ", and the exchanges that you want");
        return;
      }
      if (serviceName == CCAPI_MARKET_DATA) {
        std::set<std::string> correlationIdSet;
        std::set<std::string> duplicateCorrelationIdSet;
        std::unordered_set<std::string> unsupportedExchangeFieldSet;
        std::map<std::string, std::vector<Subscription> > subscriptionListByExchangeMap;
        auto exchangeFieldMap = this->sessionConfigs.getExchangeFieldMap();
        CCAPI_LOGGER_DEBUG("exchangeFieldMap = " + toString(exchangeFieldMap));
        for (const auto& subscription : subscriptionList) {
          auto correlationId = subscription.getCorrelationId();
          if (correlationIdSet.find(correlationId) != correlationIdSet.end()) {
            duplicateCorrelationIdSet.insert(correlationId);
          } else {
            correlationIdSet.insert(correlationId);
          }
          auto exchange = subscription.getExchange();
          CCAPI_LOGGER_DEBUG("exchange = " + exchange);
          auto field = subscription.getField();
          auto optionMap = subscription.getOptionMap();
          CCAPI_LOGGER_DEBUG("field = " + field);
          if (exchangeFieldMap.find(exchange) == exchangeFieldMap.end() ||
              std::find(exchangeFieldMap.find(exchange)->second.begin(), exchangeFieldMap.find(exchange)->second.end(), field) ==
                  exchangeFieldMap.find(exchange)->second.end()) {
            CCAPI_LOGGER_DEBUG("unsupported exchange " + exchange + ", field = " + field);
            unsupportedExchangeFieldSet.insert(exchange + "|" + field);
          }
          subscriptionListByExchangeMap[exchange].push_back(subscription);
        }
        if (!duplicateCorrelationIdSet.empty()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                        "duplicated correlation ids: " + toString(duplicateCorrelationIdSet));
          return;
        }
        if (!unsupportedExchangeFieldSet.empty()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                        "unsupported exchange fields: " + toString(unsupportedExchangeFieldSet));
          return;
        }
        CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = " + toString(subscriptionListByExchangeMap));
        for (auto& subscriptionListByExchange : subscriptionListByExchangeMap) {
          auto exchange = subscriptionListByExchange.first;
          auto subscriptionList = subscriptionListByExchange.second;
          std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
          if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "please enable exchange: " + exchange);
            return;
          }
          serviceByExchangeMap.at(exchange)->subscribe(subscriptionList);
        }
      } else if (serviceName == CCAPI_EXECUTION_MANAGEMENT) {
        std::map<std::string, std::vector<Subscription> > subscriptionListByExchangeMap;
        for (const auto& subscription : subscriptionList) {
          auto exchange = subscription.getExchange();
          subscriptionListByExchangeMap[exchange].push_back(subscription);
        }
        CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = " + toString(subscriptionListByExchangeMap));
        for (auto& subscriptionListByExchange : subscriptionListByExchangeMap) {
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
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void subscribeByFix(Subscription& subscription) {
    auto serviceName = subscription.getServiceName();
    CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
    if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "please enable service: " + serviceName + ", and the exchanges that you want");
      return;
    }
    auto exchange = subscription.getExchange();
    std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
    if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "please enable exchange: " + exchange);
      return;
    }
    serviceByExchangeMap.at(exchange)->subscribeByFix(subscription);
  }
  virtual void subscribeByFix(std::vector<Subscription>& subscriptionList) {
    for (auto& x : subscriptionList) {
      this->subscribeByFix(x);
    }
  }
  virtual void onEvent(Event& event, Queue<Event>* eventQueue) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("event = " + toString(event));
    if (eventQueue){
      eventQueue->pushBack(std::move(event));
    } else {
      if (this->eventHandler) {
        CCAPI_LOGGER_TRACE("handle event in immediate mode");
        this->eventDispatcher->dispatch([that = this, event = std::move(event)] {
          bool shouldContinue = true;
          try {
            shouldContinue = that->eventHandler->processEvent(event, that);
          } catch (const std::runtime_error& e) {
            CCAPI_LOGGER_ERROR(e.what());
          }
          if (!shouldContinue) {
            CCAPI_LOGGER_DEBUG("about to pause the event dispatcher");
            that->eventDispatcher->pause();
          }
        });
      } else {
        CCAPI_LOGGER_TRACE("handle event in batching mode");
          this->eventQueue.pushBack(std::move(event));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void sendRequestByFix(Request& request) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto serviceName = request.getServiceName();
    CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
    if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "please enable service: " + serviceName + ", and the exchanges that you want");
      return;
    }
    std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
    auto exchange = request.getExchange();
    if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "please enable exchange: " + exchange);
      return;
    }
    std::shared_ptr<Service>& servicePtr = serviceByExchangeMap.at(exchange);
    auto now = UtilTime::now();
    servicePtr->sendRequestByFix(request, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void sendRequestByFix(std::vector<Request>& requestList) {
    for (auto& x : requestList) {
      this->sendRequestByFix(x);
    }
  }
  virtual void sendRequestByWebsocket(Request& request) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto serviceName = request.getServiceName();
    CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
    if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable service: " + serviceName + ", and the exchanges that you want");
      return;
    }
    std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
    auto exchange = request.getExchange();
    if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable exchange: " + exchange);
      return;
    }
    std::shared_ptr<Service>& servicePtr = serviceByExchangeMap.at(exchange);
    auto now = UtilTime::now();
    servicePtr->sendRequestByWebsocket(request, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void sendRequestByWebsocket(std::vector<Request>& requestList) {
    for (auto& x : requestList) {
      this->sendRequestByWebsocket(x);
    }
  }
  virtual void sendRequest(Request& request, Queue<Event>* eventQueuePtr = nullptr, long delayMilliSeconds = 0) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<Request> requestList({request});
    this->sendRequest(requestList, eventQueuePtr, delayMilliSeconds);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void sendRequest(std::vector<Request>& requestList, Queue<Event>* eventQueuePtr = nullptr, long delayMilliSeconds = 0) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<std::shared_ptr<std::future<void> > > futurePtrList;
    // std::set<std::string> serviceNameExchangeSet;
    for (auto& request : requestList) {
      auto serviceName = request.getServiceName();
      CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE,
                      "please enable service: " + serviceName + ", and the exchanges that you want", eventQueuePtr);
        return;
      }
      std::map<std::string, wspp::lib::shared_ptr<Service> >& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
      auto exchange = request.getExchange();
      if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable exchange: " + exchange, eventQueuePtr);
        return;
      }
      std::shared_ptr<Service>& servicePtr = serviceByExchangeMap.at(exchange);
      std::string key = serviceName + exchange;
      // if (eventQueuePtr && serviceNameExchangeSet.find(key) == serviceNameExchangeSet.end()) {
      //   // servicePtr->setEventHandler(std::bind(&Session::onEvent, this, std::placeholders::_1, eventQueuePtr));
      //   serviceNameExchangeSet.insert(key);
      // }
      auto now = UtilTime::now();
      auto futurePtr = servicePtr->sendRequest(request, !!eventQueuePtr, now, delayMilliSeconds, eventQueuePtr);
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
  virtual Queue<Event>& getEventQueue() { return eventQueue; }
  virtual void onError(const Event::Type eventType, const Message::Type messageType, const std::string& errorMessage, Queue<Event>* eventQueuePtr = nullptr) {
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
    this->onEvent(event, eventQueuePtr);
  }
#ifndef SWIG
  virtual void setTimer(const std::string& id, long delayMilliSeconds, std::function<void(const boost::system::error_code&)> errorHandler,
                        std::function<void()> successHandler) {
    wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [this, id, delayMilliSeconds, errorHandler, successHandler]() {
      std::shared_ptr<steady_timer> timerPtr(new steady_timer(*this->serviceContextPtr->ioContextPtr, boost::asio::chrono::milliseconds(delayMilliSeconds)));
      timerPtr->async_wait([this, id, errorHandler, successHandler](const boost::system::error_code& ec) {
        if (this->eventHandler) {
          this->eventDispatcher->dispatch([ec, errorHandler, successHandler] {
            if (ec) {
              errorHandler(ec);
            } else {
              successHandler();
            }
          });
        }
        this->delayTimerByIdMap.erase(id);
      });
      this->delayTimerByIdMap[id] = timerPtr;
    });
  }
  virtual void cancelTimer(const std::string& id) {
    wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [this, id]() {
      if (this->delayTimerByIdMap.find(id) != this->delayTimerByIdMap.end()) {
        this->delayTimerByIdMap[id]->cancel();
        this->delayTimerByIdMap.erase(id);
      }
    });
  }
#endif
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  EventHandler* eventHandler;
  EventDispatcher* eventDispatcher;
  bool useInternalEventDispatcher{};
  wspp::lib::shared_ptr<ServiceContext> serviceContextPtr;
  std::map<std::string, std::map<std::string, wspp::lib::shared_ptr<Service> > > serviceByServiceNameExchangeMap;
  std::thread t;
  Queue<Event> eventQueue;
  std::function<void(Event& event, Queue<Event>* eventQueue)> internalEventHandler;
  std::map<std::string, std::shared_ptr<steady_timer> > delayTimerByIdMap;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_H_
