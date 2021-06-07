#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceFtx : public MarketDataService {
 public:
  MarketDataServiceFtx(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                       std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->shouldAlignSnapshot = true;
    this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->getRecentTradesTarget = "/api/markets/{market_name}/trades";
    this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*[eE]?-?\\d*)");
  }
  virtual ~MarketDataServiceFtx() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      rj::Value channel(rj::kObjectType);
      rj::Value symbolIds(rj::kArrayType);
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("subscribe").Move(), allocator);
        std::string symbolId = subscriptionListBySymbolId.first;
        symbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        std::string exchangeSubscriptionId = channelId + "|" + symbolId;
        std::string market = symbolId;
        std::string channelIdString = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        document.AddMember("channel", rj::Value(channelIdString.c_str(), allocator).Move(), allocator);
        document.AddMember("market", rj::Value(market.c_str(), allocator).Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
    return sendStringList;
  }
  std::string calculateOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk) override {
    auto i = 0;
    auto i1 = snapshotBid.rbegin();
    auto i2 = snapshotAsk.begin();
    std::vector<std::string> csData;
    while (i < 100 && (i1 != snapshotBid.rend() || i2 != snapshotAsk.end())) {
      if (i1 != snapshotBid.rend()) {
        csData.push_back(toString(i1->first));
        csData.push_back(i1->second);
        ++i1;
      }
      if (i2 != snapshotAsk.end()) {
        csData.push_back(toString(i2->first));
        csData.push_back(i2->second);
        ++i2;
      }
      ++i;
    }
    std::string csStr = UtilString::join(csData, ":");
    CCAPI_LOGGER_DEBUG("csStr = " + csStr);
    uint_fast32_t csCalc = UtilAlgorithm::crc(csStr.begin(), csStr.end());
    return intToHex(csCalc);
  }
  std::vector<MarketDataMessage> processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage,
                                                    const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    rj::Document document;
    const std::string& quotedTextMessage = this->convertNumberToStringInJson(textMessage);
    CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
    document.Parse(quotedTextMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = " + type);
    if (type == "update") {
      const rj::Value& data = document["data"];
      auto symbolId = std::string(document["market"].GetString());
      auto channel = std::string(document["channel"].GetString());
      CCAPI_LOGGER_TRACE("channel = " + channel);
      auto exchangeSubscriptionId = channel + "|" + symbolId;
      if (channel == CCAPI_WEBSOCKET_FTX_CHANNEL_ORDERBOOKS) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        auto timePair = UtilTime::divide(data["time"].GetString());
        auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
        tp += std::chrono::nanoseconds(timePair.second);
        marketDataMessage.tp = tp;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        const rj::Value& asks = data["asks"];
        const rj::Value& bids = data["bids"];
        for (auto& ask : asks.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          if (this->sessionOptions.enableCheckOrderBookChecksum) {
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, ask[0].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, ask[1].GetString()});
          } else {
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(ask[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(ask[1].GetString())});
          }
          marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
        }
        for (auto& bid : bids.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          if (this->sessionOptions.enableCheckOrderBookChecksum) {
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, bid[0].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, bid[1].GetString()});
          } else {
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(bid[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(bid[1].GetString())});
          }
          marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
        }
        marketDataMessageList.push_back(std::move(marketDataMessage));
        if (this->sessionOptions.enableCheckOrderBookChecksum) {
          this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnection.id][symbolId] =
              intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoul(data["checksum"].GetString()))));
        }
      } else if (channel == CCAPI_WEBSOCKET_FTX_CHANNEL_TRADES) {
        for (const auto& x : data.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.tp = UtilTime::parse(std::string(x["time"].GetString()), "%FT%T%Ez");
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["size"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["id"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["side"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      }
    } else if (type == "partial") {
      const rj::Value& data = document["data"];
      auto symbolId = std::string(document["market"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_FTX_CHANNEL_ORDERBOOKS) + "|" + symbolId;
      if (this->sessionOptions.enableCheckOrderBookChecksum) {
        this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnection.id][symbolId] =
            intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoul(data["checksum"].GetString()))));
      }
      MarketDataMessage marketDataMessage;
      marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
      auto timePair = UtilTime::divide(std::string(data["time"].GetString()));
      auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
      tp += std::chrono::nanoseconds(timePair.second);
      marketDataMessage.tp = tp;
      const rj::Value& bids = data["bids"];
      for (auto& x : bids.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        if (this->sessionOptions.enableCheckOrderBookChecksum) {
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, x[0].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, x[1].GetString()});
        } else {
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
        }
        marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
      }
      const rj::Value& asks = data["asks"];
      for (auto& x : asks.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        if (this->sessionOptions.enableCheckOrderBookChecksum) {
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, x[0].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, x[1].GetString()});
        } else {
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
        }
        marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
      }
      marketDataMessageList.push_back(std::move(marketDataMessage));
    } else if (type == "subscribed") {
      // TODO(cryptochassis): implement
    } else if (type == "error") {
      // TODO(cryptochassis): implement
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return marketDataMessageList;
  }
  void convertReq(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                  const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        this->substituteParam(target, {
                                          {"{market_name}", Url::urlEncode(symbolId)},
                                      });
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit"},
                          });
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertReqCustom(req, request, now, symbolId, credential);
    }
  }
  void processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    const std::string& quotedTextMessage = this->convertNumberToStringInJson(textMessage);
    CCAPI_LOGGER_TRACE("quotedTextMessage = " + quotedTextMessage);
    MarketDataService::processSuccessfulTextMessage(request, quotedTextMessage, timeReceived);
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage,
                                                                       const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
          marketDataMessage.tp = UtilTime::parse(std::string(x["time"].GetString()), "%FT%T%Ez");
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["size"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["id"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["side"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
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
