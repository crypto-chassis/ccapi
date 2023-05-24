#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGemini : public MarketDataService {
 public:
  MarketDataServiceGemini(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GEMINI;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v1/marketdata";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->getRecentTradesTarget = "/v1/trades/:symbol";
    this->getInstrumentTarget = "/v1/symbols/details/:symbol";
    this->getInstrumentsTarget = "/v1/symbols";
  }
  virtual ~MarketDataServiceGemini() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        int marketDepthSubscribedToExchange = 1;
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
      }
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override { return std::vector<std::string>(); }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void onOpen(wspp::connection_hdl hdl) override {
    MarketDataService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    std::vector<std::string> correlationIdList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
        if (marketDepthSubscribedToExchange == 1) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        auto exchangeSubscriptionId = wsConnection.url;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        std::vector<std::string> correlationIdList_2 = this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
        correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
      }
    }
    auto timeReceived = UtilTime::now();
    Event event;
    event.setType(Event::Type::SUBSCRIPTION_STATUS);
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList(correlationIdList);
    message.setType(Message::Type::SUBSCRIPTION_STARTED);
    messageList.emplace_back(std::move(message));
    event.setMessageList(messageList);
    this->eventHandler(event, nullptr);
  }
  void onClose(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->sequenceByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
  }
  bool checkSequence(const WsConnection& wsConnection, int sequence) {
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) == this->sequenceByConnectionIdMap.end()) {
      if (sequence != this->sessionConfigs.getInitialSequenceByExchangeMap().at(this->exchangeName)) {
        CCAPI_LOGGER_WARN("incorrect initial sequence, wsConnection = " + toString(wsConnection));
        return false;
      }
      this->sequenceByConnectionIdMap.insert(std::pair<std::string, int>(wsConnection.id, sequence));
      return true;
    } else {
      if (sequence - this->sequenceByConnectionIdMap[wsConnection.id] == 1) {
        this->sequenceByConnectionIdMap[wsConnection.id] = sequence;
        return true;
      } else {
        return false;
      }
    }
  }
  void onOutOfSequence(WsConnection& wsConnection, int sequence, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived,
                       const std::string& exchangeSubscriptionId) {
    int previous = 0;
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) != this->sequenceByConnectionIdMap.end()) {
      previous = this->sequenceByConnectionIdMap[wsConnection.id];
    }
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::INCORRECT_STATE_FOUND,
                  "out of sequence: previous = " + toString(previous) + ", current = " + toString(sequence) + ", connection = " + toString(wsConnection) +
                      ", textMessage = " + textMessage + ", timeReceived = " + UtilTime::getISOTimestamp(timeReceived));
    ErrorCode ec;
    this->close(wsConnection, hdl, websocketpp::close::status::normal, "out of sequence", ec);
    if (ec) {
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
  }
#else
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    MarketDataService::onOpen(wsConnectionPtr);
    std::vector<std::string> correlationIdList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
        if (marketDepthSubscribedToExchange == 1) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
        auto exchangeSubscriptionId = wsConnectionPtr->url;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        std::vector<std::string> correlationIdList_2 =
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
        correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
      }
    }
    auto timeReceived = UtilTime::now();
    Event event;
    event.setType(Event::Type::SUBSCRIPTION_STATUS);
    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList(correlationIdList);
    message.setType(Message::Type::SUBSCRIPTION_STARTED);
    messageList.emplace_back(std::move(message));
    event.setMessageList(messageList);
    this->eventHandler(event, nullptr);
  }
  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    this->sequenceByConnectionIdMap.erase(wsConnectionPtr->id);
    MarketDataService::onClose(wsConnectionPtr, ec);
  }
  bool checkSequence(std::shared_ptr<WsConnection> wsConnectionPtr, int sequence) {
    if (this->sequenceByConnectionIdMap.find(wsConnectionPtr->id) == this->sequenceByConnectionIdMap.end()) {
      if (sequence != this->sessionConfigs.getInitialSequenceByExchangeMap().at(this->exchangeName)) {
        CCAPI_LOGGER_WARN("incorrect initial sequence, wsConnection = " + toString(*wsConnectionPtr));
        return false;
      }
      this->sequenceByConnectionIdMap.insert(std::pair<std::string, int>(wsConnectionPtr->id, sequence));
      return true;
    } else {
      if (sequence - this->sequenceByConnectionIdMap[wsConnectionPtr->id] == 1) {
        this->sequenceByConnectionIdMap[wsConnectionPtr->id] = sequence;
        return true;
      } else {
        return false;
      }
    }
  }
  void onOutOfSequence(std::shared_ptr<WsConnection> wsConnectionPtr, int sequence, boost::beast::string_view textMessageView, const TimePoint& timeReceived,
                       const std::string& exchangeSubscriptionId) {
    int previous = 0;
    if (this->sequenceByConnectionIdMap.find(wsConnectionPtr->id) != this->sequenceByConnectionIdMap.end()) {
      previous = this->sequenceByConnectionIdMap[wsConnectionPtr->id];
    }
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::INCORRECT_STATE_FOUND,
                  "out of sequence: previous = " + toString(previous) + ", current = " + toString(sequence) + ", connection = " + toString(*wsConnectionPtr) +
                      ", textMessage = " + std::string(textMessageView) + ", timeReceived = " + UtilTime::getISOTimestamp(timeReceived));
    ErrorCode ec;
    this->close(wsConnectionPtr, beast::websocket::close_code::normal, beast::websocket::close_reason(beast::websocket::close_code::normal, "out of sequence"),
                ec);
    if (ec) {
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnectionPtr->id] = false;
  }
