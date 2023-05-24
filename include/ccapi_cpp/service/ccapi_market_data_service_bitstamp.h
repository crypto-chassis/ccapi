#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceBitstamp : public MarketDataService {
 public:
  MarketDataServiceBitstamp(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                            std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITSTAMP;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->enableCheckPingPongWebsocketApplicationLevel = false;
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
    this->getRecentTradesTarget = "/api/v2/transactions/{currency_pair}/";  // must have trailing slash
    this->getInstrumentTarget = "/api/v2/trading-pairs-info/";
    this->getInstrumentsTarget = "/api/v2/trading-pairs-info/";
  }
  virtual ~MarketDataServiceBitstamp() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    this->send(hdl, R"({"event": "bts:heartbeat"})", wspp::frame::opcode::text, ec);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"event": "bts:heartbeat"})", ec);
  }
#endif
  bool doesHttpBodyContainError(const std::string& body) override {
    return body.find(R"("status": "error")") != std::string::npos || body.find(R"("status":"error")") != std::string::npos;
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("bts:subscribe").Move(), allocator);
        rj::Value data(rj::kObjectType);
        auto symbolId = subscriptionListByInstrument.first;
        if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = channelId + "_" + symbolId;
        data.AddMember("channel", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        document.AddMember("data", data, allocator);
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
    const rj::Value& data = document["data"];
    if (document.IsObject() && document.HasMember("event") && std::string(document["event"].GetString()) == "bts:subscription_succeeded") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        std::string exchangeSubscriptionId = document["channel"].GetString();
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
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
      event.setMessageList(messageList);
    } else if (document.IsObject() && document.HasMember("event") &&
               (std::string(document["event"].GetString()) == "data" || std::string(document["event"].GetString()) == "trade")) {
      MarketDataMessage marketDataMessage;
      std::string exchangeSubscriptionId = document["channel"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_ORDER_BOOK) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        } else {
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
        }
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : data["bids"].GetArray()) {
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
        for (const auto& x : data["asks"].GetArray()) {
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
      } else if (channelId == CCAPI_WEBSOCKET_BITSTAMP_CHANNEL_LIVE_TRADES) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        std::string microtimestamp = data["microtimestamp"].GetString();
        microtimestamp.insert(microtimestamp.size() - 6, ".");
        marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(microtimestamp));
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(data["price_str"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(data["amount_str"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(data["id"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(data["type"].GetString()) == "0" ? "1" : "0"});
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
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
        this->substituteParam(target, {
                                          {"{currency_pair}", symbolId},
                                      });
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param, {});
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        req.target(target);
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
    element.insert(CCAPI_INSTRUMENT, x["url_symbol"].GetString());
    std::string name = x["name"].GetString();
    auto splitted = UtilString::split(name, "/");
    auto baseAsset = splitted.at(0);
    auto quoteAsset = splitted.at(1);
    element.insert(CCAPI_BASE_ASSET, baseAsset);
    element.insert(CCAPI_QUOTE_ASSET, quoteAsset);
    int counterDecimals = std::stoi(x["counter_decimals"].GetString());
    if (counterDecimals > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(counterDecimals - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1");
    }
    int baseDecimals = std::stoi(x["base_decimals"].GetString());
    if (baseDecimals > 0) {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(baseDecimals - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1");
    }
    auto splittedMinimumOrder = UtilString::split(x["minimum_order"].GetString(), ' ');
    if (splittedMinimumOrder.size() == 2) {
      if (splittedMinimumOrder.at(1) == quoteAsset) {
        element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, splittedMinimumOrder.at(0));
      } else if (splittedMinimumOrder.at(1) == baseAsset) {
        element.insert(CCAPI_ORDER_QUANTITY_MIN, splittedMinimumOrder.at(0));
      }
    }
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
          marketDataMessage.tp = UtilTime::makeTimePointFromSeconds(std::stol(x["date"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["tid"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["type"].GetString()) == "0" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document.GetArray()) {
          if (std::string(x["url_symbol"].GetString()) == request.getInstrument()) {
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
        for (const auto& x : document.GetArray()) {
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
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITSTAMP_H_
