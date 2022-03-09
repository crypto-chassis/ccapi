#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceBybit : public MarketDataService {
 public:
  MarketDataServiceBybit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                         std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BYBIT;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/spot/quote/ws/v2";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->getRecentTradesTarget = "/spot/quote/v1/trades";
    this->getInstrumentTarget = "/spot/v1/symbols";
    this->getInstrumentsTarget = "/spot/v1/symbols";
  }
  virtual ~MarketDataServiceBybit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = CCAPI_WEBSOCKET_BYBIT_CHANNEL_BOOK_TICKER;
      } else {
        channelId = CCAPI_WEBSOCKET_BYBIT_CHANNEL_DEPTH;
      }
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        auto exchangeSubscriptionId = channelId;
        if (channelId == CCAPI_WEBSOCKET_BYBIT_CHANNEL_BOOK_TICKER || channelId == CCAPI_WEBSOCKET_BYBIT_CHANNEL_DEPTH) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        exchangeSubscriptionId += "|";
        exchangeSubscriptionId += symbolId;
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        document.AddMember("topic", rj::Value(channelId.c_str(), allocator).Move(), allocator);
        document.AddMember("event", rj::Value("sub").Move(), allocator);
        rj::Value params(rj::kObjectType);
        params.AddMember("symbol", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        params.AddMember("binary", rj::Value(false), allocator);
        document.AddMember("params", params, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
    return sendStringList;
  }
  void processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    if (document.IsObject() && document.HasMember("code")) {
      std::string code = document["code"].GetString();
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        std::string channelId = document["topic"].GetString();
        std::string symbolId = document["params"]["symbol"].GetString();
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
      message.setType(code == "0" ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(code == "0" ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
      event.setMessageList(messageList);
    } else if (document.IsObject() && document.HasMember("data")) {
      MarketDataMessage marketDataMessage;
      std::string channelId = document["topic"].GetString();
      std::string symbolId = document["params"]["symbol"].GetString();
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      std::string exchangeSubscriptionId = channelId + "|" + symbolId;
      const rj::Value& data = document["data"];
      if (channelId == CCAPI_WEBSOCKET_BYBIT_CHANNEL_BOOK_TICKER) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(data["time"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data["bidPrice"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["bidQty"].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data["askPrice"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["askQty"].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channelId == CCAPI_WEBSOCKET_BYBIT_CHANNEL_DEPTH) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(data["t"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        const char* bidsName = "b";
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : data[bidsName].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          ++bidIndex;
        }
        const char* asksName = "a";
        int askIndex = 0;
        for (const auto& x : data[asksName].GetArray()) {
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
      } else if (channelId == CCAPI_WEBSOCKET_BYBIT_CHANNEL_TRADE) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(data["t"].GetString()));
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(data["p"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(data["q"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(data["v"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, data["m"].GetBool() ? "1" : "0"});
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
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        req.target(target);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        req.target(target);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["name"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseCurrency"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteCurrency"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["minPricePrecision"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["basePrecision"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minTradeQuantity"].GetString());
    element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, x["minTradeAmount"].GetString());
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["time"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["qty"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, x["isBuyerMaker"].GetBool() ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["result"].GetArray()) {
          if (std::string(x["name"].GetString()) == request.getInstrument()) {
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
        for (const auto& x : document["result"].GetArray()) {
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BYBIT_H_