#endif
  void processTextMessage(
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage
#else
      std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView
#endif
      ,
      const TimePoint& timeReceived, Event& event, std::vector<MarketDataMessage>& marketDataMessageList) override {
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    WsConnection& wsConnection = *wsConnectionPtr;
    std::string textMessage(textMessageView);
#endif
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    auto type = std::string(document["type"].GetString());
    if (this->sessionOptions.enableCheckSequence) {
      int sequence = std::stoi(document["socket_sequence"].GetString());
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      if (!this->checkSequence(wsConnection, sequence)) {
        this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, "");
        return;
      }
#else
      if (!this->checkSequence(wsConnectionPtr, sequence)) {
        this->onOutOfSequence(wsConnectionPtr, sequence, textMessageView, timeReceived, "");
        return;
      }
#endif
    }
    if (type == "update" && !document["events"].GetArray().Empty()) {
      MarketDataMessage marketDataMessage;
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
      marketDataMessage.exchangeSubscriptionId = wsConnection.url;
#else
      marketDataMessage.exchangeSubscriptionId = wsConnectionPtr->url;
#endif
      TimePoint time = timeReceived;
      auto it = document.FindMember("timestampms");
      if (it != document.MemberEnd()) {
        time = TimePoint(std::chrono::milliseconds(std::stoll(it->value.GetString())));
      }
      for (auto& event : document["events"].GetArray()) {
        auto gType = std::string(event["type"].GetString());
        if (gType == "change") {
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(event["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(event["remaining"].GetString())});
          auto isBid = std::string(event["side"].GetString()) == "bid";
          std::string reason = event["reason"].GetString();
          if (reason == "place" || reason == "cancel" || reason == "trade") {
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = time;
            if (isBid) {
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            } else {
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
          } else if (reason == "initial") {
            marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            marketDataMessage.tp = time;
            if (isBid) {
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            } else {
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
          } else if (reason == "top-of-book") {
            marketDataMessage.tp = time;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            if (isBid) {
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            } else {
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
          }
        } else if (gType == "trade") {
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          std::string makerSide = event["makerSide"].GetString();
          if (makerSide == "bid" || makerSide == "ask") {
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = time;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(event["price"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(event["amount"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(event["tid"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, makerSide == "bid" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          }
        }
      }
      marketDataMessageList.emplace_back(std::move(marketDataMessage));
    }
  }
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto instrument = subscription.getInstrument();
    auto symbolId = instrument;
    auto field = subscription.getField();
    auto parameterList = UtilString::split(this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(this->exchangeName).at(field), ",");
    std::set<std::string> parameterSet(parameterList.begin(), parameterList.end());
    std::string url = this->baseUrlWs + "/" + symbolId;
    url += "?";
    if ((parameterSet.find(CCAPI_WEBSOCKET_GEMINI_PARAMETER_BIDS) != parameterSet.end() ||
         parameterSet.find(CCAPI_WEBSOCKET_GEMINI_PARAMETER_OFFERS) != parameterSet.end())) {
      auto optionMap = subscription.getOptionMap();
      if (std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)) == 1) {
        parameterSet.insert(CCAPI_WEBSOCKET_GEMINI_PARAMETER_TOP_OF_BOOK);
      }
    }
    parameterSet.insert("heartbeat");
    bool isFirstParameter = true;
    for (auto const& parameter : parameterSet) {
      if (isFirstParameter) {
        isFirstParameter = false;
      } else {
        url += "&";
      }
      url += parameter + "=true";
    }
    return url;
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
        this->substituteParam(target, {
                                          {":symbol", symbolId},
                                      });
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit_trades"},
                          });
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        this->substituteParam(target, {
                                          {":symbol", symbolId},
                                      });
        req.target(target);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        req.target(this->getInstrumentsTarget);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["base_currency"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote_currency"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, Decimal(x["quote_increment"].GetString()).toString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, Decimal(x["tick_size"].GetString()).toString());
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["timestampms"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, x["tid"].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["type"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        Element element;
        this->extractInstrumentInfo(element, document);
        message.setElementList({element});
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x.GetString());
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
  std::map<std::string, int> sequenceByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
