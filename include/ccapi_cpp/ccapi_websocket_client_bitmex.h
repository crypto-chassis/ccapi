#ifndef INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_BITMEX_H_
#define INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_BITMEX_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_BITMEX
#include "ccapi_cpp/ccapi_websocket_client.h"
namespace ccapi {
class WebsocketClientBitmex final : public WebsocketClient {
 public:
  WebsocketClientBitmex(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs): WebsocketClient(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs) {
    this->name = CCAPI_EXCHANGE_NAME_BITMEX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    this->priceByConnectionIdChannelIdProductIdPriceIdMap.erase(wsConnection.id);
    WebsocketClient::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::string> > > > priceByConnectionIdChannelIdProductIdPriceIdMap;
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketClient::onOpen(hdl);
//    this->onOpen_2(hdl);
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    std::vector<std::string> requestStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto & subscriptionListByChannelIdProductId : this->subscriptionListByConnectionIdChannelIdProductIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdProductId.first;
      for (const auto & subscriptionListByProductId : subscriptionListByChannelIdProductId.second) {
        std::string productId = subscriptionListByProductId.first;
        if (channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_QUOTE || channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] = true;
        }
        std::string exchangeSubscriptionId = channelId+
        ":"+productId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID] = productId;
        args.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
      }
    }
    CCAPI_LOGGER_TRACE("this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap));
    document.AddMember("args", args, allocator);
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
    TlsClient::connection_ptr con = this->tlsClient.get_con_from_hdl(hdl);
    WebsocketClient::onTextMessage(hdl, textMessage, timeReceived);
//    this->onTextMessage_2(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<WebsocketMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage, TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WebsocketConnection& wsConnection = this->getWebsocketConnectionFromConnectionPtr(this->tlsClient.get_con_from_hdl(hdl));
    rj::Document document;
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*)"), "$1\"$2\"");
    CCAPI_LOGGER_TRACE("quotedTextMessage = "+quotedTextMessage);
    document.Parse(quotedTextMessage.c_str());
    std::vector<WebsocketMessage> wsMessageList;
    if (document.IsObject() && document.HasMember("table")) {
      std::string channelId = document["table"].GetString();
      if (channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10 || channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_QUOTE) {
        std::string action = document["action"].GetString();
        WebsocketMessage::RecapType recapType;
        if (action == "partial") {
          recapType = WebsocketMessage::RecapType::SOLICITED;
        } else {
          recapType = WebsocketMessage::RecapType::NONE;
        }
        int i = 0;
        std::string productId;
        std::string exchangeSubscriptionId;
        for (const auto& x : document["data"].GetArray()) {
          if (i == 0) {
            productId = x["symbol"].GetString();
            exchangeSubscriptionId = channelId + ":" + productId;
          }
          WebsocketMessage wsMessage;
          wsMessage.type = WebsocketMessage::Type::MARKET_DATA_EVENTS;
          wsMessage.recapType = recapType;
          wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          wsMessage.tp = UtilTime::parse(std::string(x["timestamp"].GetString()));
          if (channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_QUOTE) {
            WebsocketMessage::TypeForDataPoint dataPointBid;
            dataPointBid.insert({WebsocketMessage::DataFieldType::PRICE, UtilString:: normalizeDecimalString(x["bidPrice"].GetString())});
            dataPointBid.insert({WebsocketMessage::DataFieldType::SIZE, UtilString:: normalizeDecimalString(x["bidSize"].GetString())});
            wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPointBid));
            WebsocketMessage::TypeForDataPoint dataPointAsk;
            dataPointAsk.insert({WebsocketMessage::DataFieldType::PRICE, UtilString:: normalizeDecimalString(x["askPrice"].GetString())});
            dataPointAsk.insert({WebsocketMessage::DataFieldType::SIZE, UtilString:: normalizeDecimalString(x["askSize"].GetString())});
            wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPointAsk));
          } else {
            for (const auto& y : x["bids"].GetArray()) {
              WebsocketMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString:: normalizeDecimalString(y[0].GetString())});
              dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString:: normalizeDecimalString(y[1].GetString())});
              wsMessage.data[WebsocketMessage::DataType::BID].push_back(std::move(dataPoint));
            }
            for (const auto& y : x["asks"].GetArray()) {
              WebsocketMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, UtilString:: normalizeDecimalString(y[0].GetString())});
              dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, UtilString:: normalizeDecimalString(y[1].GetString())});
              wsMessage.data[WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          }
          wsMessageList.push_back(std::move(wsMessage));
          ++i;
        }
      } else if (channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2 || channelId == CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2_25) {
        std::string action = document["action"].GetString();
        WebsocketMessage wsMessage;
        wsMessage.type = WebsocketMessage::Type::MARKET_DATA_EVENTS;
        wsMessage.tp = timeReceived;
        if (action == "partial") {
          wsMessage.recapType = WebsocketMessage::RecapType::SOLICITED;
        } else {
          wsMessage.recapType = WebsocketMessage::RecapType::NONE;
        }
        int i = 0;
        std::string productId;
        std::string exchangeSubscriptionId;
        for (const auto& x : document["data"].GetArray()) {
          if (i == 0) {
            productId = x["symbol"].GetString();
            exchangeSubscriptionId = channelId + ":" + productId;
            wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          }
          WebsocketMessage::TypeForDataPoint dataPoint;
          std::string price;
          std::string size;
          std::string priceId = x["id"].GetString();
          if (action == "insert" || action == "partial") {
            price = UtilString:: normalizeDecimalString(x["price"].GetString());
            size = UtilString:: normalizeDecimalString(x["size"].GetString());
            this->priceByConnectionIdChannelIdProductIdPriceIdMap[wsConnection.id][channelId][productId][priceId] = price;
          } else {
            price = this->priceByConnectionIdChannelIdProductIdPriceIdMap[wsConnection.id][channelId][productId][priceId];
            if (price.empty()) {
              this->onIncorrectStatesFound(wsConnection,
                  hdl, textMessage, timeReceived, exchangeSubscriptionId, "bitmex update for missing item came through on wsConnection = "+
                  toString(wsConnection)+", channelId = "+channelId+", productId = "+productId+", priceId = "+ priceId + ". Data: " + toString(this->priceByConnectionIdChannelIdProductIdPriceIdMap[wsConnection.id][channelId][productId]));
            }
            if (action == "update") {
              size = UtilString:: normalizeDecimalString(x["size"].GetString());
            } else if (action == "delete") {
              size = "0";
            }
          }
          std::string side = x["side"].GetString();
          dataPoint.insert({WebsocketMessage::DataFieldType::PRICE, price});
          dataPoint.insert({WebsocketMessage::DataFieldType::SIZE, size});
          wsMessage.data[side == "Buy" ? WebsocketMessage::DataType::BID : WebsocketMessage::DataType::ASK].push_back(std::move(dataPoint));
          ++i;
        }
        if (i > 0) {
          wsMessageList.push_back(std::move(wsMessage));
        }
      }
    } else if (document.IsObject() && document.HasMember("subscribe") && document["success"].GetBool()) {
      // TODO(cryptochassis): implement
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_WEBSOCKET_CLIENT_BITMEX_H_
