#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_CONFIGS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_CONFIGS_H_
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
/**
 * This class contains the configs when creating a session. It loads and maintains exchange information. Most of the values shouldn't be changed. To use
 * non-default configs on a Session, create a SessionConfigs instance and set the required configs and then supply it when creating a Session.
 */
class SessionConfigs CCAPI_FINAL {
 public:
  explicit SessionConfigs(std::map<std::string, std::string> credential = {}) : credential(credential) {
    this->updateExchange();
    this->updateExchangeRest();
    this->initializUrlFixBase();
  }
  const std::map<std::string, std::vector<std::string> >& getExchangeFieldMap() const { return exchangeFieldMap; }
  const std::map<std::string, std::map<std::string, std::string> >& getExchangeFieldWebsocketChannelMap() const { return exchangeFieldWebsocketChannelMap; }
  const std::map<std::string, std::string>& getUrlWebsocketBase() const { return urlWebsocketBase; }
  const std::map<std::string, std::string>& getUrlRestBase() const { return urlRestBase; }
  const std::map<std::string, std::string>& getUrlFixBase() const { return urlFixBase; }
  const std::map<std::string, int>& getInitialSequenceByExchangeMap() const { return initialSequenceByExchangeMap; }
  const std::map<std::string, std::string>& getCredential() const { return credential; }
  void setUrlWebsocketBase(const std::map<std::string, std::string>& urlWebsocketBase) { this->urlWebsocketBase = urlWebsocketBase; }
  void setUrlRestBase(const std::map<std::string, std::string>& urlRestBase) { this->urlRestBase = urlRestBase; }
  void setUrlFixBase(const std::map<std::string, std::string>& urlFixBase) { this->urlFixBase = urlFixBase; }
  void setCredential(const std::map<std::string, std::string>& credential) { this->credential = credential; }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void updateExchange() {
    std::map<std::string, std::string> fieldWebsocketChannelMapCoinbase = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_COINBASE_CHANNEL_MATCH},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_COINBASE_CHANNEL_LEVEL2},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapGemini = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_GEMINI_PARAMETER_TRADES},
        {CCAPI_MARKET_DEPTH, std::string(CCAPI_WEBSOCKET_GEMINI_PARAMETER_BIDS) + "," + CCAPI_WEBSOCKET_GEMINI_PARAMETER_OFFERS},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapKraken = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapKrakenFutures = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_KRAKEN_FUTURES_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_KRAKEN_FUTURES_CHANNEL_BOOK},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitstamp = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_LIVE_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitfinex = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BITFINEX_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BITFINEX_CHANNEL_BOOKS},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BITFINEX_CHANNEL_CANDLES},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitmex = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BITMEX_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBinanceUs = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_TRADE},
        {CCAPI_AGG_TRADE, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_AGG_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBinance = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_TRADE},
        {CCAPI_AGG_TRADE, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_AGG_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBinanceUsdsFutures = {
        {CCAPI_AGG_TRADE, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_AGG_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBinanceCoinFutures = {
        {CCAPI_AGG_TRADE, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_AGG_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapHuobi = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapHuobiUsdtSwap = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapHuobiCoinSwap = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapOkx = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_OKX_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH400},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_OKX_CHANNEL_CANDLESTICK},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapErisx = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_ERISX_CHANNEL_MARKET_DATA_SUBSCRIBE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_ERISX_CHANNEL_MARKET_DATA_SUBSCRIBE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapKucoin = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_MATCH},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_KLINES},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapKucoinFutures = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_KUCOIN_FUTURES_CHANNEL_MARKET_MATCH},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_KUCOIN_FUTURES_CHANNEL_MARKET_LEVEL2},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapFtx = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_ORDERBOOKS},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapFtxUs = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_ORDERBOOKS},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapDeribit = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_DERIBIT_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK_TBT},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapGateio = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_GATEIO_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_GATEIO_CHANNEL_ORDER_BOOK},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_GATEIO_CHANNEL_CANDLESTICKS},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapGateioPerpetualFutures = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_ORDER_BOOK},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_CANDLESTICKS},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapCryptocom = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_BOOK},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBybit = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BYBIT_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BYBIT_CHANNEL_ORDERBOOK},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BYBIT_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapAscendex = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_ASCENDEX_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_ASCENDEX_CHANNEL_DEPTH},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_ASCENDEX_CHANNEL_BAR},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitget = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitgetFutures = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS},
        {CCAPI_CANDLESTICK, CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_KLINE},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitmart = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_BITMART_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH5},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapMexc = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_MEXC_CHANNEL_TRADE},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_MEXC_CHANNEL_DIFF_DEPTH},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapMexcFutures = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_MEXC_FUTURES_CHANNEL_TRANSACTION},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_MEXC_FUTURES_CHANNEL_DEPTH},
    };
    std::map<std::string, std::string> fieldWebsocketChannelMapWhitebit = {
        {CCAPI_TRADE, CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES},
        {CCAPI_MARKET_DEPTH, CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_DEPTH},
    };
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapCoinbase) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_COINBASE].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketParameter : fieldWebsocketChannelMapGemini) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_GEMINI].push_back(fieldWebsocketParameter.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapKraken) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_KRAKEN].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapKrakenFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBitstamp) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BITSTAMP].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBitfinex) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BITFINEX].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBitmex) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BITMEX].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBinanceUs) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BINANCE_US].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBinance) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BINANCE].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBinanceUsdsFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBinanceCoinFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapHuobi) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_HUOBI].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapHuobiUsdtSwap) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapHuobiCoinSwap) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapOkx) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_OKX].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapErisx) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_ERISX].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapKucoin) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_KUCOIN].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapKucoinFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapFtx) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_FTX].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapFtxUs) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_FTX_US].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapDeribit) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_DERIBIT].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapGateio) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_GATEIO].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapGateioPerpetualFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapCryptocom) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_CRYPTOCOM].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBybit) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BYBIT].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapAscendex) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_ASCENDEX].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBitget) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BITGET].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBitgetFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BITGET_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapBitmart) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_BITMART].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapMexc) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_MEXC].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapMexcFutures) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_MEXC_FUTURES].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapWhitebit) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_WHITEBIT].push_back(fieldWebsocketChannel.first);
    }
    for (auto& x : this->exchangeFieldMap) {
      x.second.push_back(CCAPI_GENERIC_PUBLIC_SUBSCRIPTION);
    }
    CCAPI_LOGGER_TRACE("this->exchangeFieldMap = " + toString(this->exchangeFieldMap));
    this->exchangeFieldWebsocketChannelMap = {
        {CCAPI_EXCHANGE_NAME_COINBASE, fieldWebsocketChannelMapCoinbase},
        {CCAPI_EXCHANGE_NAME_GEMINI, fieldWebsocketChannelMapGemini},
        {CCAPI_EXCHANGE_NAME_KRAKEN, fieldWebsocketChannelMapKraken},
        {CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, fieldWebsocketChannelMapKrakenFutures},
        {CCAPI_EXCHANGE_NAME_BITSTAMP, fieldWebsocketChannelMapBitstamp},
        {CCAPI_EXCHANGE_NAME_BITFINEX, fieldWebsocketChannelMapBitfinex},
        {CCAPI_EXCHANGE_NAME_BITMEX, fieldWebsocketChannelMapBitmex},
        {CCAPI_EXCHANGE_NAME_BINANCE_US, fieldWebsocketChannelMapBinanceUs},
        {CCAPI_EXCHANGE_NAME_BINANCE, fieldWebsocketChannelMapBinance},
        {CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, fieldWebsocketChannelMapBinanceUsdsFutures},
        {CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES, fieldWebsocketChannelMapBinanceCoinFutures},
        {CCAPI_EXCHANGE_NAME_HUOBI, fieldWebsocketChannelMapHuobi},
        {CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, fieldWebsocketChannelMapHuobiUsdtSwap},
        {CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP, fieldWebsocketChannelMapHuobiCoinSwap},
        {CCAPI_EXCHANGE_NAME_OKX, fieldWebsocketChannelMapOkx},
        {CCAPI_EXCHANGE_NAME_ERISX, fieldWebsocketChannelMapErisx},
        {CCAPI_EXCHANGE_NAME_KUCOIN, fieldWebsocketChannelMapKucoin},
        {CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES, fieldWebsocketChannelMapKucoinFutures},
        {CCAPI_EXCHANGE_NAME_FTX, fieldWebsocketChannelMapFtx},
        {CCAPI_EXCHANGE_NAME_FTX_US, fieldWebsocketChannelMapFtxUs},
        {CCAPI_EXCHANGE_NAME_DERIBIT, fieldWebsocketChannelMapDeribit},
        {CCAPI_EXCHANGE_NAME_GATEIO, fieldWebsocketChannelMapGateio},
        {CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES, fieldWebsocketChannelMapGateioPerpetualFutures},
        {CCAPI_EXCHANGE_NAME_CRYPTOCOM, fieldWebsocketChannelMapCryptocom},
        {CCAPI_EXCHANGE_NAME_BYBIT, fieldWebsocketChannelMapBybit},
        {CCAPI_EXCHANGE_NAME_ASCENDEX, fieldWebsocketChannelMapAscendex},
        {CCAPI_EXCHANGE_NAME_BITGET, fieldWebsocketChannelMapBitget},
        {CCAPI_EXCHANGE_NAME_BITGET_FUTURES, fieldWebsocketChannelMapBitgetFutures},
        {CCAPI_EXCHANGE_NAME_BITMART, fieldWebsocketChannelMapBitmart},
        {CCAPI_EXCHANGE_NAME_MEXC, fieldWebsocketChannelMapMexc},
        {CCAPI_EXCHANGE_NAME_MEXC_FUTURES, fieldWebsocketChannelMapMexcFutures},
        {CCAPI_EXCHANGE_NAME_WHITEBIT, fieldWebsocketChannelMapWhitebit},
    };
    this->urlWebsocketBase = {
        {CCAPI_EXCHANGE_NAME_COINBASE, CCAPI_COINBASE_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_GEMINI, CCAPI_GEMINI_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_KRAKEN, CCAPI_KRAKEN_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, CCAPI_KRAKEN_FUTURES_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BITSTAMP, CCAPI_BITSTAMP_URL_WS_BASE},
        // Bitfinex has different urls for public and private APIs. Here it is only a placeholder for subscription grouping purposes.
        {CCAPI_EXCHANGE_NAME_BITFINEX, ""},
        {CCAPI_EXCHANGE_NAME_BITMEX, CCAPI_BITMEX_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE_US, CCAPI_BINANCE_US_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE, CCAPI_BINANCE_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, CCAPI_BINANCE_USDS_FUTURES_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES, CCAPI_BINANCE_COIN_FUTURES_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_HUOBI, CCAPI_HUOBI_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, CCAPI_HUOBI_USDT_SWAP_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP, CCAPI_HUOBI_COIN_SWAP_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_OKX, CCAPI_OKX_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_ERISX, CCAPI_ERISX_URL_WS_BASE},
        //  Kucoin has dynamic websocket url. Here it is only a placeholder for subscription grouping purposes.
        {CCAPI_EXCHANGE_NAME_KUCOIN, "CCAPI_EXCHANGE_NAME_KUCOIN_URL_WEBSOCKET_BASE"},
        //  Kucoin Futures has dynamic websocket url. Here it is only a placeholder for subscription grouping purposes.
        {CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES, "CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES_URL_WEBSOCKET_BASE"},
        {CCAPI_EXCHANGE_NAME_FTX, CCAPI_FTX_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_FTX_US, CCAPI_FTX_US_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_DERIBIT, CCAPI_DERIBIT_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_GATEIO, CCAPI_GATEIO_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES, CCAPI_GATEIO_PERPETUAL_FUTURES_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_CRYPTOCOM, CCAPI_CRYPTOCOM_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BYBIT, CCAPI_BYBIT_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_ASCENDEX, CCAPI_ASCENDEX_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BITGET, CCAPI_BITGET_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BITGET_FUTURES, CCAPI_BITGET_FUTURES_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_BITMART, CCAPI_BITMART_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_MEXC, CCAPI_MEXC_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_MEXC_FUTURES, CCAPI_MEXC_FUTURES_URL_WS_BASE},
        {CCAPI_EXCHANGE_NAME_WHITEBIT, CCAPI_WHITEBIT_URL_WS_BASE},
    };
    this->initialSequenceByExchangeMap = {{CCAPI_EXCHANGE_NAME_GEMINI, 0}, {CCAPI_EXCHANGE_NAME_BITFINEX, 1}};
  }
  void updateExchangeRest() {
    this->urlRestBase = {
        {CCAPI_EXCHANGE_NAME_COINBASE, CCAPI_COINBASE_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_GEMINI, CCAPI_GEMINI_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_KRAKEN, CCAPI_KRAKEN_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_KRAKEN_FUTURES, CCAPI_KRAKEN_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BITSTAMP, CCAPI_BITSTAMP_URL_REST_BASE},
        // Bitfinex has different urls for public and private APIs
        {CCAPI_EXCHANGE_NAME_BITFINEX, ""},
        {CCAPI_EXCHANGE_NAME_BITMEX, CCAPI_BITMEX_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE_US, CCAPI_BINANCE_US_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE, CCAPI_BINANCE_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES, CCAPI_BINANCE_USDS_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES, CCAPI_BINANCE_COIN_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_HUOBI, CCAPI_HUOBI_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_HUOBI_USDT_SWAP, CCAPI_HUOBI_USDT_SWAP_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP, CCAPI_HUOBI_COIN_SWAP_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_OKX, CCAPI_OKX_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_ERISX, CCAPI_ERISX_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_KUCOIN, CCAPI_KUCOIN_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_KUCOIN_FUTURES, CCAPI_KUCOIN_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_FTX, CCAPI_FTX_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_FTX_US, CCAPI_FTX_US_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_DERIBIT, CCAPI_DERIBIT_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_GATEIO, CCAPI_GATEIO_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES, CCAPI_GATEIO_PERPETUAL_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_CRYPTOCOM, CCAPI_CRYPTOCOM_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BYBIT, CCAPI_BYBIT_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_ASCENDEX, CCAPI_ASCENDEX_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BITGET, CCAPI_BITGET_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BITGET_FUTURES, CCAPI_BITGET_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_BITMART, CCAPI_BITMART_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_MEXC, CCAPI_MEXC_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_MEXC_FUTURES, CCAPI_MEXC_FUTURES_URL_REST_BASE},
        {CCAPI_EXCHANGE_NAME_WHITEBIT, CCAPI_WHITEBIT_URL_REST_BASE},
    };
  }
  void initializUrlFixBase() {
    this->urlFixBase = {
        {CCAPI_EXCHANGE_NAME_COINBASE, CCAPI_COINBASE_URL_FIX_BASE},
        {CCAPI_EXCHANGE_NAME_FTX, CCAPI_FTX_URL_FIX_BASE},
        {CCAPI_EXCHANGE_NAME_FTX_US, CCAPI_FTX_US_URL_FIX_BASE},
        {CCAPI_EXCHANGE_NAME_DERIBIT, CCAPI_DERIBIT_URL_FIX_BASE},
    };
  }
  std::map<std::string, std::vector<std::string> > exchangeFieldMap;
  std::map<std::string, std::map<std::string, std::string> > exchangeFieldWebsocketChannelMap;
  std::map<std::string, std::string> urlWebsocketBase;
  std::map<std::string, std::string> urlRestBase;
  std::map<std::string, std::string> urlFixBase;
  std::map<std::string, int> initialSequenceByExchangeMap;
  std::map<std::string, std::string> credential;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_CONFIGS_H_
