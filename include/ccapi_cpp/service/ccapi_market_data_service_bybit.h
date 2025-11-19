#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceBybit : public MarketDataService {
 public:
  MarketDataServiceBybit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                         ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BYBIT;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v5/public/{instrumentTypeSubstitute}";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->getRecentTradesTarget = "/v5/market/recent-trade";
    this->getHistoricalTradesTarget = "/v5/market/recent-trade";
    this->getRecentCandlesticksTarget = "/v5/market/kline";
    this->getHistoricalCandlesticksTarget = "/v5/market/kline";
    this->getMarketDepthTarget = "/v5/market/orderbook";
    this->getServerTimeTarget = "/v5/market/time";
    this->getInstrumentTarget = "/v5/market/instruments-info";
    this->getInstrumentsTarget = "/v5/market/instruments-info";
    this->getBbosTarget = "/v5/market/tickers";
  }

  virtual ~MarketDataServiceBybit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, R"({"op":"ping"})", ec); }

  std::string getInstrumentGroup(const Subscription& subscription) override {
    const auto& instrumentTypeSubstitute = subscription.getInstrumentType();
    std::string url = MarketDataService::getInstrumentGroup(subscription);
    std::string toReplace("{instrumentTypeSubstitute}");
    url.replace(url.find(toReplace), toReplace.length(), instrumentTypeSubstitute);
    return url;
  }

  std::string convertCandlestickIntervalSecondsToInterval(int intervalSeconds) {
    std::string interval;
    if (intervalSeconds < 86400) {
      interval = std::to_string(intervalSeconds / 60);
    } else if (intervalSeconds == 86400) {
      interval = "D";
    } else {
      interval = "W";
    }
    return interval;
  }

  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    const auto& marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    const auto& instrumentType = subscription.getInstrumentType();
    if (field == CCAPI_MARKET_DEPTH) {
      std::vector<int> depths;
      if (instrumentType == "spot") {
        depths = {1, 50, 200};
      } else if (instrumentType == "linear" || instrumentType == "inverse") {
        depths = {1, 50, 200, 500};
      } else if (instrumentType == "option") {
        depths = {25, 100};
      }
      int marketDepthSubscribedToExchange = 1;
      marketDepthSubscribedToExchange = this->calculateMarketDepthAllowedByExchange(marketDepthRequested, depths);
      channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
      this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = marketDepthSubscribedToExchange;
    } else if (field == CCAPI_CANDLESTICK) {
      int intervalSeconds = std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS));
      std::string interval = this->convertCandlestickIntervalSecondsToInterval(intervalSeconds);
      std::string toReplace = "{interval}";
      channelId.replace(channelId.find(toReplace), toReplace.length(), interval);
    }
  }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    std::vector<std::string> exchangeSubscriptionIdList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        std::string exchangeSubscriptionId = channelId;
        if (channelId.rfind(CCAPI_WEBSOCKET_BYBIT_CHANNEL_ORDERBOOK, 0) == 0) {
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          exchangeSubscriptionId = CCAPI_WEBSOCKET_BYBIT_CHANNEL_ORDERBOOK;
          std::string toReplace = "{depth}";
          exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), std::to_string(marketDepthSubscribedToExchange));
        }
        std::string toReplace = "{symbol}";
        exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), symbolId);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        args.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
      }
    }
    document.AddMember("args", args, allocator);
    document.AddMember("req_id", rj::Value(std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id]).c_str(), allocator).Move(),
                       allocator);
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
    if (document.IsObject() && document.HasMember("op")) {
      std::string op = document["op"].GetString();
      if (op == "subscribe") {
        bool success = document["success"].GetBool();
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        std::vector<Message> messageList;
        Message message;
        message.setTimeReceived(timeReceived);
        std::vector<std::string> correlationIdList;
        if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
          int id = std::stoi(document["req_id"].GetString());
          if (this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.find(wsConnectionPtr->id) !=
                  this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.end() &&
              this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnectionPtr->id).find(id) !=
                  this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnectionPtr->id).end()) {
            for (const auto& exchangeSubscriptionId : this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnectionPtr->id).at(id)) {
              std::string channelId =
                  this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
              std::string symbolId =
                  this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
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
        message.setType(success ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(success ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessageView);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
        event.setMessageList(messageList);
      }
    } else if (document.IsObject() && document.HasMember("data")) {
      std::string exchangeSubscriptionId = document["topic"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
      const rj::Value& data = document["data"];
      if (channelId.rfind(CCAPI_WEBSOCKET_BYBIT_CHANNEL_ORDERBOOK, 0) == 0) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(document["cts"].GetString())));
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        std::string type = document["type"].GetString();
        marketDataMessage.recapType = type == "snapshot" ? MarketDataMessage::RecapType::SOLICITED : MarketDataMessage::RecapType::NONE;
        for (const auto& x : data["b"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : data["a"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channelId == CCAPI_WEBSOCKET_BYBIT_CHANNEL_TRADE) {
        for (const auto& x : data.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["T"].GetString())));
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["p"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["v"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["i"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["S"].GetString()) == "Buy" ? "0" : "1");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } else if (channelId.rfind(CCAPI_WEBSOCKET_BYBIT_CHANNEL_KLINE_2, 0) == 0) {
        for (const auto& x : data.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["start"].GetString())));
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, x["open"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, x["high"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, x["low"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, x["close"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, x["volume"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, x["turnover"].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      }
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PUBLIC_REQUEST: {
        MarketDataService::convertRequestForRestGenericPublicRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
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
                              {CCAPI_START_TIME_SECONDS, "start"},
                              {CCAPI_END_TIME_SECONDS, "end"},
                          },
                          {
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS,
                               [that = shared_from_base<MarketDataServiceBybit>()](const std::string& input) {
                                 return that->convertCandlestickIntervalSecondsToInterval(std::stoi(input));
                               }},
                              {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBybit>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                              {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceBybit>()](
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
                              {CCAPI_LIMIT, "limit"},
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
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "category"},
                          });
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId, "symbol");
        }
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "category"},
                              {CCAPI_LIMIT, "limit"},
                          });
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_BBOS: {
        req.method(http::verb::get);
        auto target = this->getBbosTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "category"},
                          });
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractInstrumentInfo(Element& element, const rj::Value& x, const std::string& category) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseCoin"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteCoin"].GetString());
    auto it = x.FindMember("settleCoin");
    if (it != x.MemberEnd()) {
      element.insert(CCAPI_SETTLE_ASSET, it->value.GetString());
    }
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["priceFilter"]["tickSize"].GetString());
    if (category == "spot") {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSizeFilter"]["basePrecision"].GetString());
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["lotSizeFilter"]["minOrderQty"].GetString());
      element.insert(CCAPI_ORDER_QUOTE_QUANTITY_MIN, x["lotSizeFilter"]["minOrderAmt"].GetString());
      element.insert(CCAPI_ORDER_QUOTE_QUANTITY_MAX, x["lotSizeFilter"]["maxOrderAmt"].GetString());
    } else if (category == "linear" || category == "inverse") {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSizeFilter"]["qtyStep"].GetString());
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["lotSizeFilter"]["minOrderQty"].GetString());
      element.insert(CCAPI_ORDER_QUANTITY_MAX, x["lotSizeFilter"]["maxOrderQty"].GetString());
      element.insert(CCAPI_ORDER_QUOTE_QUANTITY_MIN, x["lotSizeFilter"]["minNotionalValue"].GetString());
    } else if (category == "option") {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSizeFilter"]["qtyStep"].GetString());
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["lotSizeFilter"]["minOrderQty"].GetString());
      element.insert(CCAPI_ORDER_QUANTITY_MAX, x["lotSizeFilter"]["maxOrderQty"].GetString());
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
        for (const auto& x : document["result"]["list"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["time"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["size"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "sell" ? "0" : "1");
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["execId"].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        for (const auto& x : document["result"]["list"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x[0].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, x[1].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, x[2].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, x[3].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, x[4].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, x[5].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, x[6].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_MARKET_DEPTH: {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        const rj::Value& result = document["result"];
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(result["ts"].GetString()));
        for (const auto& x : result["b"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : result["a"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } break;
      case Request::Operation::GET_SERVER_TIME: {
        Message message;
        message.setTime(UtilTime::makeTimePoint(UtilTime::divideNanoWhole(document["result"]["timeNano"].GetString())));
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::string category = document["result"]["category"].GetString();
        for (const auto& x : document["result"]["list"].GetArray()) {
          if (std::string_view(x["symbol"].GetString()) == request.getInstrument()) {
            Element element;
            this->extractInstrumentInfo(element, x, category);
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
        std::string category = document["result"]["category"].GetString();
        std::vector<Element> elementList;
        for (const auto& x : document["result"]["list"].GetArray()) {
          Element element;
          this->extractInstrumentInfo(element, x, category);
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
        for (const auto& x : document["result"]["list"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
          element.insert(CCAPI_BEST_BID_N_PRICE, x["bid1Price"].GetString());
          element.insert(CCAPI_BEST_BID_N_SIZE, x["bid1Size"].GetString());
          element.insert(CCAPI_BEST_ASK_N_PRICE, x["ask1Price"].GetString());
          element.insert(CCAPI_BEST_ASK_N_SIZE, x["ask1Size"].GetString());
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
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_H_
