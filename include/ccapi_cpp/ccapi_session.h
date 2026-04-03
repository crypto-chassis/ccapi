#pragma once

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
#ifdef CCAPI_ENABLE_EXCHANGE_OKX
#include "ccapi_cpp/service/ccapi_market_data_service_okx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_market_data_service_erisx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/service/ccapi_market_data_service_kucoin.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_kucoin_futures.h"
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
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
#include "ccapi_cpp/service/ccapi_market_data_service_gateio.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_gateio_perpetual_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
#include "ccapi_cpp/service/ccapi_market_data_service_cryptocom.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/service/ccapi_market_data_service_bybit.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ASCENDEX
#include "ccapi_cpp/service/ccapi_market_data_service_ascendex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET
#include "ccapi_cpp/service/ccapi_market_data_service_bitget.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_bitget_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMART
#include "ccapi_cpp/service/ccapi_market_data_service_bitmart.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC
#include "ccapi_cpp/service/ccapi_market_data_service_mexc.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_mexc_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_WHITEBIT
#include "ccapi_cpp/service/ccapi_market_data_service_whitebit.h"
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
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_kraken_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/service/ccapi_execution_management_service_bitstamp.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
#include "ccapi_cpp/service/ccapi_execution_management_service_bitfinex.h"
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
// #ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_MARGIN
// #include "ccapi_cpp/service/ccapi_execution_management_service_binance_margin.h"
// #endif
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
#ifdef CCAPI_ENABLE_EXCHANGE_OKX
#include "ccapi_cpp/service/ccapi_execution_management_service_okx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_execution_management_service_erisx.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/service/ccapi_execution_management_service_kucoin.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_kucoin_futures.h"
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
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_gateio_perpetual_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
#include "ccapi_cpp/service/ccapi_execution_management_service_cryptocom.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/service/ccapi_execution_management_service_bybit.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ASCENDEX
#include "ccapi_cpp/service/ccapi_execution_management_service_ascendex.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET
#include "ccapi_cpp/service/ccapi_execution_management_service_bitget.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#include "ccapi_cpp/service/ccapi_execution_management_service_bitget_futures.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMART
#include "ccapi_cpp/service/ccapi_execution_management_service_bitmart.h"
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC
#include "ccapi_cpp/service/ccapi_execution_management_service_mexc.h"
#endif
// #ifdef CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
// #include "ccapi_cpp/service/ccapi_execution_management_service_mexc_futures.h"
// #endif
#ifdef CCAPI_ENABLE_EXCHANGE_WHITEBIT
#include "ccapi_cpp/service/ccapi_execution_management_service_whitebit.h"
#endif
#endif
// end: enable exchanges for execution management

// start: enable exchanges for FIX
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/service/ccapi_fix_service_binance.h"
#endif
// #ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
// #include "ccapi_cpp/service/ccapi_fix_service_coinbase.h"
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
// #include "ccapi_cpp/service/ccapi_fix_service_gemini.h"
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_FTX
// #include "ccapi_cpp/service/ccapi_fix_service_ftx.h"
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
// #include "ccapi_cpp/service/ccapi_fix_service_ftx_us.h"
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
// #include "ccapi_cpp/service/ccapi_fix_service_deribit.h"
// #endif
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

/**
 * This class provides a consumer session for making requests and subscriptions for services. Sessions manage access to services either by requests and
 * responses or subscriptions. A Session can dispatch events and replies in either an immediate or batching mode. The mode of a Session is determined when it is
 * constructed and cannot be changed subsequently. A Session is immediate if an EventHandler object is supplied when it is constructed. All incoming events are
 * delivered to the EventHandler supplied on construction. A Session is batching if an EventHandler object is not supplied when it is constructed.
 */
class Session {
 public:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  Session(const SessionOptions& sessionOptions = SessionOptions(), const SessionConfigs& sessionConfigs = SessionConfigs(),
          EventHandler* eventHandler = nullptr, EventDispatcher* eventDispatcher = nullptr
#ifndef SWIG
          ,
          ServiceContext* serviceContextPtr = nullptr
#endif
          )
      : sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs),
        eventHandler(eventHandler),
        eventDispatcher(eventDispatcher),
        eventQueue(sessionOptions.maxEventQueueSize)
#ifndef SWIG
        ,
        serviceContextPtr(serviceContextPtr)
