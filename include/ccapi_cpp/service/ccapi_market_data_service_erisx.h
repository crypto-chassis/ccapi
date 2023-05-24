#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_ERISX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_ERISX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceErisx : public MarketDataService {
 public:
  MarketDataServiceErisx(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                         std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_ERISX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    // this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*)");
  }
  virtual ~MarketDataServiceErisx() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested <= 20) {
        channelId = std::string(CCAPI_WEBSOCKET_ERISX_CHANNEL_TOP_OF_BOOK_MARKET_DATA_SUBSCRIBE) + "?" + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" +
                    std::to_string(marketDepthRequested);
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthRequested;
      } else {
        channelId += "|" + field;
      }
    } else if (field == CCAPI_TRADE) {
      channelId += "|" + field;
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        std::string exchangeSubscriptionId = std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]);
        this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("correlation", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        document.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        if (channelId.rfind(CCAPI_WEBSOCKET_ERISX_CHANNEL_MARKET_DATA_SUBSCRIBE, 0) == 0) {
          document.AddMember("type", rj::Value(CCAPI_WEBSOCKET_ERISX_CHANNEL_MARKET_DATA_SUBSCRIBE).Move(), allocator);
          if (UtilString::split(channelId, "|").at(1) == CCAPI_TRADE) {
            document.AddMember("tradeOnly", rj::Value(true).Move(), allocator);
          }
        } else if (channelId.rfind(CCAPI_WEBSOCKET_ERISX_CHANNEL_TOP_OF_BOOK_MARKET_DATA_SUBSCRIBE, 0) == 0) {
          document.AddMember("type", rj::Value(CCAPI_WEBSOCKET_ERISX_CHANNEL_TOP_OF_BOOK_MARKET_DATA_SUBSCRIBE).Move(), allocator);
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          document.AddMember("topOfBookDepth", rj::Value(marketDepthSubscribedToExchange).Move(), allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
    return sendStringList;
  }
  void processTextMessage(
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage
#else
      std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView
#endif
      ,
      const TimePoint& timeReceived, Event& event, std::vector<MarketDataMessage>& marketDataMessageList) override {
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    std::string type = document["type"].GetString();
    if (type == "MarketDataIncrementalRefresh" || type == "MarketDataIncrementalRefreshTrade") {
      std::string exchangeSubscriptionId = document["correlation"].GetString();
      auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
      auto symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
      MarketDataMessage::RecapType recapType;
      if (std::string(document["marketDataID"].GetString()) == "0") {
        recapType = MarketDataMessage::RecapType::SOLICITED;
      } else {
        recapType = MarketDataMessage::RecapType::NONE;
      }
      if (type == "MarketDataIncrementalRefresh") {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = recapType;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = UtilTime::parse(UtilTime::convertFIXTimeToISO(std::string(document["transactTime"].GetString())));
        for (const auto& side : {"bids", "offers"}) {
          for (const auto& x : document[side].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["price"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                              std::string(x["updateAction"].GetString()) == "DELETE" ? "0" : UtilString::normalizeDecimalString(x["amount"].GetString())});
            marketDataMessage.data[strcmp(side, "bids") == 0 ? MarketDataMessage::DataType::BID : MarketDataMessage::DataType::ASK].push_back(
                std::move(dataPoint));
          }
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else {
        for (const auto& x : document["trades"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::parse(UtilTime::convertFIXTimeToISO(std::string(x["transactTime"].GetString())));
          marketDataMessage.recapType = recapType;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["size"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, ""});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["tickerType"].GetString()) == "GIVEN" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      }
    } else if (type == "TopOfBookMarketData") {
      std::string exchangeSubscriptionId = document["correlation"].GetString();
      auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
      auto symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
      MarketDataMessage marketDataMessage;
      marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
      if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
      } else {
        marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
      }
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      std::string latestFIXTime;
      for (const auto& side : {"bids", "offers"}) {
        for (const auto& x : document[side].GetArray()) {
          if (std::string(x["action"].GetString()) != "NO CHANGE") {
            std::string lastUpdate = x["lastUpdate"].GetString();
            if (lastUpdate > latestFIXTime) {
              latestFIXTime = lastUpdate;
            }
          }
        }
      }
      marketDataMessage.tp = UtilTime::parse(UtilTime::convertFIXTimeToISO(latestFIXTime));
      for (const auto& side : {"bids", "offers"}) {
        for (const auto& x : document[side].GetArray()) {
          if (std::string(x["action"].GetString()) != "NO CHANGE") {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["price"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                              std::string(x["action"].GetString()) == "DELETE" ? "0" : UtilString::normalizeDecimalString(x["totalVolume"].GetString())});
            marketDataMessage.data[strcmp(side, "bids") == 0 ? MarketDataMessage::DataType::BID : MarketDataMessage::DataType::ASK].push_back(
                std::move(dataPoint));
          }
        }
      }
      marketDataMessageList.emplace_back(std::move(marketDataMessage));
    }
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    // TODO(cryptochassis): implement
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    // TODO(cryptochassis): implement
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_ERISX_H_
