#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_FTX)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceFTX CCAPI_FINAL : public MarketDataService {
 public:
  MarketDataServiceFTX(std::function<void(Event& event)> wsEventHandler,
                       SessionOptions sessionOptions,
                       SessionConfigs sessionConfigs,
                       std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs,
                          serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest =
        this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostFromUrl(this->baseUrlRest);
    this->getRecentTradesTarget =
        "/products/<product-id>/trades";  // todo update
  }

 private:
  void pingOnApplicationLevel(wspp::connection_hdl hdl,
                              ErrorCode& ec) override {
    this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec);
  }

 private:
  std::vector<std::string> createRequestStringList(
      const WsConnection& wsConnection) override {
    std::vector<std::string> requestStringList;
    for (const auto& subscriptionListByChannelIdSymbolId :
         this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(
             wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      rj::Value channel(rj::kObjectType);
      rj::Value symbolIds(rj::kArrayType);
      for (const auto& subscriptionListBySymbolId :
           subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("subscribe").Move(), allocator);
        std::string symbolId = subscriptionListBySymbolId.first;
        symbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(),
                           allocator);
        std::string exchangeSubscriptionId = channelId + "|" + symbolId;
        std::string market = symbolId;
        std::string channelIdString = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap
            [wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] =
            channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap
            [wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] =
            symbolId;
        document.AddMember("channel",
                           rj::Value(channelIdString.c_str(), allocator).Move(),
                           allocator);
        document.AddMember(
            "market", rj::Value(market.c_str(), allocator).Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string requestString = stringBuffer.GetString();
        requestStringList.push_back(requestString);
      }
      return requestStringList;
    }
    std::vector<MarketDataMessage> processTextMessage(
        WsConnection & wsConnection, wspp::connection_hdl hdl,
        const std::string& textMessage, const TimePoint& timeReceived)
        override {
      CCAPI_LOGGER_FUNCTION_ENTER;
      rj::Document document;
      document.Parse(textMessage.c_str());
      std::vector<MarketDataMessage> marketDataMessageList;
      auto type = std::string(document["type"].GetString());
      CCAPI_LOGGER_TRACE("type = " + type);
      if (type == "update") {
        auto symbolId = std::string(document["market"].GetString());
        auto exchangeSubscriptionId =
            std::string(CCAPI_WEBSOCKET_FTX_CHANNEL_ORDER_BOOK) + "|" +
            symbolId;
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;

        auto timePair = UtilTime::divide(
            std::string(std::to_string(document["data"]["time"].GetDouble())));
        auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
        tp += std::chrono::nanoseconds(timePair.second);
        marketDataMessage.tp = tp;

        //                marketDataMessage.tp =
        //                UtilTime::parse(std::string(std::to_string(document["data"]["time"].GetFloat())));
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        const rj::Value& asks = document["data"]["asks"];
        const rj::Value& bids = document["data"]["bids"];

        for (auto& ask : asks.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE,
                            UtilString::normalizeDecimalString(
                                std::to_string(ask[0].GetFloat()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                            UtilString::normalizeDecimalString(
                                std::to_string(ask[1].GetFloat()))});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(
              std::move(dataPoint));
        }
        for (auto& bid : bids.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE,
                            UtilString::normalizeDecimalString(
                                std::to_string(bid[0].GetFloat()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                            UtilString::normalizeDecimalString(
                                std::to_string(bid[1].GetFloat()))});
          marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(
              std::move(dataPoint));
        }
        marketDataMessageList.push_back(std::move(marketDataMessage));
      } else if (type == "fills") {
        auto symbolId = std::string(document["data"]["market"].GetString());
        auto exchangeSubscriptionId =
            std::string(CCAPI_WEBSOCKET_FTX_CHANNEL_TRADES) + "|" + symbolId;
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp =
            UtilTime::parse(std::string(document["data"]["time"].GetString()));
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert(
            {MarketDataMessage::DataFieldType::PRICE,
             UtilString::normalizeDecimalString(std::string(
                 std::to_string(document["data"]["price"].GetFloat())))});
        dataPoint.insert(
            {MarketDataMessage::DataFieldType::SIZE,
             UtilString::normalizeDecimalString(std::string(
                 std::to_string(document["data"]["size"].GetFloat())))});
        dataPoint.insert(
            {MarketDataMessage::DataFieldType::TRADE_ID,
             std::to_string(document["data"]["tradeId"].GetInt64())});
        dataPoint.insert(
            {MarketDataMessage::DataFieldType::IS_BUYER_MAKER,
             std::string(document["data"]["side"].GetString()) == "buy" ? "1"
                                                                        : "0"});
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(
            std::move(dataPoint));
        marketDataMessageList.push_back(std::move(marketDataMessage));
      } else if (type == "heartbeat") {
        // TODO(cryptochassis): implement
      } else if (type == "partial") {
        auto symbolId = std::string(document["market"].GetString());
        auto exchangeSubscriptionId =
            std::string(CCAPI_WEBSOCKET_FTX_CHANNEL_ORDER_BOOK) + "|" +
            symbolId;
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;

        auto timePair = UtilTime::divide(
            std::string(std::to_string(document["data"]["time"].GetDouble())));
        auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
        tp += std::chrono::nanoseconds(timePair.second);
        marketDataMessage.tp = tp;

        //                marketDataMessage.tp = timeReceived;
        const rj::Value& bids = document["data"]["bids"];
        for (auto& x : bids.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE,
                            UtilString::normalizeDecimalString(
                                std::to_string(x[0].GetFloat()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                            UtilString::normalizeDecimalString(
                                std::to_string(x[1].GetFloat()))});
          marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(
              std::move(dataPoint));
        }
        const rj::Value& asks = document["data"]["asks"];
        for (auto& x : asks.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE,
                            UtilString::normalizeDecimalString(
                                std::to_string(x[0].GetFloat()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                            UtilString::normalizeDecimalString(
                                std::to_string(x[1].GetFloat()))});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(
              std::move(dataPoint));
        }
        marketDataMessageList.push_back(std::move(marketDataMessage));
      } else if (type == "subscriptions") {
        // TODO(cryptochassis): implement
      } else if (type == "error") {
        // TODO(cryptochassis): implement
      }
      CCAPI_LOGGER_FUNCTION_EXIT;
      return marketDataMessageList;
    }

    void convertReq(http::request<http::string_body> & req,
                    const Request& request, const Request::Operation operation,
                    const TimePoint& now, const std::string& symbolId,
                    const std::map<std::string, std::string>& credential)
        override {
      switch (operation) {
        // TODO: implement
        case Request::Operation::GET_RECENT_TRADES: {
          req.method(http::verb::get);
          auto target = this->getRecentTradesTarget;
          std::string queryString;
          const std::map<std::string, std::string> param =
              request.getFirstParamWithDefault();
          this->appendSymbolId(queryString, symbolId, "symbol");
          req.target(target + "?" + queryString);
        } break;
        default:
          CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
      }
    }
    void processSuccessfulTextMessage(const Request& request,
                                      const std::string& textMessage,
                                      const TimePoint& timeReceived) override {
      const std::string& quotedTextMessage = std::regex_replace(
          textMessage, std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*)"),
          "$1\"$2\"");
      CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
      MarketDataService::processSuccessfulTextMessage(
          request, quotedTextMessage, timeReceived);
    }
    std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(
        const Request& request, const std::string& textMessage,
        const TimePoint& timeReceived) override {
      rj::Document document;
      document.Parse(textMessage.c_str());
      std::vector<MarketDataMessage> marketDataMessageList;
      auto operation = request.getOperation();
      switch (operation) {
        case Request::Operation::GET_RECENT_TRADES: {
          for (const auto& x : document["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type =
                MarketDataMessage::Type::MARKET_DATA_EVENTS;
            std::string timeStr = x["time"].GetString();
            marketDataMessage.tp = UtilTime::makeTimePoint(std::make_pair(
                std::stoll(timeStr.substr(0, timeStr.length() - 9)),
                std::stoll(timeStr.substr(timeStr.length() - 9))));
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE,
                              UtilString::normalizeDecimalString(
                                  std::string(x["price"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE,
                              UtilString::normalizeDecimalString(
                                  std::string(x["size"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID,
                              std::string(x["sequence"].GetString())});
            dataPoint.insert(
                {MarketDataMessage::DataFieldType::IS_BUYER_MAKER,
                 std::string(x["side"].GetString()) == "sell" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE]
                .push_back(std::move(dataPoint));
            marketDataMessageList.push_back(std::move(marketDataMessage));
          }
        } break;
        default:
          CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
      }
      return marketDataMessageList;
    }
    std::map<std::string, int> nextRequestIdByConnectionIdMap;
  };
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