#endif
  {
    if (!this->serviceContextPtr) {
      this->serviceContextPtr = new ServiceContext();
      this->useInternalServiceContextPtr = true;
    }
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (!this->eventHandler && this->eventDispatcher) {
      throw std::runtime_error("eventHandler is needed when eventDispatcher is provided");
    }
    this->start();
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual ~Session() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->delayTimerByIdMap.clear();
    this->serviceByServiceNameExchangeMap.clear();
    if (this->useInternalServiceContextPtr) {
      delete this->serviceContextPtr;
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void start() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->useInternalServiceContextPtr) {
      this->serviceContextPtr->start();
    }
    this->onEventFunc = std::bind(&Session::onEvent, this, std::placeholders::_1, std::placeholders::_2);
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_COINBASE] =
        std::make_shared<MarketDataServiceCoinbase>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_GEMINI] =
        std::make_shared<MarketDataServiceGemini>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KRAKEN] =
        std::make_shared<MarketDataServiceKraken>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES] =
        std::make_shared<MarketDataServiceKrakenFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITSTAMP] =
        std::make_shared<MarketDataServiceBitstamp>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITFINEX] =
        std::make_shared<MarketDataServiceBitfinex>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITMEX] =
        std::make_shared<MarketDataServiceBitmex>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_US] =
        std::make_shared<MarketDataServiceBinanceUs>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE] =
        std::make_shared<MarketDataServiceBinance>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES] =
        std::make_shared<MarketDataServiceBinanceUsdsFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES] =
        std::make_shared<MarketDataServiceBinanceCoinFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI] =
        std::make_shared<MarketDataServiceHuobi>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP] =
        std::make_shared<MarketDataServiceHuobiUsdtSwap>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP] =
        std::make_shared<MarketDataServiceHuobiCoinSwap>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_OKX] =
        std::make_shared<MarketDataServiceOkx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_ERISX] =
        std::make_shared<MarketDataServiceErisx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KUCOIN] =
        std::make_shared<MarketDataServiceKucoin>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES] =
        std::make_shared<MarketDataServiceKucoinFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_FTX] =
        std::make_shared<MarketDataServiceFtx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_FTX_US] =
        std::make_shared<MarketDataServiceFtxUs>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_DERIBIT] =
        std::make_shared<MarketDataServiceDeribit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_GATEIO] =
        std::make_shared<MarketDataServiceGateio>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES] =
        std::make_shared<MarketDataServiceGateioPerpetualFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_CRYPTOCOM] =
        std::make_shared<MarketDataServiceCryptocom>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BYBIT] =
        std::make_shared<MarketDataServiceBybit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ASCENDEX
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_ASCENDEX] =
        std::make_shared<MarketDataServiceAscendex>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITGET] =
        std::make_shared<MarketDataServiceBitget>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITGET_FUTURES] =
        std::make_shared<MarketDataServiceBitgetFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMART
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_BITMART] =
        std::make_shared<MarketDataServiceBitmart>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_MEXC] =
        std::make_shared<MarketDataServiceMexc>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_MEXC_FUTURES] =
        std::make_shared<MarketDataServiceMexcFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_WHITEBIT
    this->serviceByServiceNameExchangeMap[CCAPI_MARKET_DATA][CCAPI_EXCHANGE_NAME_WHITEBIT] =
        std::make_shared<MarketDataServiceWhitebit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#endif
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_COINBASE] =
        std::make_shared<ExecutionManagementServiceCoinbase>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_GEMINI] =
        std::make_shared<ExecutionManagementServiceGemini>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_KRAKEN] =
        std::make_shared<ExecutionManagementServiceKraken>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES] =
        std::make_shared<ExecutionManagementServiceKrakenFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITSTAMP] =
        std::make_shared<ExecutionManagementServiceBitstamp>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITFINEX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITFINEX] =
        std::make_shared<ExecutionManagementServiceBitfinex>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITMEX] =
        std::make_shared<ExecutionManagementServiceBitmex>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_US
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_US] =
        std::make_shared<ExecutionManagementServiceBinanceUs>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE] =
        std::make_shared<ExecutionManagementServiceBinance>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
