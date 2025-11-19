#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_FTX) || defined(CCAPI_ENABLE_EXCHANGE_FTX_US)
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceFtxBase : public MarketDataService {
 public:
  MarketDataServiceFtxBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                           ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->shouldAlignSnapshot = true;
    this->getRecentTradesTarget = "/api/markets/{market_name}/trades";
    this->getInstrumentTarget = "/api/markets/{market_name}";
    this->getInstrumentsTarget = "/api/markets";
    // this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)\\s?(-?\\d+\\.?\\d*[eE]?-?\\d*)");
  }

  virtual ~MarketDataServiceFtxBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_TICKER;
        this->shouldAlignSnapshot = false;
      } else {
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = 100;
      }
    }
  }

  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, R"({"op":"ping"})", wspp::frame::opcode::text, ec); }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      rj::Value channel(rj::kObjectType);
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("op", rj::Value("subscribe").Move(), allocator);
        std::string symbolId = subscriptionListBySymbolId.first;
        if (channelId == CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_TICKER) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = channelId + "|" + symbolId;
        std::string market = symbolId;
        std::string channelIdString = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        document.AddMember("channel", rj::Value(channelIdString.c_str(), allocator).Move(), allocator);
        document.AddMember("market", rj::Value(market.c_str(), allocator).Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
    return sendStringList;
  }

  std::string calculateOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk) override {
    auto i = 0;
    auto i1 = snapshotBid.rbegin();
    auto i2 = snapshotAsk.begin();
    std::vector<std::string> csData;
    while (i < 100 && (i1 != snapshotBid.rend() || i2 != snapshotAsk.end())) {
      if (i1 != snapshotBid.rend()) {
        csData.push_back(ConvertDecimalToString(i1->first));
        csData.push_back(i1->second);
        ++i1;
      }
      if (i2 != snapshotAsk.end()) {
        csData.push_back(ConvertDecimalToString(i2->first));
        csData.push_back(i2->second);
        ++i2;
      }
      ++i;
    }
    std::string csStr = UtilString::join(csData, ":");
    uint_fast32_t csCalc = UtilAlgorithm::crc(csStr.begin(), csStr.end());
    return intToHex(csCalc);
  }

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    auto type = std::string(document["type"].GetString());
    if (type == "update") {
      const rj::Value& data = document["data"];
      auto symbolId = std::string(document["market"].GetString());
      auto channel = std::string(document["channel"].GetString());
      auto exchangeSubscriptionId = channel + "|" + symbolId;
      if (channel == CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_TICKER) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        auto timePair = UtilTime::divide(data["time"].GetString());
        auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
        tp += std::chrono::nanoseconds(timePair.second);
        marketDataMessage.tp = tp;
        if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channel][symbolId]) {
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        } else {
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
        }
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(data["bid"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(data["bidSize"].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(data["ask"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(data["askSize"].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } else if (channel == CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_ORDERBOOKS) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        auto timePair = UtilTime::divide(data["time"].GetString());
        auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
        tp += std::chrono::nanoseconds(timePair.second);
        marketDataMessage.tp = tp;
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        const rj::Value& asks = data["asks"];
        const rj::Value& bids = data["bids"];
        for (auto& ask : asks.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          if (this->sessionOptions.enableCheckOrderBookChecksum) {
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, ask[0].GetString());
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, ask[1].GetString());
          } else {
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(ask[0].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(ask[1].GetString()));
          }
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        for (auto& bid : bids.GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          if (this->sessionOptions.enableCheckOrderBookChecksum) {
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, bid[0].GetString());
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, bid[1].GetString());
          } else {
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(bid[0].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(bid[1].GetString()));
          }
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
        if (this->sessionOptions.enableCheckOrderBookChecksum) {
          this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnectionPtr->id][symbolId] =
              intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoul(data["checksum"].GetString()))));
        }
      } else if (channel == CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_TRADES) {
        for (const auto& x : data.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.tp = UtilTime::parse(std::string(x["time"].GetString()), "%FT%T%Ez");
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["size"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["id"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "sell" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      }
    } else if (type == "partial") {
      const rj::Value& data = document["data"];
      auto symbolId = std::string(document["market"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_FTX_BASE_CHANNEL_ORDERBOOKS) + "|" + symbolId;
      if (this->sessionOptions.enableCheckOrderBookChecksum) {
        this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnectionPtr->id][symbolId] =
            intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoul(data["checksum"].GetString()))));
      }
      MarketDataMessage marketDataMessage;
      marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
      auto timePair = UtilTime::divide(std::string(data["time"].GetString()));
      auto tp = TimePoint(std::chrono::duration<int64_t>(timePair.first));
      tp += std::chrono::nanoseconds(timePair.second);
      marketDataMessage.tp = tp;
      const rj::Value& bids = data["bids"];
      for (auto& x : bids.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        if (this->sessionOptions.enableCheckOrderBookChecksum) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
        } else {
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
        }
        marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
      }
      const rj::Value& asks = data["asks"];
      for (auto& x : asks.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        if (this->sessionOptions.enableCheckOrderBookChecksum) {
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
        } else {
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
        }
        marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
      }
      marketDataMessageList.emplace_back(std::move(marketDataMessage));
    } else if (type == "subscribed") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
          this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        std::string channelId = document["channel"].GetString();
        std::string symbolId = document["market"].GetString();
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
      message.setCorrelationIdList(correlationIdList);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessageView);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
      event.setMessageList(messageList);
    } else if (type == "error") {
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
                                          {"{market_name}", Url::urlEncode(symbolId)},
                                      });
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit"},
                          });
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        this->substituteParam(target, {
                                          {"{market_name}", Url::urlEncode(symbolId)},
                                      });
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
    element.insert(CCAPI_INSTRUMENT, x["name"].GetString());
    if (!x["baseCurrency"].IsNull()) {
      element.insert(CCAPI_BASE_ASSET, x["baseCurrency"].GetString());
    }
    if (!x["quoteCurrency"].IsNull()) {
      element.insert(CCAPI_QUOTE_ASSET, x["quoteCurrency"].GetString());
    }
    if (!x["priceIncrement"].IsNull()) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["priceIncrement"].GetString());
    }
    if (!x["sizeIncrement"].IsNull()) {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["sizeIncrement"].GetString());
    }
    if (!x["underlying"].IsNull()) {
      element.insert(CCAPI_UNDERLYING_SYMBOL, x["underlying"].GetString());
    }
    if (!x["minProvideSize"].IsNull()) {
      element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minProvideSize"].GetString());
    }
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::parse(std::string(x["time"].GetString()), "%FT%T%Ez");
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["size"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["id"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "sell" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        const rj::Value& result = document["result"];
        Element element;
        this->extractInstrumentInfo(element, result);
        message.setElementList({element});
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        const rj::Value& result = document["result"];
        std::vector<Element> elementList;
        for (const auto& x : result.GetArray()) {
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_FTX_BASE_H_
