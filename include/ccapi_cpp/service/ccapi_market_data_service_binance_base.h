#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_BINANCE_US) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES) || \
    defined(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceBinanceBase : public MarketDataService {
 public:
  MarketDataServiceBinanceBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                               ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->enableCheckPingPongWebsocketApplicationLevel = false;
  }

  virtual ~MarketDataServiceBinanceBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif

  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    Service::onOpen(wsConnectionPtr);
    this->startSubscribe(wsConnectionPtr);
  }

  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_BOOK_TICKER;
      } else {
        int marketDepthSubscribedToExchange = 1;
        marketDepthSubscribedToExchange = this->calculateMarketDepthAllowedByExchange(marketDepthRequested, std::vector<int>({5, 10, 20}));
        std::string updateSpeed;
        if (conflateIntervalMilliseconds < 1000) {
          updateSpeed = "100ms";
        }
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
        if (!updateSpeed.empty()) {
          channelId += "&UPDATE_SPEED=" + updateSpeed;
        }
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = marketDepthSubscribedToExchange;
      }
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval =
          this->convertCandlestickIntervalSecondsToInterval(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)), "s", "m", "h", "d", "w");
      channelId = channelId + "_" + interval;
    }
  }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("method", rj::Value("SUBSCRIBE").Move(), allocator);
    rj::Value params(rj::kArrayType);
    std::vector<std::string> exchangeSubscriptionIdList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        auto exchangeSubscriptionId = UtilString::toLower(subscriptionListByInstrument.first) + "@";
        if (channelId.rfind(CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_BOOK_TICKER, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
          exchangeSubscriptionId += CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_BOOK_TICKER;
        } else if (channelId.rfind(CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          exchangeSubscriptionId += std::string(CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH) + std::to_string(marketDepthSubscribedToExchange);
          auto splitted = UtilString::split(channelId, "?");
          if (splitted.size() == 2) {
            auto mapped = Url::convertQueryStringToMap(splitted.at(1));
            if (mapped.find("UPDATE_SPEED") != mapped.end()) {
              exchangeSubscriptionId += "@" + mapped.at("UPDATE_SPEED");
            }
          }
        } else {
          exchangeSubscriptionId += channelId;
        }
        params.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
      }
    }
    document.AddMember("params", params, allocator);
    document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id]).Move(), allocator);
    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnectionPtr->id]
                                                                          [this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id]] =
        exchangeSubscriptionIdList;
    this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id] += 1;
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    if (document.IsObject() && document.HasMember("result") && document["result"].IsNull()) {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
          this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        int id = std::stoi(document["id"].GetString());
        if (this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.find(wsConnectionPtr->id) !=
                this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.end() &&
            this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnectionPtr->id).find(id) !=
                this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnectionPtr->id).end()) {
          for (const auto& exchangeSubscriptionId : this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnectionPtr->id).at(id)) {
            std::string channelId =
                this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
            std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
            if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).find(channelId) !=
                this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).end()) {
              if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).find(symbolId) !=
                  this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).end()) {
                std::vector<std::string> correlationIdList_2 =
                    this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
                correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
              }
            }
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
    } else if (document.IsObject() && document.HasMember("stream") && document.HasMember("data")) {
      MarketDataMessage marketDataMessage;
      std::string exchangeSubscriptionId = document["stream"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
      const rj::Value& data = document["data"];
      if (channelId == CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_BOOK_TICKER) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        marketDataMessage.tp = this->isDerivatives ? TimePoint(std::chrono::milliseconds(std::stoll(data["T"].GetString()))) : timeReceived;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(data["b"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(data["B"].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(data["a"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(data["A"].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channelId.rfind(CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_PARTIAL_BOOK_DEPTH, 0) == 0) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        marketDataMessage.tp = this->isDerivatives ? TimePoint(std::chrono::milliseconds(std::stoll(data["T"].GetString()))) : timeReceived;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        const char* bidsName = this->isDerivatives ? "b" : "bids";
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : data[bidsName].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          ++bidIndex;
        }
        const char* asksName = this->isDerivatives ? "a" : "asks";
        int askIndex = 0;
        for (const auto& x : data[asksName].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          ++askIndex;
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channelId == CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_TRADE) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(data["T"].GetString()));
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(data["p"].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(data["q"].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, data["t"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["m"].GetBool() ? "1" : "0");
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channelId == CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_AGG_TRADE) {
        auto time = UtilTime::makeTimePointFromMilliseconds(std::stoll(data["T"].GetString()));
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_AGG_TRADE;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = time;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(data["p"].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(data["q"].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::AGG_TRADE_ID, data["a"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["m"].GetBool() ? "1" : "0");
        marketDataMessage.data[MarketDataMessage::DataType::AGG_TRADE].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channelId.find(CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_KLINE) != std::string::npos) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        const rj::Value& k = data["k"];
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(k["t"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, k["o"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, k["h"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, k["l"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, k["c"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, k["v"].GetString());
        dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, k["q"].GetString());
        marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    }
  }

  void prepareReq(http::request<http::string_body>& req, const std::map<std::string, std::string>& credential) {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    if (!apiKey.empty()) {
      req.set("X-MBX-APIKEY", apiKey);
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req, credential);
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
                              {CCAPI_START_TRADE_ID, "fromId"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_RECENT_AGG_TRADES:
      case Request::Operation::GET_HISTORICAL_AGG_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentAggTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit"},
                              {CCAPI_START_TIME_SECONDS, "startTime"},
                              {CCAPI_END_TIME_SECONDS, "endTime"},
                              {CCAPI_START_AGG_TRADE_ID, "fromId"},
                          },
                          {
                              {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBinanceBase>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                              {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBinanceBase>()](
                                                           const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        req.method(http::verb::get);
        auto target = this->getRecentCandlesticksTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS, "interval"},
                              {CCAPI_LIMIT, "limit"},
                              {CCAPI_START_TIME_SECONDS, "startTime"},
                              {CCAPI_END_TIME_SECONDS, "endTime"},
                          },
                          {
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS,
                               [that = shared_from_base<MarketDataServiceBinanceBase>()](const std::string& input) {
                                 return that->convertCandlestickIntervalSecondsToInterval(std::stoi(input), "s", "m", "h", "d", "w");
                               }},
                              {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBinanceBase>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                              {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBinanceBase>()](
                                                           const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_MARKET_DEPTH: {
        req.method(http::verb::get);
        auto target = this->getMarketDepthTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_MARKET_DEPTH_MAX, "limit"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_SERVER_TIME: {
        req.method(http::verb::get);
        auto target = this->getServerTimeTarget;
        req.target(target);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        std::string queryString;
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        if (!this->isDerivatives) {
          target += "?showPermissionSets=false";
        }
        req.target(target);
      } break;
      case Request::Operation::GET_BBOS: {
        req.method(http::verb::get);
        auto target = this->getBbosTarget;
        req.target(target);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_INSTRUMENT_STATUS, x["status"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseAsset"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteAsset"].GetString());
    for (const auto& y : x["filters"].GetArray()) {
      std::string filterType = y["filterType"].GetString();
      if (filterType == "PRICE_FILTER") {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, y["tickSize"].GetString());
      } else if (filterType == "LOT_SIZE") {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, y["stepSize"].GetString());
        element.insert(CCAPI_ORDER_QUANTITY_MIN, y["minQty"].GetString());
      } else if (filterType == "NOTIONAL") {
        element.insert(CCAPI_ORDER_QUOTE_QUANTITY_MIN, y["minNotional"].GetString());
        element.insert(CCAPI_ORDER_QUOTE_QUANTITY_MAX, y["maxNotional"].GetString());
      }
    }
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["time"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["qty"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["id"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, x["isBuyerMaker"].GetBool() ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_AGG_TRADES:
      case Request::Operation::GET_HISTORICAL_AGG_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_AGG_TRADE;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["T"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["p"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["q"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::AGG_TRADE_ID, x["a"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, x["m"].GetBool() ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::AGG_TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x[0].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, x[1].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, x[2].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, x[3].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, x[4].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, x[5].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, x[7].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_MARKET_DEPTH: {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(document["T"].GetString()));
        for (const auto& x : document["bids"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : document["asks"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } break;
      case Request::Operation::GET_SERVER_TIME: {
        Message message;
        message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(document["serverTime"].GetString())));
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["symbols"].GetArray()) {
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
        for (const auto& x : document["symbols"].GetArray()) {
          Element element;
          this->extractInstrumentInfo(element, x);
          elementList.push_back(element);
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_BBOS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_BEST_BID_N_PRICE, x["bidPrice"].GetString());
          element.insert(CCAPI_BEST_BID_N_SIZE, x["bidQty"].GetString());
          element.insert(CCAPI_BEST_ASK_N_PRICE, x["askPrice"].GetString());
          element.insert(CCAPI_BEST_ASK_N_SIZE, x["askQty"].GetString());
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
  std::string getRecentAggTradesTarget;
  std::string getHistoricalAggTradesTarget;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_BASE_H_