// #ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_MARGIN
//     this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_MARGIN] =
//         std::make_shared<ExecutionManagementServiceBinanceMargin>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES] =
        std::make_shared<ExecutionManagementServiceBinanceUsdsFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES] =
        std::make_shared<ExecutionManagementServiceBinanceCoinFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI] =
        std::make_shared<ExecutionManagementServiceHuobi>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP] =
        std::make_shared<ExecutionManagementServiceHuobiUsdtSwap>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP] =
        std::make_shared<ExecutionManagementServiceHuobiCoinSwap>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_OKX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_OKX] =
        std::make_shared<ExecutionManagementServiceOkx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_ERISX] =
        std::make_shared<ExecutionManagementServiceErisx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_KUCOIN] =
        std::make_shared<ExecutionManagementServiceKucoin>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES] =
        std::make_shared<ExecutionManagementServiceKucoinFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_FTX] =
        std::make_shared<ExecutionManagementServiceFtx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_FTX_US] =
        std::make_shared<ExecutionManagementServiceFtxUs>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_DERIBIT] =
        std::make_shared<ExecutionManagementServiceDeribit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_GATEIO] =
        std::make_shared<ExecutionManagementServiceGateio>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES] =
        std::make_shared<ExecutionManagementServiceGateioPerpetualFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_CRYPTOCOM] =
        std::make_shared<ExecutionManagementServiceCryptocom>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BYBIT] =
        std::make_shared<ExecutionManagementServiceBybit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_ASCENDEX
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_ASCENDEX] =
        std::make_shared<ExecutionManagementServiceAscendex>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITGET] =
        std::make_shared<ExecutionManagementServiceBitget>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITGET_FUTURES] =
        std::make_shared<ExecutionManagementServiceBitgetFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_BITMART
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_BITMART] =
        std::make_shared<ExecutionManagementServiceBitmart>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC
    this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_MEXC] =
        std::make_shared<ExecutionManagementServiceMexc>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
// #ifdef CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
//     this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_MEXC_FUTURES] =
//         std::make_shared<ExecutionManagementServiceMexcFutures>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_WHITEBIT
//     this->serviceByServiceNameExchangeMap[CCAPI_EXECUTION_MANAGEMENT][CCAPI_EXCHANGE_NAME_WHITEBIT] =
//         std::make_shared<ExecutionManagementServiceWhitebit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
#endif

#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_BINANCE
    this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_BINANCE] =
        std::make_shared<FixServiceBinance>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
