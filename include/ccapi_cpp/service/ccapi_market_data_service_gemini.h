#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
#ifdef ENABLE_SERVICE_MARKET_DATA
#ifdef ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGemini final : public MarketDataService {
 public:
  MarketDataServiceGemini(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr): MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->name = CCAPI_EXCHANGE_NAME_GEMINI;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) override {
    return std::vector<std::string>();
  }
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    for (const auto & subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto & subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
        if (marketDepthSubscribedToExchange == 1) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        auto exchangeSubscriptionId = wsConnection.url;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->sequenceByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = "+type);
    std::vector<MarketDataMessage> wsMessageList;
    if (this->sessionOptions.enableCheckSequence) {
      int sequence = document["socket_sequence"].GetInt();
      if (!this->checkSequence(wsConnection, sequence)) {
        this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, "");
        return wsMessageList;
      }
    }
    if (type == "update") {
      MarketDataMessage wsMessage;
      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.exchangeSubscriptionId = wsConnection.url;
      for (auto & event : document["events"].GetArray()) {
        auto gType = std::string(event["type"].GetString());
        if (gType == "change") {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(event["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(event["remaining"].GetString())});
          auto isBid = std::string(event["side"].GetString()) == "bid";
          std::string reason = event["reason"].GetString();
          if (reason == "place" || reason == "cancel" || reason == "trade") {
            wsMessage.recapType = MarketDataMessage::RecapType::NONE;
            wsMessage.tp = TimePoint(std::chrono::milliseconds(document["timestampms"].GetInt64()));
            if (isBid) {
              wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          } else if (reason == "initial") {
            wsMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            if (isBid) {
              wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          } else if (reason == "top-of-book") {
            wsMessage.tp = TimePoint(std::chrono::milliseconds(document["timestampms"].GetInt64()));
            wsMessage.recapType = MarketDataMessage::RecapType::NONE;
            if (isBid) {
              wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          }
        } else if (gType == "trade") {
          // TODO(cryptochassis): implement
        }
      }
      wsMessageList.push_back(std::move(wsMessage));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return wsMessageList;
  }
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto symbol = this->convertInstrumentToWebsocketSymbolId(subscription.getInstrument());
    auto field = subscription.getField();
    auto parameterList = UtilString::split(this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(this->name).at(field), ",");
    std::set<std::string> parameterSet(parameterList.begin(), parameterList.end());
    std::string url = this->baseUrl + symbol;
    url += "?";
    if ((parameterSet.find(CCAPI_WEBSOCKET_GEMINI_PARAMETER_BIDS) != parameterSet.end()
            || parameterSet.find(CCAPI_WEBSOCKET_GEMINI_PARAMETER_OFFERS) != parameterSet.end())
    ) {
      auto optionMap = subscription.getOptionMap();
      if (std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)) == 1) {
        parameterSet.insert(CCAPI_WEBSOCKET_GEMINI_PARAMETER_TOP_OF_BOOK);
      }
    }
    parameterSet.insert("heartbeat");
    bool isFirstParameter = true;
    for (auto const& parameter : parameterSet) {
      if (isFirstParameter) {
        isFirstParameter = false;
      } else {
        url += "&";
      }
      url += parameter+"=true";
    }
    return url;
  }
  bool checkSequence(const WsConnection& wsConnection, int sequence) {
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) == this->sequenceByConnectionIdMap.end()) {
      if (sequence != this->sessionConfigs.getInitialSequenceByExchangeMap().at(this->name)) {
        CCAPI_LOGGER_WARN("incorrect initial sequence, wsConnection = "+toString(wsConnection));
        return false;
      }
      this->sequenceByConnectionIdMap.insert(std::pair<std::string, int>(wsConnection.id, sequence));
      return true;
    } else {
      CCAPI_LOGGER_DEBUG("sequence: previous = "+toString(this->sequenceByConnectionIdMap[wsConnection.id])+
          ", current = "+toString(sequence)+", wsConnection = "+toString(wsConnection));
      if (sequence-this->sequenceByConnectionIdMap[wsConnection.id] == 1) {
        this->sequenceByConnectionIdMap[wsConnection.id] = sequence;
        return true;
      } else {
        return false;
      }
    }
  }
  void onOutOfSequence(WsConnection& wsConnection, int sequence, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived,
      const std::string& exchangeSubscriptionId) {
    int previous = 0;
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) != this->sequenceByConnectionIdMap.end()) {
      previous = this->sequenceByConnectionIdMap[wsConnection.id];
    }
    CCAPI_LOGGER_ERROR("out of sequence: previous = "+toString(previous)+
        ", current = "+toString(sequence)+
        ", connection = "+toString(wsConnection)+
        ", textMessage = "+textMessage+
        ", timeReceived = "+UtilTime::getISOTimestamp(timeReceived));
    ErrorCode ec;
    this->close(wsConnection, hdl, websocketpp::close::status::normal, "out of sequence", ec);
    if (ec) {
      CCAPI_LOGGER_ERROR(ec.message());
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
  }
  std::map<std::string, int> sequenceByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
