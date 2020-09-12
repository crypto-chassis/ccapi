#ifndef INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_OKEX
#include "ccapi_cpp/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceOkex final : public MarketDataService {
 public:
  MarketDataServiceOkex(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContext& serviceContext): MarketDataService(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContext) {
    this->name = CCAPI_EXCHANGE_NAME_OKEX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataService::onOpen(hdl);
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(this->tlsClient->get_con_from_hdl(hdl));
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
//        args.PushBack(rj::Value(productId.c_str(), allocator).Move(), allocator);
        std::string exchangeSubscriptionId = channelId+
        ":"+productId;
        args.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID] = productId;
      }
    }
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
    MarketDataService::onTextMessage(hdl, textMessage, timeReceived);
//    this->onTextMessage_2(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage, TimePoint& timeReceived) override {
//    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(this->tlsClient->get_con_from_hdl(hdl));
//    rj::Document document;
//    rj::Document::AllocatorType& allocator = document.GetAllocator();
//    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*[Ee]?-?\\d*)"), "$1\"$2\"");
////    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*)"), "$1\"$2\"");
//    CCAPI_LOGGER_TRACE("quotedTextMessage = "+quotedTextMessage);
//    document.Parse(quotedTextMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
//    if (document.IsObject() && document.HasMember("ch") && document.HasMember("tick")) {
//      MarketDataMessage wsMessage;
//      std::string exchangeSubscriptionId = document["ch"].GetString();
//      std::string channelId = this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID];
//      std::string productId = this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID];
//      auto optionMap = this->optionMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
//      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
//      CCAPI_LOGGER_TRACE("channel = "+channelId);
//      wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
//      if (std::regex_search(channelId, std::regex(CCAPI_EXCHANGE_NAME_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL_REGEX))) {
////        CCAPI_LOGGER_TRACE("it is trade");
////        //  id always increasing?
////        wsMessage.recapType = MarketDataMessage::RecapType::NONE;
////        wsMessage.tp = TimePoint(std::chrono::milliseconds(document["tick"]["ts"].GetInt64()));
////        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
////        const rj::Value& data = document["tick"]["data"];
////        for (const auto& x : data.GetArray()) {
////          MarketDataMessage::TypeForDataPoint dataPoint;
////          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["price"].GetString())});
////          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x["amount"].GetString())});
////          dataPoint.insert({MarketDataMessage::DataFieldType::ID, x["tradeId"].GetString()});
////          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
////          wsMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
////        }
//      } else if (std::regex_search(channelId, std::regex(CCAPI_EXCHANGE_NAME_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH_REGEX))) {
//        CCAPI_LOGGER_TRACE("it is snapshot");
//        if (this->processedInitialSnapshotByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId]) {
//          wsMessage.recapType = MarketDataMessage::RecapType::NONE;
//        } else {
//          wsMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
//        }
//        const rj::Value& data = document["tick"];
//        std::string ts = data["ts"].GetString();
//        ts.insert(ts.size() - 3, ".");
//        wsMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
//        CCAPI_LOGGER_TRACE("wsMessage.tp = " + toString(wsMessage.tp));
//        wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
//        int bidIndex = 0;
//        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
//        for (const auto& x : data["bids"].GetArray()) {
//          if (bidIndex >= maxMarketDepth) {
//            break;
//          }
//          MarketDataMessage::TypeForDataPoint dataPoint;
//          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
//          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
//          wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
//          ++bidIndex;
//        }
//        int askIndex = 0;
//        for (const auto& x : data["asks"].GetArray()) {
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
//      }
//    } else if (document.IsObject() && document.HasMember("ping")) {
//      std::string payload("{\"pong\":");
//      payload += document["ping"].GetString();
//      payload += "}";
//      ErrorCode ec;
//      this->send(hdl, payload, wspp::frame::opcode::text, ec);
//      if (ec) {
//        CCAPI_LOGGER_ERROR(ec.message());
//        // TODO(cryptochassis): implement
//      }
//    } else if (document.IsObject() && document.HasMember("status") && document.HasMember("subbed")) {
//    }
    return wsMessageList;
  }
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
