#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceBitstamp CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceBitstamp(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_BITSTAMP;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
    this->enableCheckPingPongWebsocketApplicationLevel = false;
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto & subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("bts:subscribe").Move(), allocator);
        rj::Value data(rj::kObjectType);
        auto symbolId = subscriptionListByInstrument.first;
        if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = channelId + "_" + symbolId;
        data.AddMember("channel", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        document.AddMember("data", data, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string requestString = stringBuffer.GetString();
        requestStringList.push_back(std::move(requestString));
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    return requestStringList;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    const rj::Value& data = document["data"];
    if (document.IsObject() && document.HasMember("event") && std::string(document["event"].GetString()) == "bts:subscription_succeeded") {
    } else if (document.IsObject() && document.HasMember("event") && (std::string(document["event"].GetString()) == "data" || std::string(document["event"].GetString()) == "trade")) {
      MarketDataMessage wsMessage;
      std::string exchangeSubscriptionId = document["channel"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
      CCAPI_LOGGER_TRACE("channelId = "+channelId);
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
        wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
          wsMessage.recapType = MarketDataMessage::RecapType::NONE;
        } else {
          wsMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
        }
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        wsMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : data["bids"].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          ++bidIndex;
        }
        int askIndex = 0;
        for (const auto& x : data["asks"].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
          ++askIndex;
        }
        wsMessageList.push_back(std::move(wsMessage));
      } else if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_LIVE_TRADES) {
        wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        wsMessage.recapType = MarketDataMessage::RecapType::NONE;
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        wsMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, std::string(data["price_str"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::string(data["amount_str"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(data["id"].GetInt64())});
        dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["type"].GetInt() == 0 ? "1" : "0"});
        wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
        wsMessageList.push_back(std::move(wsMessage));
      }
    }
    return wsMessageList;
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
    // TODO(cryptochassis): implement
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    std::vector<MarketDataMessage> marketDataMessageList;
    // TODO(cryptochassis): implement
    return marketDataMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
