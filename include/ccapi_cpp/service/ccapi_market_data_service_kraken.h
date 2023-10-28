#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KRAKEN_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KRAKEN_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceKraken : public MarketDataService {
 public:
  MarketDataServiceKraken(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_KRAKEN;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    //     try {
    //       this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->getRecentTradesTarget = "/0/public/Trades";
    this->getInstrumentTarget = "/0/public/AssetPairs";
    this->getInstrumentsTarget = "/0/public/AssetPairs";
    this->shouldAlignSnapshot = true;
  }
  virtual ~MarketDataServiceKraken() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"reqid\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"event\":\"ping\"}", wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr, "{\"reqid\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + ",\"event\":\"ping\"}", ec);
  }
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("error":[])") == std::string::npos; }
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      int marketDepthSubscribedToExchange = 1;
      marketDepthSubscribedToExchange = this->calculateMarketDepthAllowedByExchange(marketDepthRequested, std::vector<int>({10, 25, 100, 500, 1000}));
      channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
      this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval = std::to_string(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)) / 60);
      channelId += "-" + interval;
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      if (channelId.rfind(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK, 0) == 0) {
        std::map<int, std::vector<std::string> > symbolIdListByMarketDepthSubscribedToExchangeMap;
        for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListBySymbolId.first;
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          std::string exchangeSubscriptionId =
              std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK) + "-" + std::to_string(marketDepthSubscribedToExchange) + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          symbolIdListByMarketDepthSubscribedToExchangeMap[marketDepthSubscribedToExchange].push_back(symbolId);
        }
        for (const auto& x : symbolIdListByMarketDepthSubscribedToExchangeMap) {
          auto marketDepthSubscribedToExchange = x.first;
          auto symbolIdList = x.second;
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("event", rj::Value("subscribe").Move(), allocator);
          rj::Value instrument(rj::kArrayType);
          for (const auto& symbolId : symbolIdList) {
            instrument.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          }
          document.AddMember("pair", instrument, allocator);
          rj::Value subscription(rj::kObjectType);
          subscription.AddMember("depth", rj::Value(marketDepthSubscribedToExchange).Move(), allocator);
          subscription.AddMember("name", rj::Value(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK).c_str(), allocator).Move(), allocator);
          document.AddMember("subscription", subscription, allocator);
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          sendStringList.push_back(sendString);
        }
      } else if (channelId == CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("subscribe").Move(), allocator);
        rj::Value instrument(rj::kArrayType);
        for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListBySymbolId.first;
          std::string exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE) + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          instrument.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        }
        document.AddMember("pair", instrument, allocator);
        rj::Value subscription(rj::kObjectType);
        subscription.AddMember("name", rj::Value(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE).c_str(), allocator).Move(), allocator);
        document.AddMember("subscription", subscription, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      } else if (channelId.rfind(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC, 0) == 0) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("subscribe").Move(), allocator);
        rj::Value instrument(rj::kArrayType);
        for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListBySymbolId.first;
          std::string exchangeSubscriptionId = channelId + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          instrument.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        }
        document.AddMember("pair", instrument, allocator);
        rj::Value subscription(rj::kObjectType);
        subscription.AddMember("interval", rj::Value(std::stoi(channelId.substr(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC).length() + 1))).Move(),
                               allocator);
        subscription.AddMember("name", rj::Value(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC).c_str(), allocator).Move(), allocator);
        document.AddMember("subscription", subscription, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
    return sendStringList;
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
    rj::Document document;
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    if (document.IsArray() && document.Size() >= 4 && document.Size() <= 5) {
      auto documentSize = document.Size();
      auto channelNameWithSuffix = std::string(document[documentSize - 2].GetString());
      if (channelNameWithSuffix.rfind(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK, 0) == 0) {
        auto symbolId = std::string(document[documentSize - 1].GetString());
        auto exchangeSubscriptionId = channelNameWithSuffix + "|" + symbolId;
        auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        const rj::Value& anonymous = document[1];
        if (anonymous.IsObject() && (anonymous.HasMember("b") || anonymous.HasMember("a"))) {
          rj::Value anonymous2(anonymous, allocator);
          if (documentSize == 5) {
            rj::Value& source = document[2];
            for (rj::Value::MemberIterator itr = source.MemberBegin(); itr != source.MemberEnd(); ++itr) {
              anonymous2.AddMember(itr->name, itr->value, allocator);
            }
          }
          TimePoint latestTp(std::chrono::seconds(0));
          if (anonymous2.HasMember("b")) {
            for (const auto& x : anonymous2["b"].GetArray()) {
              auto timePair = UtilTime::divide(std::string(x[2].GetString()));
              auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
              tp += std::chrono::nanoseconds(timePair.second);
              if (tp > latestTp) {
                latestTp = tp;
              }
            }
          }
          if (anonymous2.HasMember("a")) {
            for (const auto& x : anonymous2["a"].GetArray()) {
              auto timePair = UtilTime::divide(std::string(x[2].GetString()));
              auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
              tp += std::chrono::nanoseconds(timePair.second);
              if (tp > latestTp) {
                latestTp = tp;
              }
            }
          }
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.tp = latestTp;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          if (anonymous2.HasMember("b")) {
            for (const auto& x : anonymous2["b"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            }
          }
          if (anonymous2.HasMember("a")) {
            for (const auto& x : anonymous2["a"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
          }
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        } else if (anonymous.IsObject() && anonymous.HasMember("as") && anonymous.HasMember("bs")) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
          marketDataMessage.tp = timeReceived;
          for (const auto& x : anonymous["bs"].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          }
          for (const auto& x : anonymous["as"].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          }
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } else if (channelNameWithSuffix == CCAPI_WEBSOCKET_KRAKEN_CHANNEL_TRADE) {
        auto symbolId = std::string(document[documentSize - 1].GetString());
        for (const auto& x : document[1].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.exchangeSubscriptionId = channelNameWithSuffix + "|" + symbolId;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          auto timePair = UtilTime::divide(std::string(x[2].GetString()));
          auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
          tp += std::chrono::nanoseconds(timePair.second);
          marketDataMessage.tp = tp;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x[0].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x[1].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x[3].GetString()) == "s" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } else if (channelNameWithSuffix.rfind(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC, 0) == 0) {
        int intervalSeconds = std::stoi(channelNameWithSuffix.substr(std::string(CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC).length() + 1)) * 60;
        auto symbolId = std::string(document[documentSize - 1].GetString());
        const rj::Value& x = document[1].GetArray();
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
        marketDataMessage.exchangeSubscriptionId = channelNameWithSuffix + "|" + symbolId;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        auto timePair = UtilTime::divide(std::string(x[1].GetString()));
        auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first - intervalSeconds));
        marketDataMessage.tp = tp;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, x[2].GetString()});
        dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, x[3].GetString()});
        dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, x[4].GetString()});
        dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, x[5].GetString()});
        dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, x[7].GetString()});
        dataPoint.insert({MarketDataMessage::DataFieldType::QUOTE_VOLUME,
                          Decimal(UtilString::printDoubleScientific(std::stod(x[6].GetString()) * std::stod(x[7].GetString()))).toString()});
        marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    } else if (document.IsObject() && document.HasMember("event")) {
      std::string eventPayload = std::string(document["event"].GetString());
      if (eventPayload == "heartbeat") {
      } else if (eventPayload == "subscriptionStatus") {
        std::string status = document["status"].GetString();
        if (status == "subscribed" || status == "error") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          std::vector<std::string> correlationIdList;
          std::string exchangeSubscriptionId = document["subscription"]["name"].GetString();
          if (exchangeSubscriptionId == CCAPI_WEBSOCKET_KRAKEN_CHANNEL_BOOK) {
            exchangeSubscriptionId += "-" + std::string(document["subscription"]["depth"].GetString());
          } else if (exchangeSubscriptionId == CCAPI_WEBSOCKET_KRAKEN_CHANNEL_OHLC) {
            exchangeSubscriptionId += "-" + std::string(document["subscription"]["interval"].GetString());
          }
          std::string symbolId = document["pair"].GetString();
          exchangeSubscriptionId += "|" + symbolId;
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
            auto channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
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
          message.setType(status == "subscribed" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(status == "subscribed" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          event.setMessageList(messageList);
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
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendSymbolId(queryString, symbolId, "pair");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendSymbolId(queryString, symbolId, "pair");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
        req.target(target);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_BASE_ASSET, x["base"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote"].GetString());
    int pairDecimals = std::stoi(x["pair_decimals"].GetString());
    if (pairDecimals > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(pairDecimals - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1");
    }
    int lotDecimals = std::stoi(x["lot_decimals"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(lotDecimals - 1, '0') + "1");
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["ordermin"].GetString());
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    auto instrument = request.getInstrument();
    const std::string& symbolId = instrument;
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"][symbolId.c_str()].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          auto timePair = UtilTime::divide(std::string(x[2].GetString()));
          auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
          tp += std::chrono::nanoseconds(timePair.second);
          marketDataMessage.tp = tp;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x[0].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x[1].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x[3].GetString()) == "s" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        const rj::Value& x = document["result"][request.getInstrument().c_str()];
        Element element;
        this->extractInstrumentInfo(element, x);
        element.insert(CCAPI_INSTRUMENT, request.getInstrument());
        message.setElementList({element});
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (auto itr = document["result"].MemberBegin(); itr != document["result"].MemberEnd(); ++itr) {
          Element element;
          this->extractInstrumentInfo(element, itr->value);
          element.insert(CCAPI_INSTRUMENT, itr->name.GetString());
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KRAKEN_H_
