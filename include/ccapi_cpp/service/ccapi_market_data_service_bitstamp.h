#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceBitstamp CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceBitstamp(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                            std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITSTAMP;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->enableCheckPingPongWebsocketApplicationLevel = false;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostFromUrl(this->baseUrlRest);
    this->getRecentTradesTarget = "/api/v2/transactions/{currency_pair}/";  // must have trailing slash
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
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
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = " +
                           toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    return requestStringList;
  }
  std::vector<MarketDataMessage> processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage,
                                                    const TimePoint& timeReceived) override {
    // WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    const rj::Value& data = document["data"];
    if (document.IsObject() && document.HasMember("event") && std::string(document["event"].GetString()) == "bts:subscription_succeeded") {
    } else if (document.IsObject() && document.HasMember("event") &&
               (std::string(document["event"].GetString()) == "data" || std::string(document["event"].GetString()) == "trade")) {
      MarketDataMessage marketDataMessage;
      std::string exchangeSubscriptionId = document["channel"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = " + exchangeSubscriptionId);
      CCAPI_LOGGER_TRACE("channelId = " + channelId);
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        } else {
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
        }
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : data["bids"].GetArray()) {
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
        for (const auto& x : data["asks"].GetArray()) {
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
      } else if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_LIVE_TRADES) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(data["price_str"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(data["amount_str"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(data["id"].GetInt64())});
        dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["type"].GetInt() == 0 ? "1" : "0"});
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
        marketDataMessageList.push_back(std::move(marketDataMessage));
      }
    }
    return marketDataMessageList;
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now,
                  const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
    switch (operation) {
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        this->substituteParam(target, {{"{currency_pair}", symbolId}});
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param, {});
        req.target(target + "?" + queryString);
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage,
                                                                       const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    auto operation = request.getOperation();
    switch (operation) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          marketDataMessage.tp = UtilTime::makeTimePointFromSeconds(std::stol(x["date"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["tid"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["type"].GetString()) == "0" ? "1" : "0"});
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
