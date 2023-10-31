#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceHuobiBase : public MarketDataService {
 public:
  MarketDataServiceHuobiBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->needDecompressWebsocketMessage = true;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    ErrorCode ec = this->inflater.init(false, 31);
#else
    this->inflater.setWindowBitsOverride(31);
    ErrorCode ec = this->inflater.init();
#endif
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
    // this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*[eE]?-?\\d*)");
  }
  virtual ~MarketDataServiceHuobiBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"ping\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + "}", wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr, "{\"ping\":" + std::to_string(UtilTime::getUnixTimestamp(now)) + "}", ec);
  }
#endif
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        auto symbolId = subscriptionListByInstrument.first;
        std::string exchangeSubscriptionId;
        if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO;
        } else if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH;
        } else if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL, 0) == 0) {
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL;
        } else if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BY_PRICE_REFRESH_UPDATE, 0) == 0) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          exchangeSubscriptionId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BY_PRICE_REFRESH_UPDATE;
        }
        {
          std::string toReplace("$symbol");
          std::string replacement(symbolId);
          if (this->exchangeName == CCAPI_EXCHANGE_NAME_HUOBI_COIN_SWAP) {
            replacement = UtilString::toUpper(replacement);
          }
          exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), replacement);
        }
        if (channelId.rfind(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BY_PRICE_REFRESH_UPDATE, 0) == 0) {
          std::string toReplace("$levels");

          CCAPI_LOGGER_TRACE("marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap=" +
                             toString(marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap));
          std::string replacement(
              std::to_string(this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId)));

          exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), replacement);
        }
        document.AddMember("sub", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        if (this->isDerivatives) {
          document.AddMember("id", rj::Value(std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).c_str(), allocator).Move(),
                             allocator);
        } else {
          document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
        }
        this->exchangeSubscriptionIdByExchangeJsonPayloadIdMap[this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] = exchangeSubscriptionId;
        this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.emplace_back(std::move(sendString));
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    return sendStringList;
  }
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto url = this->baseUrlWs;
    auto field = subscription.getField();
    if (!this->isDerivatives) {
      if (field == CCAPI_TRADE || field == CCAPI_MARKET_DEPTH || field == CCAPI_GENERIC_PUBLIC_SUBSCRIPTION) {
        url += "/ws";
      }
    }
    return url + "|" + field + "|" + subscription.getSerializedOptions();
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
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    if (document.IsObject() && document.HasMember("ch") && document.HasMember("tick")) {
      std::string exchangeSubscriptionId = document["ch"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO_REGEX))) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        const rj::Value& tick = document["tick"];
        std::string ts = tick[this->isDerivatives ? "ts" : "quoteTime"].GetString();
        ts.insert(ts.size() - 3, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        if (this->isDerivatives) {
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["bid"][0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["bid"][1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          }
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["ask"][0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["ask"][1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else {
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["bid"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["bidSize"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          }
          {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(tick["ask"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(tick["askSize"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        }
      } else if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BY_PRICE_REFRESH_UPDATE_REGEX))) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        const rj::Value& tick = document["tick"];
        std::string ts = document["ts"].GetString();
        ts.insert(ts.size() - 3, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : tick["bids"].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          ++bidIndex;
        }
        int askIndex = 0;
        for (const auto& x : tick["asks"].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          ++askIndex;
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH_REGEX))) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        const rj::Value& tick = document["tick"];
        std::string ts = tick["ts"].GetString();
        ts.insert(ts.size() - 3, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : tick["bids"].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          ++bidIndex;
        }
        int askIndex = 0;
        for (const auto& x : tick["asks"].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          ++askIndex;
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (std::regex_search(channelId, std::regex(CCAPI_WEBSOCKET_HUOBI_CHANNEL_TRADE_DETAIL_REGEX))) {
        const rj::Value& tick = document["tick"];
        for (const auto& x : tick["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          std::string ts = x["ts"].GetString();
          ts.insert(ts.size() - 3, ".");
          auto time = UtilTime::makeTimePoint(UtilTime::divide(ts));
          marketDataMessage.tp = time;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          if (this->isDerivatives) {
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::to_string(std::stoi(x["amount"].GetString()) / 2)});
          } else {
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          }
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x[this->isDerivatives ? "id" : "tradeId"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      }
    } else if (document.IsObject() && document.HasMember("ping")) {
      std::string payload("{\"pong\":");
      payload += document["ping"].GetString();
      payload += "}";
      ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
      this->send(hdl, payload, wspp::frame::opcode::text, ec);
#else
      this->send(wsConnectionPtr, payload, ec);
#endif
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "pong");
      }
    } else if (document.IsObject() && document.HasMember("status")) {
      std::string status = document["status"].GetString();
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        std::string exchangeSubscriptionId = this->exchangeSubscriptionIdByExchangeJsonPayloadIdMap.at(std::stoi(document["id"].GetString()));
        std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
        std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
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
      message.setType(status == "ok" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(status == "ok" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
      event.setMessageList(messageList);
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
                              {CCAPI_LIMIT, "size"},
                          });
        this->appendSymbolId(queryString, symbolId, this->isDerivatives ? "contract_code" : "symbol");
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["base-currency"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote-currency"].GetString());
    int pricePrecision = std::stoi(x["price-precision"].GetString());
    if (pricePrecision > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(pricePrecision - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1");
    }
    int amountPrecision = std::stoi(x["amount-precision"].GetString());
    if (amountPrecision > 0) {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(amountPrecision - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1");
    }
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["limit-order-min-order-amt"].GetString());
    element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, x["min-order-value"].GetString());
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& datum : document["data"].GetArray()) {
          for (const auto& x : datum["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            std::string ts = x["ts"].GetString();
            ts.insert(ts.size() - 3, ".");
            marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(ts));
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
            if (this->isDerivatives) {
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, std::to_string(std::stoi(x["amount"].GetString()) / 2)});
            } else {
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
            }
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x[this->isDerivatives ? "id" : "trade-id"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
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
  std::map<int, std::string> exchangeSubscriptionIdByExchangeJsonPayloadIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_BASE_H_
