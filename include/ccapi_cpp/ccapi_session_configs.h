#ifndef INCLUDE_CCAPI_CPP_CCAPI_SESSION_CONFIGS_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SESSION_CONFIGS_H_
#include <string>
#include <map>
#include <vector>
#include <set>
#include "ccapi_cpp/ccapi_exchange.h"
#include "ccapi_cpp/ccapi_logger.h"
namespace ccapi {
class SessionConfigs final {
 public:
  SessionConfigs() {
  }
  explicit SessionConfigs(std::map<std::string, std::map<std::string, std::string> > exchangePairSymbolMap)
      : exchangePairSymbolMap(exchangePairSymbolMap) {
    this->update();
  }
  const std::map<std::string, std::map<std::string, std::string> >& getExchangePairSymbolMap() const {
    return exchangePairSymbolMap;
  }
  void setExchangePairSymbolMap(
      const std::map<std::string, std::map<std::string, std::string> >& exchangePairSymbolMap) {
    this->exchangePairSymbolMap = exchangePairSymbolMap;
    this->update();
  }
  const std::map<std::string, std::vector<std::string> >& getExchangePairMap() const {
    return exchangePairMap;
  }
  const std::map<std::string, std::vector<std::string> >& getExchangeFieldMap() const {
    return exchangeFieldMap;
  }
  const std::map<std::string, std::map<std::string, std::string> >& getExchangeFieldWebsocketChannelMap() const {
    return exchangeFieldWebsocketChannelMap;
  }
  const std::map<std::string, std::vector<int> >& getWebsocketAvailableMarketDepth() const {
    return websocketAvailableMarketDepth;
  }
  const std::map<std::string, int>& getWebsocketMaxAvailableMarketDepth() const {
    return websocketMaxAvailableMarketDepth;
  }
  const std::map<std::string, std::string>& getUrlWebsocketBase() const {
    return urlWebsocketBase;
  }
  const std::map<std::string, int>& getInitialSequenceByExchangeMap() const {
    return initialSequenceByExchangeMap;
  }

 private:
  void update() {
    for (const auto & x : exchangePairSymbolMap) {
      for (const auto & y : x.second) {
        this->exchangePairMap[x.first].push_back(y.first);
      }
    }
    std::map<std::string, std::string> fieldWebsocketChannelMapCoinbase = { {
    CCAPI_EXCHANGE_NAME_LAST,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_COINBASE_CHANNEL_MATCH }, {
    CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_COINBASE_CHANNEL_LEVEL2 }, };
    std::map<std::string, std::string> fieldWebsocketChannelMapGemini =
        { {
        CCAPI_EXCHANGE_NAME_LAST,
        CCAPI_EXCHANGE_NAME_WEBSOCKET_GEMINI_PARAMETER_TRADES }, {
        CCAPI_EXCHANGE_NAME_MARKET_DEPTH, std::string(
        CCAPI_EXCHANGE_NAME_WEBSOCKET_GEMINI_PARAMETER_BIDS) + ","
            + CCAPI_EXCHANGE_NAME_WEBSOCKET_GEMINI_PARAMETER_OFFERS }, };
    std::map<std::string, std::string> fieldWebsocketChannelMapKraken = { {
    CCAPI_EXCHANGE_NAME_LAST,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_KRAKEN_CHANNEL_TRADE }, {
    CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_KRAKEN_CHANNEL_BOOK }, };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitstamp = { {
    CCAPI_EXCHANGE_NAME_LAST,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_BITSTAMP_CHANNEL_LIVE_TRADES }, {
    CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK }, };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitfinex = { {
    CCAPI_EXCHANGE_NAME_LAST,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_BITFINEX_CHANNEL_TRADES }, {
    CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_BITFINEX_CHANNEL_BOOK }, };
    std::map<std::string, std::string> fieldWebsocketChannelMapBitmex = { {
    CCAPI_EXCHANGE_NAME_LAST,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_TRADE }, {
    CCAPI_EXCHANGE_NAME_MARKET_DEPTH,
    CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2 }, };
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapCoinbase) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_COINBASE].push_back(fieldWebsocketChannel.first);
    }
    for (auto const& fieldWebsocketParameter : fieldWebsocketChannelMapGemini) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_GEMINI].push_back(fieldWebsocketParameter.first);
    }
    for (auto const& fieldWebsocketChannel : fieldWebsocketChannelMapKraken) {
      this->exchangeFieldMap[CCAPI_EXCHANGE_NAME_KRAKEN].push_back(fieldWebsocketChannel.first);
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
    CCAPI_LOGGER_TRACE("this->exchangeFieldMap = "+toString(this->exchangeFieldMap));
    this->exchangeFieldWebsocketChannelMap = {
      { CCAPI_EXCHANGE_NAME_COINBASE, fieldWebsocketChannelMapCoinbase},
      { CCAPI_EXCHANGE_NAME_GEMINI, fieldWebsocketChannelMapGemini},
      { CCAPI_EXCHANGE_NAME_KRAKEN, fieldWebsocketChannelMapKraken},
      { CCAPI_EXCHANGE_NAME_BITSTAMP, fieldWebsocketChannelMapBitstamp},
      { CCAPI_EXCHANGE_NAME_BITFINEX, fieldWebsocketChannelMapBitfinex},
      { CCAPI_EXCHANGE_NAME_BITMEX, fieldWebsocketChannelMapBitmex}
    };
    this->websocketAvailableMarketDepth = {
      { CCAPI_EXCHANGE_NAME_KRAKEN, std::vector<int>({10, 25, 100, 500, 1000})},
      { CCAPI_EXCHANGE_NAME_BITFINEX, std::vector<int>({1, 25, 100})},
      { CCAPI_EXCHANGE_NAME_BITMEX, std::vector<int>({1, 10, 25})},
    };
    this->websocketMaxAvailableMarketDepth = {
      { CCAPI_EXCHANGE_NAME_BITSTAMP, 100}
    };
    this->urlWebsocketBase = {
      { CCAPI_EXCHANGE_NAME_COINBASE, "wss://ws-feed.pro.coinbase.com"},
      { CCAPI_EXCHANGE_NAME_GEMINI, "wss://api.gemini.com/v1/marketdata/"},
      { CCAPI_EXCHANGE_NAME_KRAKEN, "wss://ws.kraken.com"},
      { CCAPI_EXCHANGE_NAME_BITSTAMP, "wss://ws.bitstamp.net"},
      { CCAPI_EXCHANGE_NAME_BITFINEX, "wss://api-pub.bitfinex.com/ws/2"},
      { CCAPI_EXCHANGE_NAME_BITMEX, "wss://www.bitmex.com/realtime"},
    };
    this->initialSequenceByExchangeMap = { {CCAPI_EXCHANGE_NAME_GEMINI, 0}, {CCAPI_EXCHANGE_NAME_BITFINEX, 1}};
  }
  std::map<std::string, std::map<std::string, std::string> > exchangePairSymbolMap;
  std::map<std::string, std::vector<std::string> > exchangePairMap;
  std::map<std::string, std::vector<std::string> > exchangeFieldMap;
  std::map<std::string, std::map<std::string, std::string> > exchangeFieldWebsocketChannelMap;
  std::map<std::string, std::vector<int> > websocketAvailableMarketDepth;
  std::map<std::string, int> websocketMaxAvailableMarketDepth;
  std::map<std::string, std::string> urlWebsocketBase;
  std::map<std::string, int> initialSequenceByExchangeMap;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SESSION_CONFIGS_H_
