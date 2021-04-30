#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KRAKEN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KRAKEN_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceKraken CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceKraken(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KRAKEN;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->shouldAlignSnapshot = true;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostFromUrl(this->baseUrlRest);
    this->getRecentTradesTarget = "/0/public/Trades";
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      if (channelId.rfind(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK, 0) == 0) {
        std::map<int, std::vector<std::string> > symbolIdListByMarketDepthSubscribedToExchangeMap;
        for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListBySymbolId.first;
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          std::string exchangeSubscriptionId =
              std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK) + "-" + std::to_string(marketDepthSubscribedToExchange) + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          symbolIdListByMarketDepthSubscribedToExchangeMap[marketDepthSubscribedToExchange].push_back(symbolId);
        }
        for (const auto& x : symbolIdListByMarketDepthSubscribedToExchangeMap) {
          auto marketDepthSubscribedToExchange = x.first;
          auto symbolIdList = x.second;
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("event", rj::Value("subscribe").Move(), allocator);
          rj::Value instrument(rj::kArrayType);
          for (const auto& symbolId : symbolIdList) {
            instrument.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          }
          document.AddMember("pair", instrument, allocator);
          rj::Value subscription(rj::kObjectType);
          subscription.AddMember("depth", rj::Value(marketDepthSubscribedToExchange).Move(), allocator);
          subscription.AddMember("name", rj::Value(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK).c_str(), allocator).Move(), allocator);
          document.AddMember("subscription", subscription, allocator);
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string requestString = stringBuffer.GetString();
          requestStringList.push_back(requestString);
        }
      } else if (channelId == CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("subscribe").Move(), allocator);
        rj::Value instrument(rj::kArrayType);
        for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListBySymbolId.first;
          std::string exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE) + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          instrument.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        }
        document.AddMember("pair", instrument, allocator);
        rj::Value subscription(rj::kObjectType);
        subscription.AddMember("name", rj::Value(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE).c_str(), allocator).Move(), allocator);
        document.AddMember("subscription", subscription, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string requestString = stringBuffer.GetString();
        requestStringList.push_back(requestString);
      }
    }
    return requestStringList;
  }
  std::vector<MarketDataMessage> processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage,
                                                    const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    rj::Document document;
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    if (document.IsArray() && document.Size() >= 4 && document.Size() <= 5) {
      auto documentSize = document.Size();
      auto channelNameWithSuffix = std::string(document[documentSize - 2].GetString());
      if (channelNameWithSuffix.rfind(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK, 0) == 0) {
        auto symbolId = std::string(document[documentSize - 1].GetString());
        auto exchangeSubscriptionId = channelNameWithSuffix + "|" + symbolId;
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = " +
                           toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
        CCAPI_LOGGER_TRACE("wsConnection = " + toString(wsConnection));
        CCAPI_LOGGER_TRACE("exchangeSubscriptionId = " + exchangeSubscriptionId);
        auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        CCAPI_LOGGER_TRACE("symbolId = " + symbolId);
        const rj::Value& anonymous = document[1];
        if (anonymous.IsObject() && (anonymous.HasMember("b") || anonymous.HasMember("a"))) {
          CCAPI_LOGGER_TRACE("this is update");
          rj::Value anonymous2(anonymous, allocator);
          if (documentSize == 5) {
            rj::Value& source = document[2];
            for (rj::Value::MemberIterator itr = source.MemberBegin(); itr != source.MemberEnd(); ++itr) {
              anonymous2.AddMember(itr->name, itr->value, allocator);
            }
          }
          TimePoint latestTp(std::chrono::seconds(0));
          if (anonymous2.HasMember("b")) {
            for (const auto& x : anonymous2["b"].GetArray()) {
              auto timePair = UtilTime::divide(std::string(x[2].GetString()));
              auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
              tp += std::chrono::nanoseconds(timePair.second);
              if (tp > latestTp) {
                latestTp = tp;
              }
            }
          }
          if (anonymous2.HasMember("a")) {
            for (const auto& x : anonymous2["a"].GetArray()) {
              auto timePair = UtilTime::divide(std::string(x[2].GetString()));
              auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
              tp += std::chrono::nanoseconds(timePair.second);
              if (tp > latestTp) {
                latestTp = tp;
              }
            }
          }
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.tp = latestTp;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          if (anonymous2.HasMember("b")) {
            for (const auto& x : anonymous2["b"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            }
          }
          if (anonymous2.HasMember("a")) {
            for (const auto& x : anonymous2["a"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          }
          marketDataMessageList.push_back(std::move(marketDataMessage));
        } else if (anonymous.IsObject() && anonymous.HasMember("as") && anonymous.HasMember("bs")) {
          CCAPI_LOGGER_TRACE("this is snapshot");
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
          marketDataMessage.tp = timeReceived;
          for (const auto& x : anonymous["bs"].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          }
          for (const auto& x : anonymous["as"].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
          }
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      } else if (channelNameWithSuffix == CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE) {
        for (const auto& x : document[1].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          auto symbolId = std::string(document[documentSize - 1].GetString());
          marketDataMessage.exchangeSubscriptionId = channelNameWithSuffix + "|" + symbolId;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          auto timePair = UtilTime::divide(std::string(x[2].GetString()));
          auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
          tp += std::chrono::nanoseconds(timePair.second);
          marketDataMessage.tp = tp;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x[0].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x[1].GetString()))});
          // std::stringstream ss;
          // ss << std::setw(9) << std::setfill('0') << timePair.second;
          // dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(timePair.first) + ss.str()});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x[3].GetString()) == "s" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      }
    } else if (document.IsObject() && document.HasMember("event")) {
      std::string eventPayload = std::string(document["event"].GetString());
      if (eventPayload == "heartbeat") {
        CCAPI_LOGGER_DEBUG("heartbeat: " + toString(wsConnection));
      } else if (eventPayload == "subscriptionStatus") {
        // TODO(cryptochassis): implement
      }
    }
    return marketDataMessageList;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendSymbolId(queryString, symbolId, "pair");
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertReqCustom(req, request, now, symbolId, credential);
    }
  }
  void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("([,\\[:])(-?\\d+\\.?\\d*[eE]?-?\\d*)"), "$1\"$2\"");
    CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
    MarketDataService::processSuccessfulTextMessage(request, quotedTextMessage, timeReceived);
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage,
                                                                       const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    auto instrument = request.getInstrument();
    auto symbolId = this->convertInstrumentToRestSymbolId(instrument);
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"][symbolId.c_str()].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          auto timePair = UtilTime::divide(std::string(x[2].GetString()));
          auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
          tp += std::chrono::nanoseconds(timePair.second);
          marketDataMessage.tp = tp;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x[0].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x[1].GetString()))});
          // std::stringstream ss;
          // ss << std::setw(9) << std::setfill('0') << timePair.second;
          // dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(timePair.first) + ss.str()});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x[3].GetString()) == "s" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return marketDataMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KRAKEN_H_
