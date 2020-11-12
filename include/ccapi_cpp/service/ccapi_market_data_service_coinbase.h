#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_COINBASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_COINBASE_H_
#ifdef ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceCoinbase final : public MarketDataService {
 public:
  MarketDataServiceCoinbase(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_COINBASE;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("type", rj::Value("subscribe").Move(), allocator);
    rj::Value channels(rj::kArrayType);
    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      rj::Value channel(rj::kObjectType);
      rj::Value symbolIds(rj::kArrayType);
      for (const auto & subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        symbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        std::string exchangeSubscriptionId = channelId+
        "|"+symbolId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_SYMBOL_ID] = symbolId;
      }
      channel.AddMember("name", rj::Value(channelId.c_str(), allocator).Move(), allocator);
      channel.AddMember("product_ids", symbolIds, allocator);
      channels.PushBack(channel, allocator);
      rj::Value heartbeatChannel(rj::kObjectType);
      heartbeatChannel.AddMember("name", rj::Value("heartbeat").Move(), allocator);
      rj::Value heartbeatSymbolIds(rj::kArrayType);
      for (const auto & subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        heartbeatSymbolIds.PushBack(rj::Value(subscriptionListBySymbolId.first.c_str(), allocator).Move(), allocator);
      }
      heartbeatChannel.AddMember("product_ids", heartbeatSymbolIds, allocator);
      channels.PushBack(heartbeatChannel, allocator);
    }
    document.AddMember("channels", channels, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string requestString = stringBuffer.GetString();
    requestStringList.push_back(requestString);
    return requestStringList;
  }
//  void onOpen(wspp::connection_hdl hdl) override {
//    CCAPI_LOGGER_FUNCTION_ENTER;
//    MarketDataService::onOpen(hdl);
//    CCAPI_LOGGER_FUNCTION_EXIT;
//  }
//  void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
//    CCAPI_LOGGER_FUNCTION_ENTER;
//    MarketDataService::onTextMessage(hdl, textMessage, timeReceived);
////    this->onTextMessage_2(hdl, textMessage, timeReceived);
//    CCAPI_LOGGER_FUNCTION_EXIT;
//  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = "+type);
    if (type == "l2update") {
      auto symbolId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_EXCHANGE_NAME_WEBSOCKET_COINBASE_CHANNEL_LEVEL2)+
      "|"+symbolId;
      MarketDataMessage wsMessage;
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      wsMessage.tp = UtilTime::parse(std::string(document["time"].GetString()));
      wsMessage.recapType = MarketDataMessage::RecapType::NONE;
      const rj::Value& changes = document["changes"];
      for (auto& change : changes.GetArray()) {
        auto side = std::string(change[0].GetString());
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(change[1].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(change[2].GetString())});
        if (side == "buy") {
          wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
        } else {
          wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
        }
      }
      wsMessageList.push_back(std::move(wsMessage));
    } else if (type == "match") {
      // TODO(cryptochassis): implement
    } else if (type == "heartbeat") {
      CCAPI_LOGGER_DEBUG("heartbeat: "+toString(wsConnection));
    } else if (type == "snapshot") {
      auto symbolId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_EXCHANGE_NAME_WEBSOCKET_COINBASE_CHANNEL_LEVEL2)+
      "|"+symbolId;
      MarketDataMessage wsMessage;
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      wsMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
      const rj::Value& bids = document["bids"];
      for (auto& x : bids.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
        wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
      }
      const rj::Value& asks = document["asks"];
      for (auto& x : asks.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
        wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
      }
      wsMessageList.push_back(std::move(wsMessage));
    } else if (type == "subscriptions") {
      // TODO(cryptochassis): implement
    } else if (type == "error") {
      // TODO(cryptochassis): implement
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_COINBASE_H_
