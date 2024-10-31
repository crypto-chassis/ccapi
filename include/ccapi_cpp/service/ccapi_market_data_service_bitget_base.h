#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_BITGET) || defined(CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceBitgetBase : public MarketDataService {
 public:
  MarketDataServiceBitgetBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                              ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->hostHttpHeaderValueIgnorePort = true;
  }
  virtual ~MarketDataServiceBitgetBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"00000\"")); }
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (this->isDerivatives) this->instrumentType = subscription.getInstrumentType();
    if (field == CCAPI_MARKET_DEPTH) {
      if (conflateIntervalMilliseconds < 200 && this->isDerivatives) {
        channelId = CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS;
      } else {
        if (marketDepthRequested <= 1) {
          channelId = CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS1;
        } else if (marketDepthRequested <= 5) {
          channelId = CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS5;
        } else if (marketDepthRequested <= 15) {
          channelId = CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS15;
        } else {
          channelId = CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS;
        }
      }
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval =
          this->convertCandlestickIntervalSecondsToInterval(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)), " ", "m", "H", "D", "W");
      channelId = channelId + interval;
    }
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, "ping", wspp::frame::opcode::text, ec); }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }
#endif
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        if (channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS1 || channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS5 ||
            channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS15) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = UtilString::split(channelId, "?").at(0) + ":" + symbolId;
        rj::Value arg(rj::kObjectType);
        std::string instType = this->instrumentType;
        arg.AddMember("instType", rj::Value(instType.c_str(), allocator).Move(), allocator);
        arg.AddMember("channel", rj::Value(channelId.c_str(), allocator).Move(), allocator);
        arg.AddMember("instId", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        args.PushBack(arg, allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
  std::string calculateOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk) override {
    auto i = 0;
    auto i1 = snapshotBid.rbegin();
    auto i2 = snapshotAsk.begin();
    std::vector<std::string> csData;
    while (i < 25 && (i1 != snapshotBid.rend() || i2 != snapshotAsk.end())) {
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
    uint_fast32_t csCalc = UtilAlgorithm::crc(csStr.begin(), csStr.end());
    return intToHex(csCalc);
  }
  void processTextMessage(
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage
#else
      std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView
#endif
      ,
      const TimePoint& timeReceived, Event& event, std::vector<MarketDataMessage>& marketDataMessageList) override {
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
#else
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    if (textMessage != "pong") {
      rj::Document document;
      document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
      auto it = document.FindMember("event");
      std::string eventStr = it != document.MemberEnd() ? it->value.GetString() : "";
      // if (eventStr == "login") {
      //   this->startSubscribe(wsConnection);
      // } else {
      if (!eventStr.empty()) {
        if (eventStr == "subscribe") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          std::vector<std::string> correlationIdList;
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
            const rj::Value& arg = document["arg"];
            std::string channelId = arg["channel"].GetString();
            std::string symbolId = arg["instId"].GetString();
            if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).find(channelId) !=
                this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).end()) {
              if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).find(symbolId) !=
                  this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).end()) {
                std::vector<std::string> correlationIdList_2 =
                    this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
                correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
              }
            }
          }
          message.setCorrelationIdList(correlationIdList);
          message.setType(Message::Type::SUBSCRIPTION_STARTED);
          Element element;
          element.insert(CCAPI_INFO_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          event.setMessageList(messageList);
        } else if (eventStr == "error") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          event.setMessageList(messageList);
        }
      } else if (document.HasMember("arg")) {
        const rj::Value& arg = document["arg"];
        std::string channelId = arg["channel"].GetString();
        std::string symbolId = arg["instId"].GetString();
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;
        if (channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS || channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS1 ||
            channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS5 || channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS15) {
          std::string action = document["action"].GetString();
          for (const auto& datum : document["data"].GetArray()) {
            if (this->sessionOptions.enableCheckOrderBookChecksum) {
              auto it = datum.FindMember("checksum");
              if (it != datum.MemberEnd()) {
                this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnection.id][symbolId] =
                    intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoi(it->value.GetString()))));
              }
            }
            MarketDataMessage marketDataMessage;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            if (channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS1 || channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS5 ||
                channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_BOOKS15) {
              if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
                marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
              } else {
                marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
              }
            } else {
              marketDataMessage.recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
            }
            for (const auto& x : datum["bids"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              if (this->sessionOptions.enableCheckOrderBookChecksum) {
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, x[0].GetString()});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, x[1].GetString()});
              } else {
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              }
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            }
            for (const auto& x : datum["asks"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              if (this->sessionOptions.enableCheckOrderBookChecksum) {
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, x[0].GetString()});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, x[1].GetString()});
              } else {
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              }
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (channelId == CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_TRADES) {
          std::string action = document["action"].GetString();
          MarketDataMessage::RecapType recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
          for (const auto& datum : document["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            marketDataMessage.recapType = recapType;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(datum["price"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(datum["size"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(datum["side"].GetString()) == "Buy" ? "1" : "0"});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(datum["tradeId"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (channelId.find(CCAPI_WEBSOCKET_BITGET_BASE_CHANNEL_KLINE) != std::string::npos) {
          for (const auto& datum : document["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum[0].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, datum[1].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, datum[2].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, datum[3].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, datum[4].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, datum[5].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::QUOTE_VOLUME, datum[6].GetString()});
            marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        }
      }
      // }
    }
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PUBLIC_REQUEST: {
        MarketDataService::convertRequestForRestGenericPublicRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit"},
                              {CCAPI_INSTRUMENT_TYPE, "productType"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_HISTORICAL_TRADES: {
        req.method(http::verb::get);
        auto target = this->getHistoricalTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit"},
                              {CCAPI_INSTRUMENT_TYPE, "productType"},
                              {CCAPI_START_TIME_SECONDS, "startTime"},
                              {CCAPI_END_TIME_SECONDS, "endTime"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        req.method(http::verb::get);
        auto target = this->getRecentCandlesticksTarget;
        std::string minuteSuffix = this->isDerivatives ? "m" : "min";
        std::string hourSuffix = this->isDerivatives ? "H" : "h";
        std::string daySuffix = this->isDerivatives ? "D" : "day";
        std::string weekSuffix = this->isDerivatives ? "W" : "week";
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(
            queryString, param,
            {
                {CCAPI_CANDLESTICK_INTERVAL_SECONDS, "granularity"},
                {CCAPI_LIMIT, "limit"},
                {CCAPI_START_TIME_SECONDS, "startTime"},
                {CCAPI_END_TIME_SECONDS, "endTime"},
                {CCAPI_INSTRUMENT_TYPE, "productType"},
            },
            {
                {CCAPI_CANDLESTICK_INTERVAL_SECONDS,
                 [that = shared_from_base<MarketDataServiceBitgetBase>(), minuteSuffix, hourSuffix, daySuffix, weekSuffix](const std::string& input) {
                   return that->convertCandlestickIntervalSecondsToInterval(std::stoi(input), " ", minuteSuffix, hourSuffix, daySuffix, weekSuffix);
                 }},
                {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBitgetBase>()](
                                               const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBitgetBase>()](
                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
            });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "productType"},
                          });
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        std::string target;
        std::string queryString;
        target = this->getInstrumentsTarget;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "productType"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseCoin"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteCoin"].GetString());
    if (this->isDerivatives) {
      int pricePlace = std::stoi(x["pricePlace"].GetString());
      if (pricePlace > 0) {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(pricePlace - 1, '0') + x["priceEndStep"].GetString());
      } else {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["priceEndStep"].GetString());
      }
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["sizeMultiplier"].GetString());
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minTradeNum"].GetString());
    } else {
      int priceScale = std::stoi(x["pricePrecision"].GetString());
      if (priceScale > 0) {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(priceScale - 1, '0') + "1");
      } else {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1");
      }
      int quantityScale = std::stoi(x["quantityPrecision"].GetString());
      if (quantityScale > 0) {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(quantityScale - 1, '0') + "1");
      } else {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1");
      }
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minTradeAmount"].GetString());
    }
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        for (const auto& datum : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(datum["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(datum["size"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, datum["tradeId"].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(datum["side"].GetString()) == "Buy" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        for (const auto& datum : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(datum[0].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, datum[1].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, datum[2].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, datum[3].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, datum[4].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, datum[5].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::QUOTE_VOLUME, datum[6].GetString()});
          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["data"].GetArray()) {
          if (std::string(x["symbol"].GetString()) == request.getInstrument()) {
            Element element;
            this->extractInstrumentInfo(element, x);
            message.setElementList({element});
            break;
          }
        }
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          this->extractInstrumentInfo(element, x);
          elementList.push_back(element);
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  bool isDerivatives{};
  std::string instrumentType{"SPOT"};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITGET_BASE_H_
