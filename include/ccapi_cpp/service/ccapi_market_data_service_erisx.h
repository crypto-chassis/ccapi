#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_ERISX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_ERISX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_ERISX
#include <regex>
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceErisx CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceErisx(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions,
                         SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_ERISX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    for (const auto& subscriptionListByChannelIdSymbolId :
         this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        std::string exchangeSubscriptionId =
            std::to_string(this->nextExchangeSubscriptionIdByConnectionIdMap[wsConnection.id]);
        this->nextExchangeSubscriptionIdByConnectionIdMap[wsConnection.id] += 1;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId]
                                                                      [CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId]
                                                                      [CCAPI_SYMBOL_ID] = symbolId;
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
          document.AddMember("type", rj::Value(CCAPI_WEBSOCKET_ERISX_CHANNEL_TOP_OF_BOOK_MARKET_DATA_SUBSCRIBE).Move(),
                             allocator);
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)
                  .at(channelId)
                  .at(symbolId);
          document.AddMember("topOfBookDepth", rj::Value(marketDepthSubscribedToExchange).Move(), allocator);
        }
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string requestString = stringBuffer.GetString();
        requestStringList.push_back(requestString);
      }
    }
    CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = " +
                       toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
    return requestStringList;
  }
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection =
        this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->nextExchangeSubscriptionIdByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage,
                                                    const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection =
        this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*)"), "$1\"$2\"");
    CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
    document.Parse(quotedTextMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    std::string type = document["type"].GetString();
    if (type == "MarketDataIncrementalRefresh" || type == "MarketDataIncrementalRefreshTrade") {
      std::string exchangeSubscriptionId = document["correlation"].GetString();
      auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id)
                           .at(exchangeSubscriptionId)
                           .at(CCAPI_CHANNEL_ID);
      auto symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id)
                          .at(exchangeSubscriptionId)
                          .at(CCAPI_SYMBOL_ID);
      MarketDataMessage::RecapType recapType;
      if (std::string(document["marketDataID"].GetString()) == "0") {
        recapType = MarketDataMessage::RecapType::SOLICITED;
      } else {
        recapType = MarketDataMessage::RecapType::NONE;
      }
      if (type == "MarketDataIncrementalRefresh") {
        MarketDataMessage wsMessage;
        wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        wsMessage.recapType = recapType;
        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        wsMessage.tp = UtilTime::parse(UtilTime::convertFIXTimeToISO(std::string(document["transactTime"].GetString())));
        for (const auto& side : {"bids", "offers"}) {
          for (const auto& x : document[side].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert(
                {MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["price"].GetString())});
            dataPoint.insert(
                {MarketDataMessage::DataFieldType::SIZE, std::string(x["updateAction"].GetString()) == "DELETE"
                                                             ? "0"
                                                             : UtilString::normalizeDecimalString(x["amount"].GetString())});
            wsMessage.data[strcmp(side, "bids") == 0 ? MarketDataMessage::DataType::BID : MarketDataMessage::DataType::ASK]
                .push_back(std::move(dataPoint));
          }
        }
        wsMessageList.push_back(std::move(wsMessage));
      } else {
        for (const auto& x : document["trades"].GetArray()) {
          MarketDataMessage wsMessage;
          wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          wsMessage.tp = UtilTime::parse(UtilTime::convertFIXTimeToISO(std::string(x["transactTime"].GetString())));
          wsMessage.recapType = recapType;
          wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, std::string(x["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::string(x["size"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, ""});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER,
                            std::string(x["tickerType"].GetString()) == "GIVEN" ? "1" : "0"});
          wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          wsMessageList.push_back(std::move(wsMessage));
        }
      }
    } else if (type == "TopOfBookMarketData") {
      std::string exchangeSubscriptionId = document["correlation"].GetString();
      auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id)
                           .at(exchangeSubscriptionId)
                           .at(CCAPI_CHANNEL_ID);
      auto symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id)
                          .at(exchangeSubscriptionId)
                          .at(CCAPI_SYMBOL_ID);
      MarketDataMessage wsMessage;
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
        wsMessage.recapType = MarketDataMessage::RecapType::NONE;
      } else {
        wsMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
      }
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
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
      wsMessage.tp = UtilTime::parse(UtilTime::convertFIXTimeToISO(latestFIXTime));
      for (const auto& side : {"bids", "offers"}) {
        for (const auto& x : document[side].GetArray()) {
          if (std::string(x["action"].GetString()) != "NO CHANGE") {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert(
                {MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["price"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                              std::string(x["action"].GetString()) == "DELETE"
                                  ? "0"
                                  : UtilString::normalizeDecimalString(x["totalVolume"].GetString())});
            wsMessage.data[strcmp(side, "bids") == 0 ? MarketDataMessage::DataType::BID : MarketDataMessage::DataType::ASK]
                .push_back(std::move(dataPoint));
          }
        }
      }
      wsMessageList.push_back(std::move(wsMessage));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return wsMessageList;
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation,
                  const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) override {
    // TODO(cryptochassis): implement
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request,
                                                                       const std::string& textMessage,
                                                                       const TimePoint& timeReceived) override {
    std::vector<MarketDataMessage> marketDataMessageList;
    // TODO(cryptochassis): implement
    return marketDataMessageList;
  }
  std::map<std::string, int> nextExchangeSubscriptionIdByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_ERISX_H_
