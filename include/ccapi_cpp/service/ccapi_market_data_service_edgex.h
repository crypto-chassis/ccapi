#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_EDGEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_EDGEX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_EDGEX

#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {
class MarketDataServiceEdgex : public MarketDataService {
 public:
  MarketDataServiceEdgex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                         ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_EDGEX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + CCAPI_EDGEX_PUBLIC_WS_PATH;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->getInstrumentsTarget = "/api/v1/public/meta/getMetaData";
  }
  virtual ~MarketDataServiceEdgex() {}

#ifndef CCAPI_EXPOSE_INTERNAL
 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    if (field == CCAPI_MARKET_DEPTH) {
      channelId = CCAPI_WEBSOCKET_EDGEX_CHANNEL_DEPTH;
    } else if (field == CCAPI_TRADE) {
      channelId = CCAPI_WEBSOCKET_EDGEX_CHANNEL_TRADES;
    } else if (field == CCAPI_CANDLESTICK) {
      channelId = CCAPI_WEBSOCKET_EDGEX_CHANNEL_KLINE;
    }
  }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    auto& wsConnection = *wsConnectionPtr;
    for (const auto& channelEntry : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      const auto& channelId = channelEntry.first;
      for (const auto& symbolEntry : channelEntry.second) {
        const auto& symbolId = symbolEntry.first;
        const auto& subscription = symbolEntry.second.at(0);
        const auto& optionMap = subscription.getOptionMap();
        std::string exchangeChannelId;
        if (channelId == CCAPI_WEBSOCKET_EDGEX_CHANNEL_DEPTH) {
          int requestedDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
          int depthForExchange = requestedDepth <= 15 ? 15 : 200;
          exchangeChannelId = std::string("depth.") + symbolId + "." + std::to_string(depthForExchange);
          this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = depthForExchange;
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = false;
        } else if (channelId == CCAPI_WEBSOCKET_EDGEX_CHANNEL_TRADES) {
          exchangeChannelId = std::string("trades.") + symbolId;
        } else if (channelId == CCAPI_WEBSOCKET_EDGEX_CHANNEL_KLINE) {
          int intervalSeconds = std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS));
          std::string interval = this->convertIntervalSecondsToEdgeX(intervalSeconds);
          auto it = optionMap.find(CCAPI_EDGEX_PRICE_TYPE);
          std::string priceType = it != optionMap.end() ? it->second : "LAST_PRICE";
          exchangeChannelId = std::string("kline.") + priceType + "." + symbolId + "." + interval;
        } else {
          continue;
        }
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("type", rj::Value("subscribe").Move(), allocator);
        document.AddMember("channel", rj::Value(exchangeChannelId.c_str(), allocator).Move(), allocator);
        rj::StringBuffer buf;
        rj::Writer<rj::StringBuffer> writer(buf);
        document.Accept(writer);
        sendStringList.emplace_back(buf.GetString());
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeChannelId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeChannelId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    return sendStringList;
  }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("type", rj::Value("ping").Move(), allocator);
    document.AddMember("time", rj::Value(std::to_string(nowMs).c_str(), allocator).Move(), allocator);
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    document.Accept(writer);
    this->send(wsConnectionPtr, buffer.GetString(), ec);
  }

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    if (!document.IsObject() || !document.HasMember("type")) {
      return;
    }
    std::string type = document["type"].GetString();
    if (type == "ping" && document.HasMember("time")) {
      ErrorCode ec;
      rj::Document response;
      response.SetObject();
      rj::Document::AllocatorType& allocator = response.GetAllocator();
      response.AddMember("type", rj::Value("pong").Move(), allocator);
      std::string timeValue;
      if (document["time"].IsString()) {
        timeValue = document["time"].GetString();
      } else if (document["time"].IsNumber()) {
        timeValue = std::to_string(document["time"].GetInt64());
      } else {
        auto now = UtilTime::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        timeValue = std::to_string(nowMs);
      }
      response.AddMember("time", rj::Value(timeValue.c_str(), allocator).Move(), allocator);
      rj::StringBuffer buffer;
      rj::Writer<rj::StringBuffer> writer(buffer);
      response.Accept(writer);
      this->send(wsConnectionPtr, buffer.GetString(), ec);
      return;
    }
    if (!document.HasMember("channel")) {
      if (type == "error") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
        message.setElementList({element});
        event.addMessages({message});
      }
      return;
    }
    std::string exchangeSubscriptionId = document["channel"].GetString();
    if (type == "subscribed") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      if (this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.count(wsConnectionPtr->id) &&
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).count(exchangeSubscriptionId)) {
        const auto& channelId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        const auto& symbolId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
        if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.count(wsConnectionPtr->id) &&
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).count(channelId) &&
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).count(symbolId)) {
          message.setCorrelationIdList(
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId));
        }
      }
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessageView);
      message.setElementList({element});
      event.addMessages({message});
      return;
    }
    if (type == "error") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
      message.setElementList({element});
      event.addMessages({message});
      return;
    }
    if (type != "payload" && type != "quote-event") {
      return;
    }
    if (!this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.count(wsConnectionPtr->id) ||
        !this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).count(exchangeSubscriptionId)) {
      return;
    }
    const auto& channelId =
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
    bool hasContent = document.HasMember("content") && document["content"].IsObject();
    if (!hasContent) {
      return;
    }
    const rj::Value& content = document["content"];
    std::string dataType = content.HasMember("dataType") && content["dataType"].IsString() ? content["dataType"].GetString() : "";
    std::string dataTypeLower = UtilString::toLower(dataType);
    MarketDataMessage::RecapType recapDefault =
        dataTypeLower == "snapshot" ? MarketDataMessage::RecapType::SOLICITED : MarketDataMessage::RecapType::NONE;
    if (channelId == CCAPI_WEBSOCKET_EDGEX_CHANNEL_DEPTH) {
      if (!content.HasMember("data") || !content["data"].IsArray()) {
        return;
      }
      for (const auto& entry : content["data"].GetArray()) {
        std::string depthTypeLower;
        if (entry.HasMember("depthType") && entry["depthType"].IsString()) {
          depthTypeLower = UtilString::toLower(entry["depthType"].GetString());
        }
        MarketDataMessage::RecapType recap =
            depthTypeLower == "snapshot" ? MarketDataMessage::RecapType::SOLICITED : recapDefault;
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.recapType = recap;
        marketDataMessage.tp = timeReceived;
        auto parsePriceSize = [](const rj::Value& node, const char*& priceOut, const char*& sizeOut) -> bool {
          if (node.IsArray()) {
            if (node.Size() >= 2 && node[0].IsString() && node[1].IsString()) {
              priceOut = node[0].GetString();
              sizeOut = node[1].GetString();
              return true;
            }
          } else if (node.IsObject()) {
            auto priceIt = node.FindMember("price");
            auto sizeIt = node.FindMember("size");
            if (priceIt != node.MemberEnd() && sizeIt != node.MemberEnd() && priceIt->value.IsString() && sizeIt->value.IsString()) {
              priceOut = priceIt->value.GetString();
              sizeOut = sizeIt->value.GetString();
              return true;
            }
          }
          return false;
        };
        if (entry.HasMember("bids") && entry["bids"].IsArray()) {
          auto& bids = marketDataMessage.data[MarketDataMessage::DataType::BID];
          for (const auto& level : entry["bids"].GetArray()) {
            const char* price = nullptr;
            const char* size = nullptr;
            if (parsePriceSize(level, price, size)) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(price));
              dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(size));
              bids.emplace_back(std::move(dataPoint));
            }
          }
        }
        if (entry.HasMember("asks") && entry["asks"].IsArray()) {
          auto& asks = marketDataMessage.data[MarketDataMessage::DataType::ASK];
          for (const auto& level : entry["asks"].GetArray()) {
            const char* price = nullptr;
            const char* size = nullptr;
            if (parsePriceSize(level, price, size)) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(price));
              dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(size));
              asks.emplace_back(std::move(dataPoint));
            }
          }
        }
        if (!marketDataMessage.data.empty()) {
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      }
    } else if (channelId == CCAPI_WEBSOCKET_EDGEX_CHANNEL_TRADES) {
      if (!content.HasMember("data") || !content["data"].IsArray()) {
        return;
      }
      for (const auto& trade : content["data"].GetArray()) {
        if (!trade.IsObject()) {
          continue;
        }
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        if (trade.HasMember("time") && trade["time"].IsString()) {
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(trade["time"].GetString())));
        } else {
          marketDataMessage.tp = timeReceived;
        }
        MarketDataMessage::TypeForDataPoint dataPoint;
        if (trade.HasMember("price") && trade["price"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(trade["price"].GetString()));
        }
        if (trade.HasMember("size") && trade["size"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(trade["size"].GetString()));
        }
        if (trade.HasMember("ticketId") && trade["ticketId"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, trade["ticketId"].GetString());
        }
        if (trade.HasMember("isBuyerMaker")) {
          bool isBuyerMaker = trade["isBuyerMaker"].IsBool() ? trade["isBuyerMaker"].GetBool() : false;
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, isBuyerMaker ? "1" : "0");
        }
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    } else if (channelId == CCAPI_WEBSOCKET_EDGEX_CHANNEL_KLINE) {
      if (!content.HasMember("data") || !content["data"].IsArray()) {
        return;
      }
      for (const auto& kline : content["data"].GetArray()) {
        if (!kline.IsObject()) {
          continue;
        }
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.recapType = recapDefault;
        if (kline.HasMember("klineTime") && kline["klineTime"].IsString()) {
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(kline["klineTime"].GetString())));
        } else {
          marketDataMessage.tp = timeReceived;
        }
        MarketDataMessage::TypeForDataPoint dataPoint;
        if (kline.HasMember("open") && kline["open"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, UtilString::normalizeDecimalStringView(kline["open"].GetString()));
        }
        if (kline.HasMember("high") && kline["high"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, UtilString::normalizeDecimalStringView(kline["high"].GetString()));
        }
        if (kline.HasMember("low") && kline["low"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, UtilString::normalizeDecimalStringView(kline["low"].GetString()));
        }
        if (kline.HasMember("close") && kline["close"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, UtilString::normalizeDecimalStringView(kline["close"].GetString()));
        }
        if (kline.HasMember("size") && kline["size"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, UtilString::normalizeDecimalStringView(kline["size"].GetString()));
        }
        if (kline.HasMember("value") && kline["value"].IsString()) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, UtilString::normalizeDecimalStringView(kline["value"].GetString()));
        }
        marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    }
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        this->extractInstrumentInfoFromResponse(elementList, document);
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PUBLIC_REQUEST: {
        MarketDataService::convertRequestForRestGenericPublicRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        req.target(this->getInstrumentsTarget);
        req.set(beast::http::field::content_type, "application/json");
        req.prepare_payload();
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractInstrumentInfoFromResponse(std::vector<Element>& elementList, const rj::Value& response) {
    if (!response.IsObject() || !response.HasMember("data") || !response["data"].IsObject()) {
      return;
    }
    const auto& data = response["data"];
    std::map<std::string, std::string> coinNameById;
    if (data.HasMember("coinList") && data["coinList"].IsArray()) {
      for (const auto& coin : data["coinList"].GetArray()) {
        if (coin.HasMember("coinId") && coin.HasMember("coinName") && coin["coinId"].IsString() && coin["coinName"].IsString()) {
          coinNameById[coin["coinId"].GetString()] = coin["coinName"].GetString();
        }
      }
    }
    if (!data.HasMember("contractList") || !data["contractList"].IsArray()) {
      return;
    }
    for (const auto& contract : data["contractList"].GetArray()) {
      if (!contract.IsObject()) {
        continue;
      }
      Element element;
      if (contract.HasMember("contractName") && contract["contractName"].IsString()) {
        element.insert(CCAPI_INSTRUMENT, contract["contractName"].GetString());
      }
      if (contract.HasMember("baseCoinId") && contract["baseCoinId"].IsString()) {
        auto it = coinNameById.find(contract["baseCoinId"].GetString());
        if (it != coinNameById.end()) {
          element.insert(CCAPI_BASE_ASSET, it->second);
        }
      }
      if (contract.HasMember("quoteCoinId") && contract["quoteCoinId"].IsString()) {
        auto it = coinNameById.find(contract["quoteCoinId"].GetString());
        if (it != coinNameById.end()) {
          element.insert(CCAPI_QUOTE_ASSET, it->second);
        }
      }
      if (contract.HasMember("tickSize") && contract["tickSize"].IsString()) {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, contract["tickSize"].GetString());
      }
      if (contract.HasMember("stepSize") && contract["stepSize"].IsString()) {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, contract["stepSize"].GetString());
      }
      if (contract.HasMember("minOrderSize") && contract["minOrderSize"].IsString()) {
        element.insert(CCAPI_ORDER_QUANTITY_MIN, contract["minOrderSize"].GetString());
      }
      if (contract.HasMember("maxOrderSize") && contract["maxOrderSize"].IsString()) {
        element.insert(CCAPI_ORDER_QUANTITY_MAX, contract["maxOrderSize"].GetString());
      }
      element.insert(CCAPI_CONTRACT_SIZE, "1");
      elementList.emplace_back(std::move(element));
    }
  }

  std::string convertIntervalSecondsToEdgeX(int intervalSeconds) {
    static const std::map<int, std::string> intervalMap = {
        {60, "MINUTE_1"},    {300, "MINUTE_5"},   {900, "MINUTE_15"},  {1800, "MINUTE_30"}, {3600, "HOUR_1"},
        {7200, "HOUR_2"},   {14400, "HOUR_4"},   {21600, "HOUR_6"},   {28800, "HOUR_8"},   {43200, "HOUR_12"},
        {86400, "DAY_1"},   {604800, "WEEK_1"},  {2592000, "MONTH_1"}};
    auto it = intervalMap.find(intervalSeconds);
    if (it != intervalMap.end()) {
      return it->second;
    }
    return "MINUTE_1";
  }
};
}  // namespace ccapi

#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_EDGEX_H_

