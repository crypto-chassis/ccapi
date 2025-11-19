#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_OKX

#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceOkx : public MarketDataService {
 public:
  MarketDataServiceOkx(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                       ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_OKX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + CCAPI_OKX_PUBLIC_WS_PATH;
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_OKX_API_KEY;
    this->apiSecretName = CCAPI_OKX_API_SECRET;
    this->apiPassphraseName = CCAPI_OKX_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->getRecentTradesTarget = "/api/v5/market/trades";
    this->getHistoricalTradesTarget = "/api/v5/market/history-trades";
    this->getRecentCandlesticksTarget = "/api/v5/market/candles";
    this->getHistoricalCandlesticksTarget = "/api/v5/market/history-candles";
    this->getMarketDepthTarget = "/api/v5/market/books";
    this->getRecentTradesTarget = "/api/v5/market/trades";
    this->getServerTimeTarget = "/api/v5/public/time";
    this->getInstrumentTarget = "/api/v5/public/instruments";
    this->getInstrumentsTarget = "/api/v5/public/instruments";
    this->getBbosTarget = "/api/v5/market/tickers";
    this->getTickersTarget = "/api/v5/market/tickers";
  }

  virtual ~MarketDataServiceOkx() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  std::string getInstrumentGroup(const Subscription& subscription) override {
    std::string baseUrlWsGivenSubscription(this->baseUrlWs);
    if (subscription.getField() == CCAPI_CANDLESTICK) {
      baseUrlWsGivenSubscription = this->sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + CCAPI_OKX_BUSINESS_WS_PATH;
    }
    return baseUrlWsGivenSubscription + "|" + subscription.getField() + "|" + subscription.getSerializedOptions() + "|" +
           subscription.getSerializedCredential() + "|" + subscription.getProxyUrl();
  }

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override {
    return !std::regex_search(bodyView.begin(), bodyView.end(), std::regex("\"code\":\\s*\"0\""));
  }

  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (conflateIntervalMilliseconds < 100) {
        if (marketDepthRequested == 1) {
          channelId = CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH1_L2_TBT;
        } else if (marketDepthRequested <= 50) {
          channelId = CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH50_L2_TBT;
        } else {
          channelId = CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH400_L2_TBT;
        }
      } else {
        if (marketDepthRequested <= 5) {
          channelId = CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH5;
        } else {
          channelId = CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH400;
        }
      }
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval =
          this->convertCandlestickIntervalSecondsToInterval(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)), "s", "m", "H", "D", "W");
      channelId = CCAPI_WEBSOCKET_OKX_CHANNEL_CANDLESTICK + interval;
    }
  }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        if (channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH1_L2_TBT || channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH5) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = UtilString::split(channelId, "?").at(0) + ":" + symbolId;
        rj::Value arg(rj::kObjectType);
        arg.AddMember("channel", rj::Value(channelId.c_str(), allocator).Move(), allocator);
        arg.AddMember("instId", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        args.PushBack(arg, allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }

  std::string calculateOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk) override {
    auto i = 0;
    auto i1 = snapshotBid.rbegin();
    auto i2 = snapshotAsk.begin();
    std::vector<std::string> csData;
    while (i < 25 && (i1 != snapshotBid.rend() || i2 != snapshotAsk.end())) {
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
    if (textMessageView != "pong") {
      this->jsonDocumentAllocator.Clear();
      rj::Document document(&this->jsonDocumentAllocator);
      document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
      auto it = document.FindMember("event");
      std::string eventStr = it != document.MemberEnd() ? it->value.GetString() : "";
      if (eventStr == "login") {
        this->startSubscribe(wsConnectionPtr);

      } else {
        if (document.IsObject() && document.HasMember("arg")) {
          const rj::Value& arg = document["arg"];
          std::string channelId = arg["channel"].GetString();
          std::string symbolId = arg["instId"].GetString();
          std::string exchangeSubscriptionId = channelId + ":" + symbolId;
          if (!eventStr.empty()) {
            if (eventStr == "subscribe") {
              event.setType(Event::Type::SUBSCRIPTION_STATUS);
              std::vector<Message> messageList;
              Message message;
              message.setTimeReceived(timeReceived);
              std::vector<std::string> correlationIdList;
              if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
                  this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
                const rj::Value& arg = document["arg"];
                std::string channelId = arg["channel"].GetString();
                std::string symbolId = arg["instId"].GetString();
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
            } else if (eventStr == "error") {
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
          } else {
            if (channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH1_L2_TBT || channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH5 ||
                channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH400 || channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH50_L2_TBT ||
                channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH400_L2_TBT) {
              std::string action = channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH1_L2_TBT || channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH5
                                       ? ""
                                       : document["action"].GetString();
              for (const auto& datum : document["data"].GetArray()) {
                if (this->sessionOptions.enableCheckOrderBookChecksum) {
                  auto it = datum.FindMember("checksum");
                  if (it != datum.MemberEnd()) {
                    this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnectionPtr->id][symbolId] =
                        intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoi(it->value.GetString()))));
                  }
                }
                MarketDataMessage marketDataMessage;
                marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
                marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
                marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
                if (channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH1_L2_TBT || channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_PUBLIC_DEPTH5) {
                  if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
                    marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
                  } else {
                    marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
                  }
                } else {
                  marketDataMessage.recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
                }
                for (const auto& x : datum["bids"].GetArray()) {
                  MarketDataMessage::TypeForDataPoint dataPoint;
                  dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
                  dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
                  marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
                }
                for (const auto& x : datum["asks"].GetArray()) {
                  MarketDataMessage::TypeForDataPoint dataPoint;
                  dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
                  dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
                  marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
                }
                marketDataMessageList.emplace_back(std::move(marketDataMessage));
              }
            } else if (channelId == CCAPI_WEBSOCKET_OKX_CHANNEL_TRADE) {
              for (const auto& datum : document["data"].GetArray()) {
                MarketDataMessage marketDataMessage;
                marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
                marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
                marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
                marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(datum["px"].GetString()));
                dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(datum["sz"].GetString()));
                dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, datum["tradeId"].GetString());
                dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(datum["side"].GetString()) == "sell" ? "1" : "0");
                marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
                marketDataMessageList.emplace_back(std::move(marketDataMessage));
              }
            } else if (channelId.rfind(CCAPI_WEBSOCKET_OKX_CHANNEL_CANDLESTICK, 0) == 0) {
              for (const auto& datum : document["data"].GetArray()) {
                MarketDataMessage marketDataMessage;
                marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
                marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
                marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum[0].GetString())));
                marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.emplace(MarketDataMessage::DataFieldType::OPEN_PRICE, datum[1].GetString());
                dataPoint.emplace(MarketDataMessage::DataFieldType::HIGH_PRICE, datum[2].GetString());
                dataPoint.emplace(MarketDataMessage::DataFieldType::LOW_PRICE, datum[3].GetString());
                dataPoint.emplace(MarketDataMessage::DataFieldType::CLOSE_PRICE, datum[4].GetString());
                dataPoint.emplace(MarketDataMessage::DataFieldType::VOLUME, datum[5].GetString());
                dataPoint.emplace(MarketDataMessage::DataFieldType::QUOTE_VOLUME, datum[7].GetString());
                marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
                marketDataMessageList.emplace_back(std::move(marketDataMessage));
              }
            }
          }
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
        this->appendSymbolId(queryString, symbolId, "instId");
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
                              {CCAPI_START_TRADE_ID, "before"},
                              {CCAPI_END_TRADE_ID, "after"},
                              {CCAPI_START_TIME_SECONDS, "before"},
                              {CCAPI_END_TIME_SECONDS, "after"},
                          },
                          {
                              {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceOkx>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                              {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceOkx>()](
                                                           const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                          });
        this->appendSymbolId(queryString, symbolId, "instId");
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
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS, "bar"},
                              {CCAPI_LIMIT, "limit"},
                              {CCAPI_START_TIME_SECONDS, "before"},
                              {CCAPI_END_TIME_SECONDS, "after"},
                          },
                          {
                              {CCAPI_CANDLESTICK_INTERVAL_SECONDS,
                               [that = shared_from_base<MarketDataServiceOkx>()](const std::string& input) {
                                 return that->convertCandlestickIntervalSecondsToInterval(std::stoi(input), "s", "m", "H", "D", "W");
                               }},
                              {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceOkx>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                              {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceOkx>()](
                                                           const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                          });
        this->appendSymbolId(queryString, symbolId, "instId");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_MARKET_DEPTH: {
        req.method(http::verb::get);
        auto target = this->getMarketDepthTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_MARKET_DEPTH_MAX, "sz"},
                          });
        this->appendSymbolId(queryString, symbolId, "instId");
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
                              {CCAPI_INSTRUMENT_TYPE, "instType"},
                          });
        this->appendSymbolId(queryString, symbolId, "instId");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "instType"},
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
                              {CCAPI_INSTRUMENT_TYPE, "instType"},
                          });
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_TICKERS: {
        req.method(http::verb::get);
        auto target = this->getBbosTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT_TYPE, "instType"},
                          });
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
    std::string instFamily = x["instFamily"].GetString();
    bool instFamilyHasDash = instFamily.find('-') != std::string::npos;
    std::string baseCcy = x["baseCcy"].GetString();
    element.insert(CCAPI_BASE_ASSET, baseCcy.empty() ? (instFamilyHasDash ? UtilString::split(instFamily, '-').at(0) : "") : baseCcy);
    std::string quoteCcy = x["quoteCcy"].GetString();
    element.insert(CCAPI_QUOTE_ASSET, quoteCcy.empty() ? (instFamilyHasDash ? UtilString::split(instFamily, '-').at(1) : "") : quoteCcy);
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["tickSz"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSz"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["minSz"].GetString());
    element.insert(CCAPI_SETTLE_ASSET, x["settleCcy"].GetString());
    element.insert(CCAPI_UNDERLYING_SYMBOL, x["uly"].GetString());
    element.insert(CCAPI_CONTRACT_SIZE, x["ctVal"].GetString());
    element.insert(CCAPI_CONTRACT_MULTIPLIER, x["ctMult"].GetString());
    element.insert(CCAPI_INSTRUMENT_STATUS, x["state"].GetString());
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        for (const auto& datum : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(datum["px"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(datum["sz"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, datum["tradeId"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(datum["side"].GetString()) == "sell" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        for (const auto& x : document["data"].GetArray()) {
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
        const rj::Value& data = document["data"][0];
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(data["ts"].GetString()));
        for (const auto& x : data["bids"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : data["asks"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x[0].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x[1].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } break;
      case Request::Operation::GET_SERVER_TIME: {
        Message message;
        message.setTime(UtilTime::makeTimePointMilli(UtilTime::divideMilli(document["data"][0]["ts"].GetString())));
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["data"].GetArray()) {
          if (std::string_view(x["instId"].GetString()) == request.getInstrument()) {
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
      case Request::Operation::GET_BBOS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
          element.insert(CCAPI_BEST_BID_N_PRICE, x["bidPx"].GetString());
          element.insert(CCAPI_BEST_BID_N_SIZE, x["bidSz"].GetString());
          element.insert(CCAPI_BEST_ASK_N_PRICE, x["askPx"].GetString());
          element.insert(CCAPI_BEST_ASK_N_SIZE, x["askSz"].GetString());
          elementList.push_back(element);
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_TICKERS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document["data"].GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["instId"].GetString());
          element.insert(CCAPI_BEST_BID_N_PRICE, x["bidPx"].GetString());
          element.insert(CCAPI_BEST_BID_N_SIZE, x["bidSz"].GetString());
          element.insert(CCAPI_BEST_ASK_N_PRICE, x["askPx"].GetString());
          element.insert(CCAPI_BEST_ASK_N_SIZE, x["askSz"].GetString());
          element.insert(CCAPI_LAST_PRICE, x["last"].GetString());
          element.insert(CCAPI_LAST_SIZE, x["lastSz"].GetString());
          element.insert(CCAPI_OPEN_24H_PRICE, x["open24h"].GetString());
          element.insert(CCAPI_HIGH_24H_PRICE, x["high24h"].GetString());
          element.insert(CCAPI_LOW_24H_PRICE, x["low24h"].GetString());
          element.insert(CCAPI_VOLUME_24H, x["vol24h"].GetString());
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

  std::vector<std::string> createSendStringListFromSubscriptionList(std::shared_ptr<WsConnection> wsConnectionPtr,
                                                                    const std::vector<Subscription>& subscriptionList, const TimePoint& now,
                                                                    const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("login").Move(), allocator);
    rj::Value arg(rj::kObjectType);
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    arg.AddMember("apiKey", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    arg.AddMember("passphrase", rj::Value(apiPassphrase.c_str(), allocator).Move(), allocator);
    arg.AddMember("timestamp", rj::Value(ts.c_str(), allocator).Move(), allocator);
    std::string signData = ts + "GET" + "/users/self/verify";
    std::string sign = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, signData));
    arg.AddMember("sign", rj::Value(sign.c_str(), allocator).Move(), allocator);
    rj::Value args(rj::kArrayType);
    args.PushBack(arg, allocator);
    document.AddMember("args", args, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKX_H_
