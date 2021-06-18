#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceHuobiBase : public MarketDataService {
 public:
  MarketDataServiceHuobiBase(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    ErrorCode ec = this->inflater.init(false, 31);
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
    this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*[eE]?-?\\d*)");
    this->needDecompressWebsocketMessage = true;
  }
  virtual ~MarketDataServiceHuobiBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"ping\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + "}", wspp::frame::opcode::text, ec);
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        auto symbolId = subscriptionListByInstrument.first;
        std::string exchangeSubscriptionId;
        if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO;
        } else if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH;
        } else if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL, 0) == 0) {
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL;
        }
        std::string toReplace("$symbol");
        exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), symbolId);
        document.AddMember("sub", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(std::move(sendString));
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = " +
                           toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    return sendStringList;
  }
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto url = this->baseUrl;
    auto field = subscription.getField();
    if (!this->isDerivatives) {
      if (field == CCAPI_TRADE || field == CCAPI_MARKET_DEPTH) {
        url += "/ws";
      }
    }
    return url + "|" + field + "|" + subscription.getSerializedOptions();
  }
  std::vector<MarketDataMessage> processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage,
                                                    const TimePoint& timeReceived) override {
    rj::Document document;
    std::string quotedTextMessage = this->convertNumberToStringInJson(textMessage);
    CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
    document.Parse(quotedTextMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    if (document.IsObject() && document.HasMember("ch") && document.HasMember("tick")) {
      std::string exchangeSubscriptionId = document["ch"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = " + exchangeSubscriptionId);
      CCAPI_LOGGER_TRACE("channel = " + channelId);
      if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO_REGEX))) {
        CCAPI_LOGGER_TRACE("it is bbo");
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        const rj::Value& tick = document["tick"];
        std::string ts = tick[this->isDerivatives ? "ts" : "quoteTime"].GetString();
        ts.insert(ts.size() - 3, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
        CCAPI_LOGGER_TRACE("marketDataMessage.tp = " + toString(marketDataMessage.tp));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        if (this->isDerivatives) {
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["bid"][0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["bid"][1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          }
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["ask"][0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["ask"][1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            marketDataMessageList.push_back(std::move(marketDataMessage));
          }
        } else {
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["bid"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["bidSize"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          }
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["ask"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["askSize"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            marketDataMessageList.push_back(std::move(marketDataMessage));
          }
        }
      } else if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH_REGEX))) {
        CCAPI_LOGGER_TRACE("it is snapshot");
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        const rj::Value& tick = document["tick"];
        std::string ts = tick["ts"].GetString();
        ts.insert(ts.size() - 3, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
        CCAPI_LOGGER_TRACE("marketDataMessage.tp = " + toString(marketDataMessage.tp));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : tick["bids"].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          ++bidIndex;
        }
        int askIndex = 0;
        for (const auto& x : tick["asks"].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
          ++askIndex;
        }
        marketDataMessageList.push_back(std::move(marketDataMessage));
      } else if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL_REGEX))) {
        const rj::Value& tick = document["tick"];
        for (const auto& x : tick["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          std::string ts = x["ts"].GetString();
          ts.insert(ts.size() - 3, ".");
          auto time = UtilTime::makeTimePoint(UtilTime::divide(ts));
          marketDataMessage.tp = time;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x[this->isDerivatives ? "id" : "tradeId"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      }
    } else if (document.IsObject() && document.HasMember("ping")) {
      std::string payload("{\"pong\":");
      payload += document["ping"].GetString();
      payload += "}";
      ErrorCode ec;
      this->send(hdl, payload, wspp::frame::opcode::text, ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "pong");
      }
    } else if (document.IsObject() && document.HasMember("status") && document.HasMember("subbed")) {
    }
    return marketDataMessageList;
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "size"},
                          });
        this->appendSymbolId(queryString, symbolId, this->isDerivatives ? "contract_code" : "symbol");
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    const std::string& quotedTextMessage = this->convertNumberToStringInJson(textMessage);
    CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
    MarketDataService::processSuccessfulTextMessage(request, quotedTextMessage, timeReceived);
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage,
                                                                       const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& datum : document["data"].GetArray()) {
          for (const auto& x : datum["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
            std::string ts = x["ts"].GetString();
            ts.insert(ts.size() - 3, ".");
            marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x[this->isDerivatives ? "id" : "trade-id"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
            marketDataMessageList.push_back(std::move(marketDataMessage));
          }
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return marketDataMessageList;
  }
  bool isDerivatives{};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_BASE_H_
