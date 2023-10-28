#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_WHITEBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_WHITEBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_WHITEBIT
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceWhitebit : public MarketDataService {
 public:
  MarketDataServiceWhitebit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                            ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_WHITEBIT;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
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
    this->getRecentTradesTarget = "/api/v4/public/trades/{market}";
    this->getInstrumentsTarget = "/api/v4/public/markets";
    this->methodDepthSubscribe = std::string(CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_DEPTH) + "_subscribe";
    this->methodTradesSubscribe = std::string(CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES) + "_subscribe";
    this->methodDepthUpdate = std::string(CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_DEPTH) + "_update";
    this->methodTradesUpdate = std::string(CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES) + "_update";
    this->shouldAlignSnapshot = true;
  }
  virtual ~MarketDataServiceWhitebit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->send(hdl, R"({"id":)" + std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]) + R"(,"method":"ping","params":[]})",
               wspp::frame::opcode::text, ec);
    this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr,
               R"({"id":)" + std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id]) + R"(,"method":"ping","params":[]})", ec);
    this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id] += 1;
  }
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      int marketDepthSubscribedToExchange = marketDepthRequested;
      // channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
      this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      if (channelId == CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_DEPTH) {
        for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListByInstrument.first;
          auto exchangeSubscriptionId = channelId + subscriptionListByInstrument.first;
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
          document.AddMember("method", rj::Value("depth_subscribe").Move(), allocator);
          rj::Value params(rj::kArrayType);
          params.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          params.PushBack(rj::Value(marketDepthSubscribedToExchange).Move(), allocator);
          params.PushBack(rj::Value("0").Move(), allocator);
          params.PushBack(rj::Value(true).Move(), allocator);
          document.AddMember("params", params, allocator);
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          sendStringList.push_back(sendString);
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id]
                                                                                [this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] = {
              exchangeSubscriptionId};
          this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
        }
      } else if (channelId == CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
        document.AddMember("method", rj::Value("trades_subscribe").Move(), allocator);
        rj::Value params(rj::kArrayType);
        std::vector<std::string> exchangeSubscriptionIdList;
        for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListByInstrument.first;
          params.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          auto exchangeSubscriptionId = channelId + subscriptionListByInstrument.first;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
        }
        document.AddMember("params", params, allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
        this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id][this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
            exchangeSubscriptionIdList;
        this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
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
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    auto itId = document.FindMember("id");
    if (itId == document.MemberEnd() || itId->value.IsNull()) {
      std::string method = document["method"].GetString();
      const auto& params = document["params"];
      if (method == this->methodDepthUpdate) {
        bool isFullReload = params[0].GetBool();
        std::string symbolId = params[2].GetString();
        const auto& exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_DEPTH) + symbolId;
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.recapType = isFullReload ? MarketDataMessage::RecapType::SOLICITED : MarketDataMessage::RecapType::NONE;
        marketDataMessage.tp = timeReceived;
        auto itBids = params[1].FindMember("bids");
        if (itBids != params[1].MemberEnd()) {
          const rj::Value& bids = itBids->value;
          for (const auto& x : bids.GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          }
        }
        auto itAsks = params[1].FindMember("asks");
        if (itAsks != params[1].MemberEnd()) {
          const rj::Value& asks = itAsks->value;
          for (const auto& x : asks.GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          }
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (method == this->methodTradesUpdate) {
        std::string symbolId = params[0].GetString();
        const auto& exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES) + symbolId;
        for (const auto& x : params[1].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          auto timePair = UtilTime::divide(std::string(x["time"].GetString()));
          auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
          tp += std::chrono::nanoseconds(timePair.second);
          marketDataMessage.tp = tp;
          if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES][symbolId]) {
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          } else {
            marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][CCAPI_WEBSOCKET_WHITEBIT_CHANNEL_MARKET_TRADES][symbolId] = true;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["id"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["type"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      }
    } else {
      int id = std::stoi(itId->value.GetString());
      auto itResult = document.FindMember("result");
      if (itResult != document.MemberEnd()) {
        if (itResult->value.IsObject()) {
          const auto& result = itResult->value;
          auto itError = document.FindMember("error");
          bool success = itError == document.MemberEnd() || itError->value.IsNull();
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          std::vector<std::string> correlationIdList;
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
            if (this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.find(wsConnection.id) !=
                    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.end() &&
                this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).find(id) !=
                    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).end()) {
              for (const auto& exchangeSubscriptionId : this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).at(id)) {
                std::string channelId =
                    this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
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
            }
          }
          message.setCorrelationIdList(correlationIdList);
          message.setType(success ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(success ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
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
        this->substituteParam(target, {
                                          {"{market}", symbolId},
                                      });
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param, {});
        req.target(queryString.empty() ? target : target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
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
    element.insert(CCAPI_INSTRUMENT, x["name"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["stock"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["money"].GetString());
    int moneyPrec = std::stoi(x["moneyPrec"].GetString());
    if (moneyPrec > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(moneyPrec - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1" + std::string(-moneyPrec, '0'));
    }
    int stockPrec = std::stoi(x["stockPrec"].GetString());
    if (stockPrec > 0) {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(stockPrec - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1" + std::string(-stockPrec, '0'));
    }
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minAmount"].GetString());
    element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, x["minTotal"].GetString());
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
          auto timePair = UtilTime::divide(std::string(x["trade_timestamp"].GetString()));
          auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
          tp += std::chrono::nanoseconds(timePair.second);
          marketDataMessage.tp = tp;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["base_volume"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["tradeID"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["type"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document.GetArray()) {
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
        for (const auto& x : document.GetArray()) {
          Element element;
          this->extractInstrumentInfo(element, x);
          elementList.emplace_back(std::move(element));
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  std::string methodDepthSubscribe, methodDepthUpdate, methodTradesSubscribe, methodTradesUpdate;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_WHITEBIT_H_
