#ifndef INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_COINBASE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_COINBASE_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_COINBASE
#include "ccapi_cpp/ccapi_websocket_client.h"
namespace ccapi {
class WebsocketClientCoinbase final : public WebsocketClient {
 public:
  WebsocketClientCoinbase(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs): WebsocketClient(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs) {
    this->name = CCAPI_EXCHANGE_NAME_COINBASE;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketClient::onOpen(hdl);
//    this->onOpen_2(hdl);
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    std::vector<std::string> requestStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("type", rj::Value("subscribe").Move(), allocator);
    rj::Value channels(rj::kArrayType);
    for (const auto & subscriptionListByChannelIdProductId : this->subscriptionListByConnectionIdChannelIdProductIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdProductId.first;
      rj::Value channel(rj::kObjectType);
      rj::Value productIds(rj::kArrayType);
      for (const auto & subscriptionListByProductId : subscriptionListByChannelIdProductId.second) {
        std::string productId = subscriptionListByProductId.first;
        productIds.PushBack(rj::Value(productId.c_str(), allocator).Move(), allocator);
        std::string exchangeSubscriptionId = channelId+
        "|"+productId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID] = productId;
      }
      channel.AddMember("name", rj::Value(channelId.c_str(), allocator).Move(), allocator);
      channel.AddMember("product_ids", productIds, allocator);
      channels.PushBack(channel, allocator);
      rj::Value heartbeatChannel(rj::kObjectType);
      heartbeatChannel.AddMember("name", rj::Value("heartbeat").Move(), allocator);
      rj::Value heartbeatProductIds(rj::kArrayType);
      for (const auto & subscriptionListByProductId : subscriptionListByChannelIdProductId.second) {
        heartbeatProductIds.PushBack(rj::Value(subscriptionListByProductId.first.c_str(), allocator).Move(), allocator);
      }
      heartbeatChannel.AddMember("product_ids", heartbeatProductIds, allocator);
      channels.PushBack(heartbeatChannel, allocator);
    }
    document.AddMember("channels", channels, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string requestString = stringBuffer.GetString();
    requestStringList.push_back(requestString);
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
    WebsocketClient::onTextMessage(hdl, textMessage, timeReceived);
//    this->onTextMessage_2(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<WebsocketMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage, TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<WebsocketMessage> wsMessageList;
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = "+type);
    if (type == "l2update") {
      auto productId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_EXCHANGE_NAME_WEBSOCKET_COINBASE_CHANNEL_LEVEL2)+
      "|"+productId;
      WebsocketMessage wsMessage;
      wsMessage.type = WebsocketMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      wsMessage.tp = UtilTime::parse(std::string(document["time"].GetString()));
      wsMessage.recapType = WebsocketMessage::RecapType::NONE;
      const rj::Value& changes = document["changes"];
      for (auto& change : changes.GetArray()) {
        auto side = std::string(change[0].GetString());
        WebsocketMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(change[1].GetString())});
        dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(change[2].GetString())});
        if (side == "buy") {
          wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
        } else {
          wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
        }
      }
      wsMessageList.push_back(std::move(wsMessage));
    } else if (type == "match") {
      // TODO(cryptochassis): implement
    } else if (type == "heartbeat") {
      CCAPI_LOGGER_DEBUG("heartbeat: "+toString(wsConnection));
    } else if (type == "snapshot") {
      auto productId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_EXCHANGE_NAME_WEBSOCKET_COINBASE_CHANNEL_LEVEL2)+
      "|"+productId;
      WebsocketMessage wsMessage;
      wsMessage.type = WebsocketMessage::Type::MARKET_DATA_EVENTS;
      wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      wsMessage.recapType = WebsocketMessage::RecapType::SOLICITED;
      const rj::Value& bids = document["bids"];
      for (auto& x : bids.GetArray()) {
        WebsocketMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
        dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
        wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
      }
      const rj::Value& asks = document["asks"];
      for (auto& x : asks.GetArray()) {
        WebsocketMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
        dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
        wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
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
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_COINBASE_H_
