#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_GATEIO) || defined(CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGateioBase : public MarketDataService {
 public:
  MarketDataServiceGateioBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                              ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~MarketDataServiceGateioBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl,
               "{\"time\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"channel\":\"" + (this->isDerivatives ? "futures" : "spot") + ".ping\"}",
               wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr,
               "{\"time\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"channel\":\"" + (this->isDerivatives ? "futures" : "spot") + ".ping\"}",
               ec);
  }
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = this->websocketChannelBookTicker;
      } else {
        int marketDepthSubscribedToExchange = 1;
        marketDepthSubscribedToExchange = this->calculateMarketDepthAllowedByExchange(marketDepthRequested, std::vector<int>({5, 10, 20}));
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
        std::string updateSpeed;
        if (this->isDerivatives) {
          updateSpeed = "0";
        } else {
          if (conflateIntervalMilliseconds < 1000) {
            updateSpeed = "100ms";
          } else {
            updateSpeed = "1000ms";
          }
        }
        if (!updateSpeed.empty()) {
          channelId += "&UPDATE_SPEED=" + updateSpeed;
        }
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
      }
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval =
          this->convertCandlestickIntervalSecondsToInterval(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)), "s", "m", "h", "d", "w");
      channelId = this->websocketChannelCandlesticks + interval;
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    auto now = UtilTime::now();
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      if (channelId == this->websocketChannelBookTicker || channelId == this->websocketChannelTrades ||
          channelId.rfind(this->websocketChannelCandlesticks, 0) == 0) {
        std::vector<std::string> exchangeSubscriptionIdList;
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("subscribe").Move(), allocator);
        document.AddMember("time", rj::Value(UtilTime::getUnixTimestamp(now)).Move(), allocator);
        std::string channel(channelId);
        if (channelId.rfind(this->websocketChannelCandlesticks, 0) == 0) {
          channel = this->websocketChannelCandlesticks;
        }
        document.AddMember("channel", rj::Value(channel.c_str(), allocator).Move(), allocator);
        rj::Value payload(rj::kArrayType);
        for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListByInstrument.first;
          if (channelId == this->websocketChannelBookTicker) {
            this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          }
          if (channelId.rfind(this->websocketChannelCandlesticks, 0) == 0) {
            payload.PushBack(rj::Value(std::string(channelId.substr(this->websocketChannelCandlesticks.length())).c_str(), allocator).Move(), allocator);
          }
          payload.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          std::string exchangeSubscriptionId = channelId + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
        }
        document.AddMember("payload", payload, allocator);
        // if (!this->isDerivatives) {
        document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
        this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id][this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
            exchangeSubscriptionIdList;
        this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
        // }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      } else if (channelId.rfind(this->websocketChannelOrderBook, 0) == 0) {
        for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListByInstrument.first;
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("event", rj::Value("subscribe").Move(), allocator);
          document.AddMember("time", rj::Value(UtilTime::getUnixTimestamp(now)).Move(), allocator);
          document.AddMember("channel", rj::Value(this->websocketChannelOrderBook.c_str(), allocator).Move(), allocator);
          rj::Value payload(rj::kArrayType);
          payload.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          payload.PushBack(rj::Value(std::to_string(marketDepthSubscribedToExchange).c_str(), allocator).Move(), allocator);
          auto splitted = UtilString::split(channelId, "?");
          if (splitted.size() == 2) {
            auto mapped = Url::convertQueryStringToMap(splitted.at(1));
            if (mapped.find("UPDATE_SPEED") != mapped.end()) {
              payload.PushBack(rj::Value(mapped.at("UPDATE_SPEED").c_str(), allocator).Move(), allocator);
            }
          }
          document.AddMember("payload", payload, allocator);
          std::string exchangeSubscriptionId = std::string(this->websocketChannelOrderBook) + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          // if (!this->isDerivatives) {
          std::vector<std::string> exchangeSubscriptionIdList;
          exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
          document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
          this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id]
                                                                                [this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
              exchangeSubscriptionIdList;
          this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
          // }
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          sendStringList.push_back(sendString);
        }
      }
    }
    return sendStringList;
  }
  void processTextMessage(
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage
#else
      std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView
#endif
      ,
      const TimePoint& timeReceived, Event& event, std::vector<MarketDataMessage>& marketDataMessageList) override {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    if (document.HasMember("event") && std::string(document["event"].GetString()) == "subscribe") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (document.HasMember("id") && !document["id"].IsNull()) {
        if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
          int id = std::stoi(document["id"].GetString());
          if (this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.find(wsConnection.id) !=
                  this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.end() &&
              this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).find(id) !=
                  this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).end()) {
            for (const auto& exchangeSubscriptionId : this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).at(id)) {
              auto splitted = UtilString::split(exchangeSubscriptionId, '|');
              std::string channelId = splitted.at(0);
              std::string symbolId = splitted.at(1);
              const auto& it = this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).lower_bound(channelId);
              if (it != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).end()) {
                const std::string& k = it->first;
                if (k.compare(0, channelId.size(), channelId) == 0) {
                  std::vector<std::string> correlationIdList_2 = it->second.at(symbolId);
                  correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
                }
              }
            }
          }
        }
      }
      message.setCorrelationIdList(correlationIdList);
      bool hasError = document.HasMember("error") && !document["error"].IsNull();
      message.setType(!hasError ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(!hasError ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
      event.setMessageList(messageList);
    } else if (document.HasMember("event")) {
      std::string eventType = document["event"].GetString();
      if (eventType == "update" || eventType == "all") {
        std::string channel = document["channel"].GetString();
        const rj::Value& result = document["result"];
        std::string originalChannelId(channel);
        if (result.IsObject()) {
          std::string channelId(originalChannelId);
          std::string symbolId;
          if (channel == this->websocketChannelCandlesticks) {
            std::string n = result["n"].GetString();
            auto first_of = n.find_first_of("_");
            channelId = originalChannelId + n.substr(0, first_of);
            symbolId = n.substr(first_of + 1);
          } else {
            auto it = result.FindMember(symbolName.c_str());
            auto its = result.FindMember("s");
            symbolId = it != result.MemberEnd() ? it->value.GetString() : (its != result.MemberEnd() ? its->value.GetString() : "");
          }
          MarketDataMessage marketDataMessage;
          std::string exchangeSubscriptionId;
          std::map<std::string, std::string> optionMap;
          if (!symbolId.empty()) {
            exchangeSubscriptionId = channelId + "|" + symbolId;
            optionMap = (*this->optionMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).lower_bound("channelId")).second.at(symbolId);
          }
          if (channel == this->websocketChannelBookTicker) {
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                              ? MarketDataMessage::RecapType::NONE
                                              : MarketDataMessage::RecapType::SOLICITED;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(result["t"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            {
              std::string p = result["b"].GetString();
              if (!p.empty()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(p)});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(result["B"].GetString())});
                marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
              }
            }
            {
              std::string p = result["a"].GetString();
              if (!p.empty()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(p)});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(result["A"].GetString())});
                marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
              }
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          } else if (channel == this->websocketChannelOrderBook) {
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                              ? MarketDataMessage::RecapType::NONE
                                              : MarketDataMessage::RecapType::SOLICITED;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(result["t"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            int bidIndex = 0;
            int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
            for (const auto& x : result["bids"].GetArray()) {
              if (bidIndex >= maxMarketDepth) {
                break;
              }
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert(
                  {MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString((this->isDerivatives ? x["p"] : x[0]).GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString((this->isDerivatives ? x["s"] : x[1]).GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
              ++bidIndex;
            }
            int askIndex = 0;
            for (const auto& x : result["asks"].GetArray()) {
              if (askIndex >= maxMarketDepth) {
                break;
              }
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert(
                  {MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString((this->isDerivatives ? x["p"] : x[0]).GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString((this->isDerivatives ? x["s"] : x[1]).GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
              ++askIndex;
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          } else if (channel == this->websocketChannelTrades) {
            if (!this->isDerivatives) {
              MarketDataMessage marketDataMessage;
              marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
              marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
              marketDataMessage.tp = UtilTime::makeTimePointMilli(UtilTime::divideMilli(result["create_time_ms"].GetString()));
              marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(result["price"].GetString()))});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(result["amount"].GetString()))});
              dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(result["id"].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(result["side"].GetString()) == "sell" ? "1" : "0"});
              marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
              marketDataMessageList.emplace_back(std::move(marketDataMessage));
            }
          } else if (channel == this->websocketChannelCandlesticks) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = TimePoint(std::chrono::seconds(std::stoll(result["t"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, result["o"].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, result["h"].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, result["l"].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, result["c"].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, result["a"].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::QUOTE_VOLUME, result["v"].GetString()});
            marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (result.IsArray()) {
          if (channel == this->websocketChannelTrades) {
            if (this->isDerivatives) {
              for (const auto& x : result.GetArray()) {
                std::string symbolId = x["contract"].GetString();
                std::string exchangeSubscriptionId = channel + "|" + symbolId;
                MarketDataMessage marketDataMessage;
                marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
                marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
                marketDataMessage.tp = UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["create_time_ms"].GetString()));
                marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
                MarketDataMessage::TypeForDataPoint dataPoint;
                std::string size = x["size"].GetString();
                std::string sizeAbs;
                bool isBuyerMaker;
                if (size.at(0) == '-') {
                  sizeAbs = size.substr(1);
                  isBuyerMaker = true;
                } else {
                  sizeAbs = size;
                  isBuyerMaker = false;
                }
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(sizeAbs)});
                dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["id"].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, isBuyerMaker ? "1" : "0"});
                marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
                marketDataMessageList.emplace_back(std::move(marketDataMessage));
              }
            }
          } else if (channel == this->websocketChannelCandlesticks) {
            if (this->isDerivatives) {
              for (const auto& x : result.GetArray()) {
                std::string n = x["n"].GetString();
                auto first_of = n.find_first_of("_");
                std::string channelId = originalChannelId + n.substr(0, first_of);
                std::string symbolId = n.substr(first_of + 1);
                std::string exchangeSubscriptionId = channelId + "|" + symbolId;
                MarketDataMessage marketDataMessage;
                marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
                marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
                marketDataMessage.tp = TimePoint(std::chrono::seconds(std::stoll(x["t"].GetString())));
                marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, x["o"].GetString()});
                dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, x["h"].GetString()});
                dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, x["l"].GetString()});
                dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, x["c"].GetString()});
                dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, x["v"].GetString()});
                marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
                marketDataMessageList.emplace_back(std::move(marketDataMessage));
              }
            }
          }
        }
      }
    }
  }
  bool isDerivatives{};
  std::string symbolName;
  std::string websocketChannelTrades;
  std::string websocketChannelBookTicker;
  std::string websocketChannelOrderBook;
  std::string websocketChannelCandlesticks;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_BASE_H_
