#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_KUCOIN)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceKucoin CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceKucoin(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_KUCOIN;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }
  virtual ~MarketDataServiceKucoin() {
  }

 protected:
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->pingIntervalMilliSecondsByMethodMap[PingPongMethod::APPLICATION_LEVEL] = std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingInterval"));
    this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::APPLICATION_LEVEL] = std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingTimeout"));
    //  must ping kucoin server
    this->setPingPongTimer(PingPongMethod::APPLICATION_LEVEL, wsConnection, hdl, [that = shared_from_base<MarketDataServiceKucoin>()](wspp::connection_hdl hdl, ErrorCode & ec) {
      auto now = UtilTime::now();
      auto payload = "{\"id\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"type\":\"ping\"}";
      CCAPI_LOGGER_TRACE("payload = " + payload);
      that->send(hdl, payload, wspp::frame::opcode::text, ec);
    });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    std::map<std::string, std::vector<std::string>> symbolListByTopicMap;
    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto & subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        auto exchangeSubscriptionId = channelId + ":" + symbolId;
        if (channelId == CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_TICKER || channelId == CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2DEPTH5 || channelId == CCAPI_WEBSOCKET_KUCOIN_CHANNEL_MARKET_LEVEL2DEPTH50) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        symbolListByTopicMap[channelId].push_back(symbolId);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    for (const auto& x : symbolListByTopicMap) {
      auto topic = x.first;
      auto symbolList = x.second;
      rj::Document document;
      document.SetObject();
      rj::Document::AllocatorType& allocator = document.GetAllocator();
      std::string requestId = std::to_string(this->nextRequestIdByConnectionIdMap[wsConnection.id]);
      this->nextRequestIdByConnectionIdMap[wsConnection.id] += 1;
      document.AddMember("id", rj::Value(requestId.c_str(), allocator).Move(), allocator);
      document.AddMember("type", rj::Value("subscribe").Move(), allocator);
      document.AddMember("privateChannel", false, allocator);
      document.AddMember("response", true, allocator);
      document.AddMember("topic", rj::Value((topic + ":" + UtilString::join(symbolList, ",")).c_str(), allocator).Move() ,allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      std::string requestString = stringBuffer.GetString();
      requestStringList.push_back(requestString);
    }
    return requestStringList;
  }
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->nextRequestIdByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    if (document.IsObject() && std::string(document["type"].GetString()) == "pong") {
      auto now = UtilTime::now();
      WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
          this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
      this->lastPongTpByMethodByConnectionIdMap[wsConnection.id][PingPongMethod::APPLICATION_LEVEL] = now;
    } else if (document.IsObject() && std::string(document["type"].GetString()) == "message" && std::string(document["subject"].GetString()) == "trade.ticker") {
      MarketDataMessage wsMessage;
      std::string exchangeSubscriptionId = document["topic"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      const rj::Value& data = document["data"];
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
      wsMessage.tp = TimePoint(std::chrono::milliseconds(data["time"].GetInt64()));
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      {
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data["bestBid"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["bestBidSize"].GetString())});
        wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
      }
      {
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data["bestAsk"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["bestAskSize"].GetString())});
        wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
      }
      wsMessageList.push_back(std::move(wsMessage));
    } else if (document.IsObject() && std::string(document["type"].GetString()) == "message" && std::string(document["subject"].GetString()) == "level2") {
      MarketDataMessage wsMessage;
      std::string exchangeSubscriptionId = document["topic"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      const rj::Value& data = document["data"];
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
      wsMessage.tp = TimePoint(std::chrono::milliseconds(data["timestamp"].GetInt64()));
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      const std::map<MarketDataMessage::DataType, const char *> bidAsk {
        {MarketDataMessage::DataType::BID, "bids"},
        {MarketDataMessage::DataType::ASK, "asks"}
      };
      for (const auto& x: bidAsk) {
        int index = 0;
        for (const auto& y : data[x.second].GetArray()) {
          if (index >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(y[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(y[1].GetString())});
          wsMessage.data[x.first].push_back(std::move(dataPoint));
          ++index;
        }
      }
      wsMessageList.push_back(std::move(wsMessage));
    } else if (document.IsObject() && std::string(document["type"].GetString()) == "message" && std::string(document["subject"].GetString()) == "trade.l3match") {
      const rj::Value& data = document["data"];
      MarketDataMessage wsMessage;
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      std::string exchangeSubscriptionId = document["topic"].GetString();
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      std::string timeStr = data["time"].GetString();
      wsMessage.tp = UtilTime::makeTimePoint(std::make_pair(std::stoll(timeStr.substr(0, timeStr.length() - 9)), std::stoll(timeStr.substr(timeStr.length() - 9))));
      wsMessage.recapType = MarketDataMessage::RecapType::NONE;
      MarketDataMessage::TypeForDataPoint dataPoint;
      dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, std::string(data["price"].GetString())});
      dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::string(data["size"].GetString())});
      dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(data["sequence"].GetString())});
      dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(data["side"].GetString()) == "sell" ? "1" : "0"});
      wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
      wsMessageList.push_back(std::move(wsMessage));
    }
    return wsMessageList;
  }
  std::map<std::string, int> nextRequestIdByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
