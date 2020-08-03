#ifndef INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_BITSTAMP_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_BITSTAMP_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_BITSTAMP
#include "ccapi_cpp/ccapi_websocket_client.h"
namespace ccapi {
class WebsocketClientBitstamp final : public WebsocketClient {
 public:
  WebsocketClientBitstamp(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs): WebsocketClient(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs) {
    this->name = CCAPI_EXCHANGE_NAME_BITSTAMP;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketClient::onOpen(hdl);
//    this->onOpen_2(hdl);
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    std::vector<std::string> requestStringList;
    for (const auto & subscriptionListByChannelIdProductId : this->subscriptionListByConnectionIdChannelIdProductIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdProductId.first;
      for (auto & subscriptionListByPair : subscriptionListByChannelIdProductId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("bts:subscribe").Move(), allocator);
        rj::Value data(rj::kObjectType);
        auto productId = subscriptionListByPair.first;
        if (channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] = true;
        }
        std::string exchangeSubscriptionId = channelId + "_" + productId;
        data.AddMember("channel", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        document.AddMember("data", data, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string requestString = stringBuffer.GetString();
        requestStringList.push_back(std::move(requestString));
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID] = productId;
        CCAPI_LOGGER_TRACE("this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    CCAPI_LOGGER_TRACE("this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap = "+toString(this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap));
    for (const auto & requestString : requestStringList) {
      CCAPI_LOGGER_INFO("requestString = "+requestString);
      ErrorCode ec;
      this->send(hdl, requestString, wspp::frame::opcode::text, ec);
      if (ec) {
        CCAPI_LOGGER_ERROR(ec.message());
        // TODO(cryptochassis): implement
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onTextMessage(wspp::connection_hdl hdl, std::string textMessage, TimePoint timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    TlsClient::connection_ptr con = this->tlsClient.get_con_from_hdl(hdl);
    WebsocketClient::onTextMessage(hdl, textMessage, timeReceived);
//    this->onTextMessage_2(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<WebsocketMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage, TimePoint& timeReceived) override {
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    rj::Document document;
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.Parse(textMessage.c_str());
    std::vector<WebsocketMessage> wsMessageList;
    const rj::Value& data = document["data"];
    if (document.IsObject() && document.HasMember("event") && std::string(document["event"].GetString()) == "bts:subscription_succeeded") {
    } else if (document.IsObject() && document.HasMember("event") && std::string(document["event"].GetString()) == "data") {
      WebsocketMessage wsMessage;
      std::string exchangeSubscriptionId = document["channel"].GetString();
      std::string channelId = this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID];
      std::string productId = this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
      CCAPI_LOGGER_TRACE("channel = "+channelId);
      if (channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
        wsMessage.type = WebsocketMessage::Type::MARKET_DATA_EVENTS;
        if (this->processedInitialSnapshotByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId]) {
          wsMessage.recapType = WebsocketMessage::RecapType::NONE;
        } else {
          wsMessage.recapType = WebsocketMessage::RecapType::SOLICITED;
        }
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        wsMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
        for (const auto& x : data["bids"].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          WebsocketMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
          ++bidIndex;
        }
        int askIndex = 0;
        for (const auto& x : data["asks"].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          WebsocketMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
          ++askIndex;
        }
        wsMessageList.push_back(std::move(wsMessage));
      }
    }
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_BITSTAMP_H_
