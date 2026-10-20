#pragma once

#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITGET_UTA
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceBitgetUta : public MarketDataService {
 public:
  MarketDataServiceBitgetUta(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITGET_UTA;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v3/ws/public";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->hostHttpHeaderValueIgnorePort = true;
    this->getServerTimeTarget = "/api/v3/market/time";
    this->getRecentTradesTarget = "/api/v3/market/fills";
    this->getHistoricalTradesTarget = "/api/v3/market/fills";
    this->getInstrumentTarget = "/api/v3/market/instruments";
    this->getInstrumentsTarget = "/api/v3/market/instruments";
    this->getRecentCandlesticksTarget = "/api/v3/market/candles";
    this->getHistoricalCandlesticksTarget = "/api/v3/market/history-candles";
    this->getBbosTarget = "/api/v3/market/tickers";
  }

  virtual ~MarketDataServiceBitgetUta() {}

#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override {
    return !std::regex_search(bodyView.begin(), bodyView.end(), std::regex("\"code\":\\s*\"00000\""));
  }

  // books1 and books5 push a full snapshot every time (replace semantics).
  // books and books50 push a snapshot first and then incremental updates, so they are driven by the `action` field.
  static bool isSnapshotDepthTopic(const std::string& channelId) {
    return channelId == CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS1 || channelId == CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS5;
  }

  static bool isDepthTopic(const std::string& channelId) {
    return channelId == CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS || channelId == CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS50 || isSnapshotDepthTopic(channelId);
  }

  // For kline the channelId is "kline" + interval (e.g. "kline1m") so that subscriptions with different intervals are kept apart.
  static bool isKlineChannel(const std::string& channelId) { return channelId.rfind(CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_KLINE, 0) == 0; }

  static std::string channelIdFromArg(const rj::Value& arg) {
    std::string topic = arg["topic"].GetString();
    auto it = arg.FindMember("interval");
    if (it != arg.MemberEnd() && it->value.IsString()) {
      topic += it->value.GetString();
    }
    return topic;
  }

  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    this->instrumentType = UtilString::toLower(subscription.getInstrumentType().empty() ? "SPOT" : subscription.getInstrumentType());
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested <= 1) {
        channelId = CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS1;
      } else if (marketDepthRequested <= 5) {
        channelId = CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS5;
      } else if (marketDepthRequested <= 50) {
        channelId = CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS50;
      } else {
        channelId = CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_BOOKS;
      }
    } else if (field == CCAPI_TRADE) {
      channelId = CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_TRADES;
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval =
          this->convertCandlestickIntervalSecondsToInterval(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)), " ", "m", "H", "D", "W");
      channelId = std::string(CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_KLINE) + interval;
    }
  }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        if (isSnapshotDepthTopic(channelId)) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
        std::string topic = channelId;
        std::string interval;
        if (isKlineChannel(channelId)) {
          topic = CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_KLINE;
          interval = channelId.substr(std::string(CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_KLINE).size());
        }
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;
        rj::Value arg(rj::kObjectType);
        arg.AddMember("instType", rj::Value(this->instrumentType.c_str(), allocator).Move(), allocator);
        arg.AddMember("topic", rj::Value(topic.c_str(), allocator).Move(), allocator);
        arg.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        if (!interval.empty()) {
          arg.AddMember("interval", rj::Value(interval.c_str(), allocator).Move(), allocator);
        }
        args.PushBack(arg, allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    sendStringList.push_back(stringBuffer.GetString());
    return sendStringList;
  }

  // NOTE: Bitget removed the depth checksum on 2026-05-19 (use seq/pseq instead), so there is no calculateOrderBookChecksum override.

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    if (textMessageView == "pong") return;
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    auto it = document.FindMember("event");
    std::string eventStr = (it != document.MemberEnd() && it->value.IsString()) ? it->value.GetString() : "";
    if (!eventStr.empty()) {
      if (eventStr == "subscribe") {
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        std::vector<Message> messageList;
        Message message;
        message.setTimeReceived(timeReceived);
        std::vector<std::string> correlationIdList;
        auto itConn = this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id);
        if (itConn != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end() && document.HasMember("arg")) {
          const rj::Value& arg = document["arg"];
          std::string channelId = channelIdFromArg(arg);
          std::string symbolId = arg["symbol"].GetString();
          auto itChannel = itConn->second.find(channelId);
          if (itChannel != itConn->second.end()) {
            auto itSymbol = itChannel->second.find(symbolId);
            if (itSymbol != itChannel->second.end()) {
              correlationIdList.insert(correlationIdList.end(), itSymbol->second.begin(), itSymbol->second.end());
            }
          }
        }
        message.setCorrelationIdList(correlationIdList);
        message.setType(Message::Type::SUBSCRIPTION_STARTED);
        Element element;
        element.insert(CCAPI_INFO_MESSAGE, textMessageView);
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
        element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
        event.setMessageList(messageList);
      }
      return;
    }
    if (!document.HasMember("arg") || !document.HasMember("data")) return;

    const rj::Value& arg = document["arg"];
    std::string channelId = channelIdFromArg(arg);
    std::string symbolId = arg["symbol"].GetString();
    std::string exchangeSubscriptionId = channelId + ":" + symbolId;
    std::string action = document.HasMember("action") ? document["action"].GetString() : "update";

    if (isDepthTopic(channelId)) {
      for (const auto& datum : document["data"].GetArray()) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        if (isSnapshotDepthTopic(channelId)) {
          if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          } else {
            marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
          }
        } else {
          marketDataMessage.recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
        }
        // v3 order book: "b" = bids, "a" = asks, each entry [price, size]
        for (const auto& x : datum["b"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : datum["a"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    } else if (channelId == CCAPI_WEBSOCKET_BITGET_UTA_CHANNEL_TRADES) {
      MarketDataMessage::RecapType recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
      for (const auto& datum : document["data"].GetArray()) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
        marketDataMessage.recapType = recapType;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["T"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(datum["p"].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(datum["v"].GetString()));
        // Side is the taker side; buyer is the maker when the taker sells.
        dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(datum["S"].GetString()) == "Sell" ? "1" : "0");
        dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, datum["i"].GetString());
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    } else if (isKlineChannel(channelId)) {
      for (const auto& datum : document["data"].GetArray()) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["start"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;

        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, datum["open"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, datum["high"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, datum["low"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, datum["close"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, datum["volume"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, datum["turnover"].GetString());

        marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    }
  }

  // v3 market endpoints require `category` (SPOT, MARGIN, USDT-FUTURES, COIN-FUTURES, USDC-FUTURES); default to SPOT.
  void appendCategory(std::string& queryString, const std::map<std::string, std::string>& param) {
    auto it = param.find(CCAPI_INSTRUMENT_TYPE);
    std::string category = (it != param.end() && !it->second.empty()) ? it->second : "SPOT";
    if (!queryString.empty()) queryString += "&";
    queryString += "category=" + category;
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PUBLIC_REQUEST: {
        MarketDataService::convertRequestForRestGenericPublicRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::GET_SERVER_TIME: {
        req.method(http::verb::get);
        req.target(this->getServerTimeTarget);
      } break;
      case Request::Operation::GET_BBOS: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendCategory(queryString, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId, "symbol");
        }
        req.target(this->getBbosTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        // v3 /market/fills has no time filter; only the most recent fills (max 100) are available
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendCategory(queryString, param);
        queryString += "&";
        this->appendParam(queryString, param, {{CCAPI_LIMIT, "limit"}});
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(this->getRecentTradesTarget + "?" + queryString);
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        req.method(http::verb::get);
        auto target =
            request.getOperation() == Request::Operation::GET_RECENT_CANDLESTICKS ? this->getRecentCandlesticksTarget : this->getHistoricalCandlesticksTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendCategory(queryString, param);
        queryString += "&";
        this->appendParam(queryString, param,
                          {
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS, "interval"},
                              {CCAPI_LIMIT, "limit"},
                              {CCAPI_START_TIME_SECONDS, "startTime"},
                              {CCAPI_END_TIME_SECONDS, "endTime"},
                          },
                          {
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS,
                               [that = shared_from_base<MarketDataServiceBitgetUta>()](const std::string& input) {
                                 return that->convertCandlestickIntervalSecondsToInterval(std::stoi(input), " ", "m", "H", "D", "W");
                               }},
                              {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBitgetUta>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                              {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBitgetUta>()](
                                                           const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS:
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendCategory(queryString, param);
        if (request.getOperation() == Request::Operation::GET_INSTRUMENT) {
          this->appendSymbolId(queryString, symbolId, "symbol");
        }
        req.target(this->getInstrumentsTarget + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  static std::string precisionToIncrement(int precision) { return precision > 0 ? "0." + std::string(precision - 1, '0') + "1" : "1"; }

  static bool hasNonEmptyString(const rj::Value& x, const char* key) {
    auto it = x.FindMember(key);
    return it != x.MemberEnd() && it->value.IsString() && it->value.GetStringLength() > 0;
  }

  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseCoin"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteCoin"].GetString());
    // priceMultiplier / quantityMultiplier are futures-only and, when present, are the actual order step
    if (hasNonEmptyString(x, "priceMultiplier")) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["priceMultiplier"].GetString());
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, precisionToIncrement(std::stoi(x["pricePrecision"].GetString())));
    }
    if (hasNonEmptyString(x, "quantityMultiplier")) {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["quantityMultiplier"].GetString());
    } else {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, precisionToIncrement(std::stoi(x["quantityPrecision"].GetString())));
    }
    if (hasNonEmptyString(x, "minOrderQty")) {
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minOrderQty"].GetString());
    }
  }

  static void insertNullable(Element& element, const char* name, const rj::Value& x, const char* key) {
    auto it = x.FindMember(key);
    if (it == x.MemberEnd() || it->value.IsNull() || !it->value.IsString() || it->value.GetStringLength() == 0) {
      element.insert(name, "null");
    } else {
      element.insert(name, it->value.GetString());
    }
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_SERVER_TIME: {
        Message message;
        const rj::Value& data = document["data"];
        message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(data["serverTime"].GetString())));
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_BBOS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          insertNullable(element, CCAPI_BEST_BID_N_PRICE, x, "bid1Price");
          insertNullable(element, CCAPI_BEST_ASK_N_PRICE, x, "ask1Price");
          insertNullable(element, CCAPI_BEST_BID_N_SIZE, x, "bid1Size");
          insertNullable(element, CCAPI_BEST_ASK_N_SIZE, x, "ask1Size");
          elementList.push_back(element);
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        for (const auto& datum : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(datum["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(datum["size"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, datum["execId"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(datum["side"].GetString()) == "sell" ? "1" : "0");
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
          dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, datum[1].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, datum[2].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, datum[3].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, datum[4].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, datum[5].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, datum[6].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["data"].GetArray()) {
          if (std::string_view(x["symbol"].GetString()) == request.getInstrument()) {
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

  std::string instrumentType{"spot"};
};

} /* namespace ccapi */
#endif
#endif
