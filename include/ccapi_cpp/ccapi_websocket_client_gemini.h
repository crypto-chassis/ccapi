#ifndef INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_GEMINI_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_GEMINI_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_GEMINI
#include "ccapi_cpp/ccapi_websocket_client.h"
namespace ccapi {
class WebsocketClientGemini final : public WebsocketClient {
 public:
  WebsocketClientGemini(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs): WebsocketClient(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs) {
    this->name = CCAPI_EXCHANGE_NAME_GEMINI;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketClient::onOpen(hdl);
//    this->onOpen_2(hdl);
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    for (const auto & subscriptionListByChannelIdProductId : this->subscriptionListByConnectionIdChannelIdProductIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdProductId.first;
      for (auto & subscriptionListByPair : subscriptionListByChannelIdProductId.second) {
        auto productId = subscriptionListByPair.first;
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
        if (marketDepthSubscribedToExchange == 1) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] = true;
        }
        auto exchangeSubscriptionId = wsConnection.url;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID] = productId;
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onTextMessage(wspp::connection_hdl hdl, std::string textMessage, TimePoint timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketClient::onTextMessage(hdl, textMessage, timeReceived);
//    this->onTextMessage_2(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<WebsocketMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage, TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = "+type);
    std::vector<WebsocketMessage> wsMessageList;
    if (this->sessionOptions.enableCheckSequence) {
      int sequence = document["socket_sequence"].GetInt();
      if (!this->checkSequence(wsConnection, sequence)) {
        this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, "");
        return wsMessageList;
      }
    }
    if (type == "update") {
      WebsocketMessage wsMessage;
      wsMessage.type = WebsocketMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.exchangeSubscriptionId = wsConnection.url;
      for (auto & event : document["events"].GetArray()) {
        auto gType = std::string(event["type"].GetString());
        if (gType == "change") {
          WebsocketMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(event["price"].GetString())});
          dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(event["remaining"].GetString())});
          auto isBid = std::string(event["side"].GetString()) == "bid";
          std::string reason = event["reason"].GetString();
          if (reason == "place" || reason == "cancel" || reason == "trade") {
            wsMessage.recapType = WebsocketMessage::RecapType::NONE;
            wsMessage.tp = TimePoint(std::chrono::milliseconds(document["timestampms"].GetInt64()));
            if (isBid) {
              wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          } else if (reason == "initial") {
            wsMessage.recapType = WebsocketMessage::RecapType::SOLICITED;
            if (isBid) {
              wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          } else if (reason == "top-of-book") {
            wsMessage.tp = TimePoint(std::chrono::milliseconds(document["timestampms"].GetInt64()));
            wsMessage.recapType = WebsocketMessage::RecapType::NONE;
            if (isBid) {
              wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          }
        } else if (gType == "trade") {
          //        wsMessage.recapType = WebsocketMessage::RecapType::NONE;
          //        auto makerSide = std::string(event["makerSide"].GetString());
          //        if (makerSide == "bid" || makerSide == "ask") {
          //          Element element;
          //          element.insert(nameLastPrice, UtilString::normalizeDecimalString(event["price"].GetString()));
          //          element.insert(nameLastSize, UtilString::normalizeDecimalString(event["amount"].GetString()));
          //          elementList.push_back(element);
          //        }
        }
      }
      wsMessageList.push_back(std::move(wsMessage));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return wsMessageList;
  }
  std::map<std::string, SubscriptionList> groupSubscriptionListByUrl(const SubscriptionList& subscriptionList) {
    std::map<std::string, std::set<std::string> > parameterBySymbolMap;
    for (auto const& subscription : subscriptionList.getSubscriptionList()) {
      auto symbol = this->sessionConfigs.getExchangePairSymbolMap().at(this->name).at(subscription.getPair());
      auto fieldSet = subscription.getFieldSet();
      for (auto const& field : fieldSet) {
        auto parameterList = UtilString::split(this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(CCAPI_EXCHANGE_NAME_GEMINI).at(field), ",");
        std::set<std::string> parameterSet(parameterList.begin(), parameterList.end());
        parameterBySymbolMap[symbol].insert(parameterSet.begin(), parameterSet.end());
      }
    }
    std::map<std::string, SubscriptionList> subscriptionListByUrlMap;
    for (auto const& subscription : subscriptionList.getSubscriptionList()) {
      auto symbol = this->sessionConfigs.getExchangePairSymbolMap().at(this->name).at(subscription.getPair());
      std::string url = this->baseUrl + symbol;
      url += "?";
      std::set<std::string> parameterSet = parameterBySymbolMap[symbol];
      if ((parameterSet.find(CCAPI_EXCHANGE_NAME_WEBSOCKET_GEMINI_PARAMETER_BIDS) != parameterSet.end()
              || parameterSet.find(CCAPI_EXCHANGE_NAME_WEBSOCKET_GEMINI_PARAMETER_OFFERS) != parameterSet.end())
      ) {
        auto optionMap = subscription.getOptionMap();
        if (std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX)) == 1) {
          parameterSet.insert(CCAPI_EXCHANGE_NAME_WEBSOCKET_GEMINI_PARAMETER_TOP_OF_BOOK);
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
      subscriptionListByUrlMap[url].add(subscription);
    }
    return subscriptionListByUrlMap;
  }
  std::map<std::string, WebsocketConnection> buildWebsocketConnectionMap(std::string url, const SubscriptionList& subscriptionList) override {
    std::map<std::string, WebsocketConnection> wsConnectionMap;
    for (const auto & x : this->groupSubscriptionListByUrl(this->subscriptionList)) {
      WebsocketConnection wsConnection(x.first, x.second);
      wsConnectionMap.insert(std::pair<std::string, WebsocketConnection>(wsConnection.id, wsConnection));
    }
    return wsConnectionMap;
  }
  bool checkSequence(const WebsocketConnection& wsConnection, int sequence) {
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) == this->sequenceByConnectionIdMap.end()) {
      if (sequence != this->sessionConfigs.getInitialSequenceByExchangeMap().at(this->name)) {
        CCAPI_LOGGER_WARN("incorrect initial sequence, wsConnection = "+toString(wsConnection));
        return false;
      }
      this->sequenceByConnectionIdMap.insert(std::pair<std::string, int>(wsConnection.id, sequence));
      return true;
    } else {
      CCAPI_LOGGER_DEBUG("sequence: previous = "+toString(this->sequenceByConnectionIdMap[wsConnection.id])+
          ", current = "+toString(sequence)+"wsConnection = "+toString(wsConnection));
      if (sequence-this->sequenceByConnectionIdMap[wsConnection.id] == 1) {
        this->sequenceByConnectionIdMap[wsConnection.id] = sequence;
        return true;
      } else {
        return false;
      }
    }
  }
  void onOutOfSequence(WebsocketConnection& wsConnection, int sequence, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived,
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
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    this->sequenceByConnectionIdMap.erase(wsConnection.id);
    WebsocketClient::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::map<std::string, int> sequenceByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_GEMINI_H_
