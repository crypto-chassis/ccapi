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
    this->pongTimeoutMilliSecondsByMethodMap[PingPongMethod::APPLICATION_LEVEL] = std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingInterval"));
    //  must ping kucoin server
    this->setPingPongTimer(PingPongMethod::APPLICATION_LEVEL, wsConnection, hdl, [that = shared_from_base<MarketDataServiceKucoin>()](wspp::connection_hdl hdl, ErrorCode & ec) {
      auto now = UtilTime::now();
      that->send(hdl, "{\"id\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"type\":\"ping\"}", wspp::frame::opcode::text, ec);
    });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
//    rj::Document document;
//    document.SetObject();
//    rj::Document::AllocatorType& allocator = document.GetAllocator();
//    document.AddMember("method", rj::Value("SUBSCRIBE").Move(), allocator);
//    rj::Value params(rj::kArrayType);
//    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
//      auto channelId = subscriptionListByChannelIdSymbolId.first;
//      for (const auto & subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
//        auto symbolId = subscriptionListByInstrument.first;
//        auto exchangeSubscriptionId = symbolId + "@";
//        if (channelId.rfind(CCAPI_WEBSOCKET_KUCOIN_CHANNEL_PARTIAL_BOOK_DEPTH, 0) == 0) {
//          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
//          int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
//          exchangeSubscriptionId += std::string(CCAPI_WEBSOCKET_KUCOIN_CHANNEL_PARTIAL_BOOK_DEPTH) + std::to_string(marketDepthSubscribedToExchange);
//        } else {
//          exchangeSubscriptionId += channelId;
//        }
//        params.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
//        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
//        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
//        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
//      }
//    }
//    document.AddMember("params", params, allocator);
//    document.AddMember("id", rj::Value(exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
//    exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
//    rj::StringBuffer stringBuffer;
//    rj::Writer<rj::StringBuffer> writer(stringBuffer);
//    document.Accept(writer);
//    std::string requestString = stringBuffer.GetString();
//    requestStringList.push_back(requestString);
    return requestStringList;
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
    }
//    else if (document.IsObject() && document.HasMember("stream") && document.HasMember("data")) {
//      MarketDataMessage wsMessage;
//      std::string exchangeSubscriptionId = document["stream"].GetString();
//      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
//      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
//      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
//      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
//      CCAPI_LOGGER_TRACE("channelId = "+channelId);
//      const rj::Value& data = document["data"];
//      if (channelId.rfind(CCAPI_WEBSOCKET_KUCOIN_CHANNEL_PARTIAL_BOOK_DEPTH, 0) == 0) {
//        wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
//        wsMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
//        wsMessage.tp = this->isFutures ? TimePoint(std::chrono::milliseconds(data["T"].GetInt64())) : timeReceived;
//        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
//        const char* bidsName = this->isFutures ? "b" : "bids";
//        int bidIndex = 0;
//        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
//        for (const auto& x : data[bidsName].GetArray()) {
//          if (bidIndex >= maxMarketDepth) {
//            break;
//          }
//          MarketDataMessage::TypeForDataPoint dataPoint;
//          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
//          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
//          wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
//          ++bidIndex;
//        }
//        const char* asksName = this->isFutures ? "a" : "asks";
//        int askIndex = 0;
//        for (const auto& x : data[asksName].GetArray()) {
//          if (askIndex >= maxMarketDepth) {
//            break;
//          }
//          MarketDataMessage::TypeForDataPoint dataPoint;
//          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
//          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
//          wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
//          ++askIndex;
//        }
//        wsMessageList.push_back(std::move(wsMessage));
//      } else if (channelId == CCAPI_WEBSOCKET_KUCOIN_CHANNEL_TRADE) {
//        MarketDataMessage wsMessage;
//        wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
//        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
//        wsMessage.tp = UtilTime::makeTimePointFromMilliseconds(data["T"].GetInt64());
//        wsMessage.recapType = MarketDataMessage::RecapType::NONE;
//        MarketDataMessage::TypeForDataPoint dataPoint;
//        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, std::string(data["p"].GetString())});
//        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::string(data["q"].GetString())});
//        dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(data["t"].GetInt64())});
//        dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["m"].GetBool() ? "1" : "0"});
//        wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
//        wsMessageList.push_back(std::move(wsMessage));
//      } else if (channelId == CCAPI_WEBSOCKET_KUCOIN_CHANNEL_AGG_TRADE) {
//        if (this->isFutures) {
//          int64_t firstTradeId = data["f"].GetInt64();
//          int64_t lastTradeId = data["l"].GetInt64();
//          auto tradeId = firstTradeId;
//          auto time = UtilTime::makeTimePointFromMilliseconds(data["T"].GetInt64());
//          while (tradeId <= lastTradeId) {
//            MarketDataMessage wsMessage;
//            wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
//            wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
//            wsMessage.tp = time;
//            wsMessage.recapType = MarketDataMessage::RecapType::NONE;
//            MarketDataMessage::TypeForDataPoint dataPoint;
//            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, std::string(data["p"].GetString())});
//            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::string(data["q"].GetString())});
//            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(tradeId)});
//            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["m"].GetBool() ? "1" : "0"});
//            wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
//            wsMessageList.push_back(std::move(wsMessage));
//            ++tradeId;
//          }
//        }
//      }
//    }
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_H_
