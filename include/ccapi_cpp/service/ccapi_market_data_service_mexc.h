#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_MEXC_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_MEXC_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_MEXC
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceMexc : public MarketDataService {
 public:
  MarketDataServiceMexc(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                        ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_MEXC;
    this->baseUrlWs = std::string(CCAPI_MEXC_URL_WS_BASE) + "/ws";
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
    this->getRecentTradesTarget = "/api/v3/trades";
    this->getRecentAggTradesTarget = "/api/v3/aggTrades";
    this->getInstrumentsTarget = "/api/v3/exchangeInfo";
  }
  virtual ~MarketDataServiceMexc() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  // bool doesHttpBodyContainError(const std::string& body) override { return body.find(R"("code":0)") == std::string::npos; }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->send(hdl, R"({"id":)" + std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]) + R"(,"method":"PING"})",
               wspp::frame::opcode::text, ec);
    this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"id":)" + std::to_string(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id]) + R"(,"method":"PING"})", ec);
    this->exchangeJsonPayloadIdByConnectionIdMap[wsConnectionPtr->id] += 1;
  }
#endif
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("method", rj::Value("SUBSCRIPTION").Move(), allocator);
    rj::Value params(rj::kArrayType);
    std::vector<std::string> exchangeSubscriptionIdList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        auto exchangeSubscriptionId = channelId + "@" + subscriptionListByInstrument.first;
        params.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
      }
    }
    document.AddMember("params", params, allocator);
    document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id][this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
        exchangeSubscriptionIdList;
    this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
  void createFetchOrderBookInitialReq(http::request<http::string_body>& req, const std::string& symbolId, const TimePoint& now,
                                      const std::map<std::string, std::string>& credential) override {
    req.set(http::field::host, this->hostRest);
    req.method(http::verb::get);
    req.target("/api/v3/depth?symbol=" + Url::urlEncode(symbolId) + "&limit=5000");
  }
  void extractOrderBookInitialVersionId(int64_t& versionId, const rj::Document& document) override {
    versionId = std::stoll(document["lastUpdateId"].GetString());
  }
  void extractOrderBookInitialData(MarketDataMessage::TypeForData& input, const rj::Document& document) override {
    for (const auto& x : document["bids"].GetArray()) {
      MarketDataMessage::TypeForDataPoint dataPoint;
      dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
      dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
      input[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
    }
    for (const auto& x : document["asks"].GetArray()) {
      MarketDataMessage::TypeForDataPoint dataPoint;
      dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
      dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
      input[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
    }
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
    if (document.IsObject() && document.HasMember("code") && std::string(document["code"].GetString()) == "0") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      std::vector<std::string> correlationIdListSuccess, correlationIdListFailure;
      std::string msg = document["msg"].GetString();
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        int id = std::stoi(document["id"].GetString());
        if (this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.find(wsConnection.id) !=
                this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.end() &&
            this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).find(id) !=
                this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).end()) {
          for (const auto& exchangeSubscriptionId : this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).at(id)) {
            std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
            std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
            if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).find(channelId) !=
                this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).end()) {
              if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).find(symbolId) !=
                  this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).end()) {
                std::vector<std::string> correlationIdList_2 =
                    this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
                for (const auto& correlationId : correlationIdList_2) {
                  if (exchangeSubscriptionId == msg) {
                    correlationIdListSuccess.push_back(correlationId);
                  } else {
                    correlationIdListFailure.push_back(correlationId);
                  }
                }
              }
            }
          }
        }
      }
      if (!correlationIdListSuccess.empty()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList(correlationIdListSuccess);
        message.setType(Message::Type::SUBSCRIPTION_STARTED);
        Element element;
        element.insert(CCAPI_INFO_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
      if (!correlationIdListFailure.empty()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setCorrelationIdList(correlationIdListFailure);
        message.setType(Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(CCAPI_ERROR_MESSAGE, textMessage);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
      }
      event.setMessageList(messageList);
    } else if (document.IsObject() && document.HasMember("c") && document.HasMember("d")) {
      std::string exchangeSubscriptionId = document["c"].GetString();
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      const auto& optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      const auto& correlationIdList = this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
      const rj::Value& data = document["d"];
      if (channelId == CCAPI_WEBSOCKET_MEXC_CHANNEL_DIFF_DEPTH) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(document["t"].GetString()));
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        const auto& itAsks = data.FindMember("asks");
        if (itAsks != data.MemberEnd()) {
          const rj::Value& asks = itAsks->value;
          for (auto& x : asks.GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["p"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x["v"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          }
        }
        const auto& itBids = data.FindMember("bids");
        if (itBids != data.MemberEnd()) {
          const rj::Value& bids = itBids->value;
          for (auto& x : bids.GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x["p"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x["v"].GetString())});
            marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          }
        }
        int64_t versionId = std::stoll(document["d"]["r"].GetString());
        this->processOrderBookWithVersionId(versionId, wsConnection, channelId, symbolId, exchangeSubscriptionId, optionMap, marketDataMessageList,
                                            marketDataMessage);
      } else if (channelId == CCAPI_WEBSOCKET_MEXC_CHANNEL_TRADE) {
        for (const auto& x : data["deals"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["t"].GetString()));
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["p"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["v"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["S"].GetString()) == "2" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
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
      case Request::Operation::GET_RECENT_AGG_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentAggTradesTarget;
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
        auto target = this->getInstrumentsTarget;
        std::string queryString;
        this->appendSymbolId(queryString, symbolId, "symbol");
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
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["baseAsset"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quoteAsset"].GetString());
    int quotePrecision = std::stoi(x["quotePrecision"].GetString());
    if (quotePrecision > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(quotePrecision - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1" + std::string(-quotePrecision, '0'));
    }
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["baseSizePrecision"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["baseSizePrecision"].GetString());
    element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, x["quoteAmountPrecision"].GetString());
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
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["time"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["qty"].GetString()))});
          const auto& it = x.FindMember("id");
          if (it != x.MemberEnd() && !it->value.IsNull()) {
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, it->value.GetString()});
          }
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, x["isBuyerMaker"].GetBool() ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_AGG_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_AGG_TRADE;
          marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(x["T"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["p"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["q"].GetString()))});
          const auto& it = x.FindMember("a");
          if (it != x.MemberEnd() && !it->value.IsNull()) {
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, it->value.GetString()});
          }
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, x["m"].GetBool() ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::AGG_TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["symbols"].GetArray()) {
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
        for (const auto& x : document["symbols"].GetArray()) {
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
  std::string getRecentAggTradesTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_MEXC_H_