#endif
// #ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
//     this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_COINBASE] =
//         std::make_shared<FixServiceCoinbase>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
//     this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_GEMINI] =
//         std::make_shared<FixServiceGemini>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_FTX
//     this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_FTX] =
//         std::make_shared<FixServiceFtx>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_FTX_US
//     this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_FTX_US] =
//         std::make_shared<FixServiceFtxUs>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
// #ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
//     this->serviceByServiceNameExchangeMap[CCAPI_FIX][CCAPI_EXCHANGE_NAME_DERIBIT] =
//         std::make_shared<FixServiceDeribit>(this->onEventFunc, sessionOptions, sessionConfigs, this->serviceContextPtr);
// #endif
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
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [this]() {
      for (const auto& [_, delayTimer] : this->delayTimerByIdMap) {
        delayTimer->cancel();
      }
      if (heartbeatTimerPtr) {
        heartbeatTimerPtr->cancel();
      }
      for (const auto& x : this->serviceByServiceNameExchangeMap) {
        for (const auto& y : x.second) {
          y.second->stop();
        }
      }
    });

    if (this->useInternalServiceContextPtr) {
      this->serviceContextPtr->stop();
    }
  }

  typedef boost::system::error_code ErrorCode;

  virtual void setHeartbeatTimer(long heartbeatIntervalMilliseconds) {
    auto timerPtr =
        std::make_shared<boost::asio::steady_timer>(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(heartbeatIntervalMilliseconds));
    timerPtr->async_wait([this, heartbeatIntervalMilliseconds](ErrorCode const& ec) {
      if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
          std::string errorMessage = "heartbeat timer error: " + ec.message();
          CCAPI_LOGGER_ERROR(errorMessage);
          this->onError(Event::Type::SESSION_STATUS, Message::Type::GENERIC_ERROR, errorMessage);
        }
      } else {
        Event event;
        event.setType(Event::Type::HEARTBEAT);
        this->onEvent(event, nullptr);
        this->setHeartbeatTimer(heartbeatIntervalMilliseconds);
      }
    });
    this->heartbeatTimerPtr = timerPtr;
  }

  virtual void subscribe(Subscription& subscription) {
    std::vector<Subscription> subscriptionList;
    subscriptionList.push_back(subscription);
    this->subscribe(subscriptionList);
  }

  virtual void subscribe(std::vector<Subscription>& subscriptionList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    for (auto& subscription : subscriptionList) {
      CCAPI_LOGGER_TRACE("subscription = " + toString(subscription));
      if (subscription.getField() == CCAPI_HEARTBEAT) {
        this->setHeartbeatTimer(std::stol(subscription.getOptionMap().at(CCAPI_HEARTBEAT_INTERVAL_MILLISECONDS)));
        continue;
      }
      auto exchange = subscription.getExchange();
      if (exchange == CCAPI_EXCHANGE_NAME_BYBIT) {
        auto instrumentType = subscription.getInstrumentType();
        if (instrumentType.empty()) {
          instrumentType = "spot";
        }
        std::vector<std::string> instrumentTypeList = {"spot", "linear", "inverse", "option"};
        if (std::find(instrumentTypeList.begin(), instrumentTypeList.end(), instrumentType) == instrumentTypeList.end()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                        "unsupported exchange instrument types: " + toString(instrumentType) + ". Allowed values: " + toString(instrumentTypeList) + ".");
          return;
        }
        subscription.setInstrumentType(instrumentType);
      } else if (exchange == CCAPI_EXCHANGE_NAME_BITGET_FUTURES) {
        auto instrumentType = subscription.getInstrumentType();
        if (instrumentType.empty()) {
          instrumentType = "USDT-FUTURES";
        }
        std::vector<std::string> instrumentTypeList = {"USDT-FUTURES", "COIN-FUTURES", "USDC-FUTURES"};
        if (std::find(instrumentTypeList.begin(), instrumentTypeList.end(), instrumentType) == instrumentTypeList.end()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                        "unsupported exchange instrument types: " + toString(instrumentType) + ". Allowed values: " + toString(instrumentTypeList) + ".");
          return;
        }
        subscription.setInstrumentType(instrumentType);
      }
    }
    std::map<std::string, std::vector<Subscription>> subscriptionListByServiceNameMap;
    for (const auto& subscription : subscriptionList) {
      if (subscription.getField() == CCAPI_HEARTBEAT) {
        continue;
      }
      auto serviceName = subscription.getServiceName();
      subscriptionListByServiceNameMap[serviceName].push_back(subscription);
    }
    for (const auto& x : subscriptionListByServiceNameMap) {
      auto serviceName = x.first;
      auto subscriptionList = x.second;
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                      "please enable service: " + serviceName + ", and the exchanges that you want for subscriptionList " + toString(subscriptionList));
        return;
      }
      if (serviceName == CCAPI_MARKET_DATA) {
        std::unordered_set<std::string> unsupportedExchangeFieldSet;
        auto exchangeFieldMap = this->sessionConfigs.getExchangeFieldMap();
        CCAPI_LOGGER_DEBUG("exchangeFieldMap = " + toString(exchangeFieldMap));
        for (const auto& subscription : subscriptionList) {
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
        }
        if (!unsupportedExchangeFieldSet.empty()) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                        "unsupported exchange fields: " + toString(unsupportedExchangeFieldSet));
          return;
        }
        std::map<std::string, std::vector<Subscription>> subscriptionListByExchangeMap;
        for (const auto& subscription : subscriptionList) {
          auto exchange = subscription.getExchange();
          subscriptionListByExchangeMap[exchange].push_back(subscription);
        }
        CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = " + toString(subscriptionListByExchangeMap));
        for (auto& subscriptionListByExchange : subscriptionListByExchangeMap) {
          auto exchange = subscriptionListByExchange.first;
          auto subscriptionList = subscriptionListByExchange.second;
          std::map<std::string, std::shared_ptr<Service>>& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
          if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                          "please enable exchange: " + exchange + " for subscriptionList " + toString(subscriptionList));
            return;
          }
          serviceByExchangeMap.at(exchange)->subscribe(subscriptionList);
        }
      } else if (serviceName == CCAPI_EXECUTION_MANAGEMENT || serviceName == CCAPI_FIX) {
        std::map<std::string, std::vector<Subscription>> subscriptionListByExchangeMap;
        for (const auto& subscription : subscriptionList) {
          auto exchange = subscription.getExchange();
          subscriptionListByExchangeMap[exchange].push_back(subscription);
        }
        CCAPI_LOGGER_TRACE("subscriptionListByExchangeMap = " + toString(subscriptionListByExchangeMap));
        for (auto& subscriptionListByExchange : subscriptionListByExchangeMap) {
          auto exchange = subscriptionListByExchange.first;
          auto subscriptionList = subscriptionListByExchange.second;
          std::map<std::string, std::shared_ptr<Service>>& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
          if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
            this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE,
                          "please enable exchange: " + exchange + " for subscriptionList " + toString(subscriptionList));
            return;
          }
          serviceByExchangeMap.at(exchange)->subscribe(subscriptionList);
        }
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  //   virtual void subscribe(Subscription& subscription) {
  //     auto serviceName = subscription.getServiceName();
  //     CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
  //     if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
  //       this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE,
  //                     "please enable service: " + serviceName + ", and the exchanges that you want for subscription " + toString(subscription));
  //       return;
  //     }
  //     auto exchange = subscription.getExchange();
  //     std::map<std::string, std::shared_ptr<Service>>& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
  //     if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
  //       this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "please enable exchange: " + exchange+" for subscription " +
  //       toString(subscription)); return;
  //     }
  //     serviceByExchangeMap.at(exchange)->subscribe(subscription);
  //   }

  //   virtual void subscribe(std::vector<Subscription>& subscriptionList) {
  //     for (auto& x : subscriptionList) {
  //       this->subscribe(x);
  //     }
  //   }

  virtual void onEvent(Event& event, Queue<Event>* eventQueue) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("event = " + toString(event));
    if (eventQueue) {
      eventQueue->pushBack(std::move(event));
    } else {
      if (this->eventHandler) {
        CCAPI_LOGGER_TRACE("handle event in immediate mode");
        if (!this->eventDispatcher) {
          try {
            this->eventHandler->processEvent(event, this);
          } catch (const std::runtime_error& e) {
            CCAPI_LOGGER_ERROR(e.what());
          }
        } else {
          this->eventDispatcher->dispatch([that = this, event = std::move(event)] {
            try {
              that->eventHandler->processEvent(event, that);
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(e.what());
            }
          });
        }
      } else {
        CCAPI_LOGGER_TRACE("handle event in batching mode");
        this->eventQueue.pushBack(std::move(event));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void sendRequestByFix(const std::string& fixOrderEntrySubscriptionCorrelationId, Request& request) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto serviceName = request.getServiceName();
    CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
    if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE,
                    "please enable service: " + serviceName + ", and the exchanges that you want for request " + toString(request));
      return;
    }
    std::map<std::string, std::shared_ptr<Service>>& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
    auto exchange = request.getExchange();
    if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "please enable exchange: " + exchange);
      return;
    }
    std::shared_ptr<Service> servicePtr = serviceByExchangeMap.at(exchange);
    auto now = UtilTime::now();
    servicePtr->sendRequestByFix(fixOrderEntrySubscriptionCorrelationId, request, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void sendRequestByFix(const std::string& fixOrderEntrySubscriptionCorrelationId, std::vector<Request>& requestList) {
    for (auto& x : requestList) {
      this->sendRequestByFix(fixOrderEntrySubscriptionCorrelationId, x);
    }
  }

  virtual void sendRequestByWebsocket(const std::string& websocketOrderEntrySubscriptionCorrelationId, Request& request) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    const auto& serviceName = request.getServiceName();
    CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
    if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE,
                    "please enable service: " + serviceName + ", and the exchanges that you want for websocketOrderEntrySubscriptionCorrelationId " +
                        toString(websocketOrderEntrySubscriptionCorrelationId) + ", request = " + toString(request));
      return;
    }
    const std::map<std::string, std::shared_ptr<Service>>& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
    const auto& exchange = request.getExchange();
    if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable exchange: " + exchange);
      return;
    }
    std::shared_ptr<Service> servicePtr = serviceByExchangeMap.at(exchange);
    const auto& now = UtilTime::now();
    servicePtr->sendRequestByWebsocket(websocketOrderEntrySubscriptionCorrelationId, request, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void sendRequestByWebsocket(const std::string& websocketOrderEntrySubscriptionCorrelationId, std::vector<Request>& requestList) {
    for (auto& x : requestList) {
      this->sendRequestByWebsocket(websocketOrderEntrySubscriptionCorrelationId, x);
    }
  }

  virtual void sendRequest(Request& request, Queue<Event>* eventQueuePtr = nullptr, long delayMilliseconds = 0) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<Request> requestList({request});
    this->sendRequest(requestList, eventQueuePtr, delayMilliseconds);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void sendRequest(std::vector<Request>& requestList, Queue<Event>* eventQueuePtr = nullptr, long delayMilliseconds = 0) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    std::vector<std::shared_ptr<std::future<void>>> futurePtrList;
    // std::set<std::string> serviceNameExchangeSet;
    int i = 0;
    for (auto& request : requestList) {
      request.setIndex(i);
      const auto& serviceName = request.getServiceName();
      CCAPI_LOGGER_DEBUG("serviceName = " + serviceName);
      if (this->serviceByServiceNameExchangeMap.find(serviceName) == this->serviceByServiceNameExchangeMap.end()) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE,
                      "please enable service: " + serviceName + ", and the exchanges that you want for request " + toString(request), eventQueuePtr);
        return;
      }
      std::map<std::string, std::shared_ptr<Service>>& serviceByExchangeMap = this->serviceByServiceNameExchangeMap.at(serviceName);
      const auto& exchange = request.getExchange();
      if (serviceByExchangeMap.find(exchange) == serviceByExchangeMap.end()) {
        this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "please enable exchange: " + exchange + " for request " + toString(request),
                      eventQueuePtr);
        return;
      }
      std::shared_ptr<Service> servicePtr = serviceByExchangeMap.at(exchange);
      const std::string& key = serviceName + exchange;
      const auto& now = UtilTime::now();
      auto futurePtr = servicePtr->sendRequest(request, !!eventQueuePtr, now, delayMilliseconds, eventQueuePtr);
      if (eventQueuePtr) {
        futurePtrList.push_back(futurePtr);
      }
      ++i;
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
  virtual void setImmediate(std::function<void()> successHandler) {
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [this, successHandler]() {
      if (this->eventHandler) {
        if (!this->eventDispatcher) {
          successHandler();
        } else {
          this->eventDispatcher->dispatch([successHandler] { successHandler(); });
        }
      }
    });
  }

  virtual void setTimer(const std::string& id, long delayMilliseconds, std::function<void(const boost::system::error_code&)> errorHandler,
                        std::function<void()> successHandler) {
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [this, id, delayMilliseconds, errorHandler, successHandler]() {
      auto timerPtr = std::make_shared<boost::asio::steady_timer>(*this->serviceContextPtr->ioContextPtr, boost::asio::chrono::milliseconds(delayMilliseconds));
      timerPtr->async_wait([this, id, errorHandler, successHandler](const boost::system::error_code& ec) {
        if (this->eventHandler) {
          if (!this->eventDispatcher) {
            if (ec) {
              if (errorHandler) {
                errorHandler(ec);
              }
            } else {
              if (successHandler) {
                successHandler();
              }
            }
          } else {
            this->eventDispatcher->dispatch([ec, errorHandler, successHandler] {
              if (ec) {
                if (errorHandler) {
                  errorHandler(ec);
                }
              } else {
                if (successHandler) {
                  successHandler();
                }
              }
            });
          }
        }
        this->delayTimerByIdMap.erase(id);
      });
      this->delayTimerByIdMap[id] = timerPtr;
    });
  }

  virtual void cancelTimer(const std::string& id) {
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [this, id]() {
      if (this->delayTimerByIdMap.find(id) != this->delayTimerByIdMap.end()) {
        this->delayTimerByIdMap[id]->cancel();
        this->delayTimerByIdMap.erase(id);
      }
    });
  }

  void purgeHttpConnectionPool(const std::string& serviceName = "", const std::string& exchangeName = "") {
    for (const auto& x : this->serviceByServiceNameExchangeMap) {
      if (serviceName.empty() || serviceName == x.first) {
        for (const auto& y : x.second) {
          if (exchangeName.empty() || exchangeName == y.first) {
            y.second->purgeHttpConnectionPool();
          }
        }
      }
    }
  }

  void forceCloseWebsocketConnections(const std::string& serviceName = "", const std::string& exchangeName = "") {
    for (const auto& x : this->serviceByServiceNameExchangeMap) {
      if (serviceName.empty() || serviceName == x.first) {
        for (const auto& y : x.second) {
          if (exchangeName.empty() || exchangeName == y.first) {
            y.second->forceCloseWebsocketConnections();
          }
        }
      }
    }
  }
#endif
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  EventHandler* eventHandler{nullptr};
  EventDispatcher* eventDispatcher{nullptr};
  std::map<std::string, std::map<std::string, std::shared_ptr<Service>>> serviceByServiceNameExchangeMap;
  Queue<Event> eventQueue;
  ServiceContext* serviceContextPtr{nullptr};
  bool useInternalServiceContextPtr{};
  std::function<void(Event& event, Queue<Event>* eventQueue)> onEventFunc;
  std::map<std::string, std::shared_ptr<steady_timer>> delayTimerByIdMap;
  std::shared_ptr<steady_timer> heartbeatTimerPtr{nullptr};
};

} /* namespace ccapi */
