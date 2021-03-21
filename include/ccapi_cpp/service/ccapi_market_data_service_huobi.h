#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_market_data_service.h"
#include <regex>
namespace ccapi {
class MarketDataServiceHuobi CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceHuobi(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_HUOBI;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
    ErrorCode ec = this->inflater.init(false, 31);
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
  }

 private:
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode & ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"ping\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + "}", wspp::frame::opcode::text, ec);
  }
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto & subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        auto symbolId = subscriptionListByInstrument.first;
        std::string exchangeSubscriptionId;
        if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH, 0) == 0) {
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
        std::string requestString = stringBuffer.GetString();
        requestStringList.push_back(std::move(requestString));
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    return requestStringList;
  }
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto url = this->baseUrl;
    auto field = subscription.getField();
    if (field == CCAPI_TRADE || field == CCAPI_MARKET_DEPTH) {
      url += "/ws";
    }
    return url + "|" + field + "|" + subscription.getSerializedOptions();
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*[Ee]?-?\\d*)"), "$1\"$2\"");
    CCAPI_LOGGER_TRACE("quotedTextMessage = "+quotedTextMessage);
    document.Parse(quotedTextMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    if (document.IsObject() && document.HasMember("ch") && document.HasMember("tick")) {
      std::string exchangeSubscriptionId = document["ch"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
      CCAPI_LOGGER_TRACE("channel = "+channelId);
      if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH_REGEX))) {
        CCAPI_LOGGER_TRACE("it is snapshot");
        MarketDataMessage wsMessage;
        wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        wsMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
        const rj::Value& tick = document["tick"];
        std::string ts = tick["ts"].GetString();
        ts.insert(ts.size() - 3, ".");
        wsMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
        CCAPI_LOGGER_TRACE("wsMessage.tp = " + toString(wsMessage.tp));
        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : tick["bids"].GetArray()) {
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
        for (const auto& x : tick["asks"].GetArray()) {
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
      } else if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL_REGEX))) {
        const rj::Value& tick = document["tick"];
        std::string ts = tick["ts"].GetString();
        ts.insert(ts.size() - 3, ".");
        auto time = UtilTime::makeTimePoint(UtilTime::divide(ts));
        for (const auto& x : tick["data"].GetArray()) {
          MarketDataMessage wsMessage;
          wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          wsMessage.tp = time;
          wsMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, std::string(x["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::string(x["amount"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["tradeId"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
          wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          wsMessageList.push_back(std::move(wsMessage));
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
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_H_
