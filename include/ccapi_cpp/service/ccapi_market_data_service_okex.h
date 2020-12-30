#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceOkex CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceOkex(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_OKEX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
    ErrorCode ec = this->inflater.init(false);
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto & subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        std::string exchangeSubscriptionId = UtilString::split(channelId, "?").at(0)+
        ":"+symbolId;
        args.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string requestString = stringBuffer.GetString();
    requestStringList.push_back(requestString);
    return requestStringList;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    if (document.IsObject() && document.HasMember("table")) {
      std::string table = document["table"].GetString();
      if (table == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH400) {
        std::string action = document["action"].GetString();
        CCAPI_LOGGER_TRACE("action = " + toString(action));
        for (const auto& datum : document["data"].GetArray()) {
          std::string exchangeSubscriptionId = table + ":" + datum["instrument_id"].GetString();
          MarketDataMessage wsMessage;
          wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          wsMessage.recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
          wsMessage.tp = UtilTime::parse(std::string(datum["timestamp"].GetString()));
          CCAPI_LOGGER_TRACE("wsMessage.tp = " + toString(wsMessage.tp));
          wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          for (const auto& x : datum["bids"].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          }
          for (const auto& x : datum["asks"].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
          }
          wsMessageList.push_back(std::move(wsMessage));
        }
      } else if (table == CCAPI_WEBSOCKET_OKEX_CHANNEL_TRADE) {
        for (const auto& datum : document["data"].GetArray()) {
          std::string exchangeSubscriptionId = table + ":" + datum["instrument_id"].GetString();
          MarketDataMessage wsMessage;
          wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          wsMessage.recapType = MarketDataMessage::RecapType::NONE;
          wsMessage.tp = UtilTime::parse(std::string(datum["timestamp"].GetString()));
          wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(datum["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(datum["size"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, datum["trade_id"].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(datum["side"].GetString()) == "sell" ? "1" : "0"});
          wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          wsMessageList.push_back(std::move(wsMessage));
        }
      }
    }
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
