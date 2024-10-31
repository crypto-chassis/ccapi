#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_KUCOIN) || defined(CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceKucoinBase : public MarketDataService {
 public:
  MarketDataServiceKucoinBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                              ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~MarketDataServiceKucoinBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  bool doesHttpBodyContainError(const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"200000\"")); }
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (this->isDerivatives) {
        if (marketDepthRequested == 1) {
          channelId = this->channelMarketTicker;
        } else if (marketDepthRequested <= 5) {
          channelId = this->channelMarketLevel2Depth5;
        } else {
          channelId = this->channelMarketLevel2Depth50;
        }
      } else {
        // if (conflateIntervalMilliseconds < 100) {
        //     channelId = this->channelMarketLevel2;
        // }else{
        if (marketDepthRequested == 1) {
          channelId = this->channelMarketTicker;
        } else if (marketDepthRequested <= 5) {
          channelId = this->channelMarketLevel2Depth5;
        } else {
          channelId = this->channelMarketLevel2Depth50;
        }
        // }
      }
    } else if (field == CCAPI_CANDLESTICK) {
      std::string interval =
          this->convertCandlestickIntervalSecondsToInterval(std::stoi(optionMap.at(CCAPI_CANDLESTICK_INTERVAL_SECONDS)), "", "min", "hour", "day", "week");
      channelId += interval;
    }
  }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"id\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"type\":\"ping\"}", wspp::frame::opcode::text, ec);
  }
  void onOpen(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
  }
  void prepareConnect(WsConnection& wsConnection) override {
    http::request<http::string_body> req;
    req.set(http::field::host, this->hostRest);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(beast::http::field::content_type, "application/json");
    req.method(http::verb::post);
    req.target("/api/v1/bullet-public");
    this->sendRequest(
        req,
        [wsConnection, that = shared_from_base<MarketDataServiceKucoinBase>()](const beast::error_code& ec) {
          WsConnection thisWsConnection = wsConnection;
          that->onFail_(thisWsConnection);
        },
        [wsConnection, that = shared_from_base<MarketDataServiceKucoinBase>()](const http::response<http::string_body>& res) {
          WsConnection thisWsConnection = wsConnection;
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              const rj::Value& instanceServer = document["data"]["instanceServers"][0];
              urlWebsocketBase += std::string(instanceServer["endpoint"].GetString());
              urlWebsocketBase += "?token=";
              urlWebsocketBase += std::string(document["data"]["token"].GetString());
              thisWsConnection.url = urlWebsocketBase;
              that->connect(thisWsConnection);
              for (const auto& subscription : thisWsConnection.subscriptionList) {
                auto instrument = subscription.getInstrument();
                that->subscriptionStatusByInstrumentGroupInstrumentMap[thisWsConnection.group][instrument] = Subscription::Status::SUBSCRIBING;
              }
              that->extraPropertyByConnectionIdMap[thisWsConnection.id].insert({
                  {"pingInterval", std::string(instanceServer["pingInterval"].GetString())},
                  {"pingTimeout", std::string(instanceServer["pingInterval"].GetString())},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(thisWsConnection);
        },
        this->sessionOptions.httpRequestTimeoutMilliseconds);
  }
#else
  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(wsConnectionPtr, "{\"id\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"type\":\"ping\"}", ec);
  }
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override { wsConnectionPtr->status = WsConnection::Status::OPEN; }
  void prepareConnect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    http::request<http::string_body> req;
    req.set(http::field::host, this->hostRest);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(beast::http::field::content_type, "application/json");
    req.method(http::verb::post);
    req.target("/api/v1/bullet-public");
    this->sendRequest(
        req, [wsConnectionPtr, that = shared_from_base<MarketDataServiceKucoinBase>()](const beast::error_code& ec) { that->onFail_(wsConnectionPtr); },
        [wsConnectionPtr, that = shared_from_base<MarketDataServiceKucoinBase>()](const http::response<http::string_body>& res) {
          int statusCode = res.result_int();
          std::string body = res.body();
          if (statusCode / 100 == 2) {
            std::string urlWebsocketBase;
            try {
              rj::Document document;
              document.Parse<rj::kParseNumbersAsStringsFlag>(body.c_str());
              const rj::Value& instanceServer = document["data"]["instanceServers"][0];
              urlWebsocketBase += std::string(instanceServer["endpoint"].GetString());
              urlWebsocketBase += "?token=";
              urlWebsocketBase += std::string(document["data"]["token"].GetString());
              wsConnectionPtr->setUrl(urlWebsocketBase);
              that->connect(wsConnectionPtr);
              for (const auto& subscription : wsConnectionPtr->subscriptionList) {
                auto instrument = subscription.getInstrument();
                that->subscriptionStatusByInstrumentGroupInstrumentMap[wsConnectionPtr->group][instrument] = Subscription::Status::SUBSCRIBING;
              }
              that->extraPropertyByConnectionIdMap[wsConnectionPtr->id].insert({
                  {"pingInterval", std::string(instanceServer["pingInterval"].GetString())},
                  {"pingTimeout", std::string(instanceServer["pingInterval"].GetString())},
              });
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->onFail_(wsConnectionPtr);
        },
        this->sessionOptions.httpRequestTimeoutMilliseconds);
  }
#endif
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    std::map<std::string, std::vector<std::string>> symbolListByTopicMap;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        auto exchangeSubscriptionId = channelId + ":" + symbolId;
        if (channelId == this->channelMarketTicker || channelId == this->channelMarketLevel2Depth5 || channelId == this->channelMarketLevel2Depth50) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        if (!this->channelMarketKlines.empty() && channelId.rfind(this->channelMarketKlines, 0) == 0) {
          exchangeSubscriptionId = this->channelMarketKlines + ":" + symbolId + "_" + channelId.substr(this->channelMarketKlines.length());
        }
        symbolListByTopicMap[channelId].push_back(symbolId);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    for (const auto& x : symbolListByTopicMap) {
      auto topic = x.first;
      auto symbolList = x.second;
      if (!this->channelMarketKlines.empty() && topic.rfind(this->channelMarketKlines, 0) == 0) {
        for (const auto& symbol : symbolList) {
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("type", rj::Value("subscribe").Move(), allocator);
          document.AddMember("privateChannel", false, allocator);
          document.AddMember("response", true, allocator);
          std::string exchangeSubscriptionId =
              std::string(this->channelMarketKlines) + ":" + symbol + "_" + topic.substr(std::string(this->channelMarketKlines).length());
          document.AddMember("topic", rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
          int nextRequestId = this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id];
          std::string requestId = std::to_string(nextRequestId);
          std::vector<std::string> exchangeSubscriptionIdList;
          exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
          this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id]
                                                                                [this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
              exchangeSubscriptionIdList;
          this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
          document.AddMember("id", rj::Value(requestId.c_str(), allocator).Move(), allocator);
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          sendStringList.push_back(sendString);
        }
      } else {
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("type", rj::Value("subscribe").Move(), allocator);
        document.AddMember("privateChannel", false, allocator);
        document.AddMember("response", true, allocator);
        document.AddMember("topic", rj::Value((topic + ":" + UtilString::join(symbolList, ",")).c_str(), allocator).Move(), allocator);
        int nextRequestId = this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id];
        std::string requestId = std::to_string(nextRequestId);
        std::vector<std::string> exchangeSubscriptionIdList;
        for (const auto& symbol : symbolList) {
          exchangeSubscriptionIdList.push_back(topic + ":" + symbol);
        }
        this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap[wsConnection.id][this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
            exchangeSubscriptionIdList;
        this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
        document.AddMember("id", rj::Value(requestId.c_str(), allocator).Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      }
    }
    return sendStringList;
  }
  void createFetchOrderBookInitialReq(http::request<http::string_body>& req, const std::string& symbolId, const TimePoint& now,
                                      const std::map<std::string, std::string>& credential) override {
    req.set(http::field::host, this->hostRest);
    req.method(http::verb::get);
    req.target("/api/v3/market/orderbook/level2?symbol=" + Url::urlEncode(symbolId));
    this->prepareReq(req, now, credential);
    this->signRequest(req, "", credential);
  }
  void prepareReq(http::request<http::string_body>& req, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    req.set(beast::http::field::content_type, "application/json");
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    req.set("KC-API-KEY", apiKey);
    req.set("KC-API-TIMESTAMP", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    req.set("KC-API-KEY-VERSION", "2");
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    this->signApiPassphrase(req, apiPassphrase, apiSecret);
  }
  void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto preSignedText = req.base().at("KC-API-TIMESTAMP").to_string();
    preSignedText += std::string(req.method_string());
    preSignedText += req.target().to_string();
    preSignedText += body;
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
    req.set("KC-API-SIGN", signature);
    req.body() = body;
    req.prepare_payload();
  }
  void signApiPassphrase(http::request<http::string_body>& req, const std::string& apiPassphrase, const std::string& apiSecret) {
    req.set("KC-API-PASSPHRASE", UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, apiPassphrase)));
  }
  void extractOrderBookInitialVersionId(int64_t& versionId, const rj::Document& document) override {
    versionId = std::stoll(document["data"]["sequence"].GetString());
  }
  void extractOrderBookInitialData(MarketDataMessage::TypeForData& input, const rj::Document& document) override {
    for (const auto& x : document["data"]["bids"].GetArray()) {
      MarketDataMessage::TypeForDataPoint dataPoint;
      dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
      dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
      input[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
    }
    for (const auto& x : document["data"]["asks"].GetArray()) {
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
    if (document.IsObject()) {
      auto it = document.FindMember("type");
      if (it != document.MemberEnd()) {
        std::string type = it->value.GetString();
        if (type == "message") {
          std::string subject = document["subject"].GetString();
          if (subject == "trade.l2update") {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            std::string exchangeSubscriptionId = document["topic"].GetString();
            std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
            std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
            const auto& optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            const auto& data = document["data"];
            marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(data["time"].GetString()));
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            const auto& changes = data["changes"];
            const auto& itAsks = changes.FindMember("asks");
            if (itAsks != changes.MemberEnd()) {
              const rj::Value& asks = itAsks->value;
              for (auto& x : asks.GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
                marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
              }
            }
            const auto& itBids = data.FindMember("changes");
            if (itBids != changes.MemberEnd()) {
              const rj::Value& bids = itBids->value;
              for (auto& x : bids.GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
                marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
              }
            }
            int64_t versionId = std::stoll(data["sequenceEnd"].GetString());
            this->processOrderBookWithVersionId(versionId, wsConnection, channelId, symbolId, exchangeSubscriptionId, optionMap, marketDataMessageList,
                                                marketDataMessage);
          } else if (subject == this->tickerSubject) {
            MarketDataMessage marketDataMessage;
            std::string exchangeSubscriptionId = document["topic"].GetString();
            std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
            std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
            const rj::Value& data = document["data"];
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                              ? MarketDataMessage::RecapType::NONE
                                              : MarketDataMessage::RecapType::SOLICITED;
            marketDataMessage.tp = this->isDerivatives ? UtilTime::makeTimePoint(UtilTime::divideNanoWhole(data["ts"].GetString()))
                                                       : TimePoint(std::chrono::milliseconds(std::stoll(data["time"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert(
                  {MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data[this->tickerBestBidPriceKey.c_str()].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["bestBidSize"].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            }
            {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert(
                  {MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data[this->tickerBestAskPriceKey.c_str()].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["bestAskSize"].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          } else if (subject == this->level2Subject) {
            MarketDataMessage marketDataMessage;
            std::string exchangeSubscriptionId = document["topic"].GetString();
            std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
            std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
            auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
            const rj::Value& data = document["data"];
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                              ? MarketDataMessage::RecapType::NONE
                                              : MarketDataMessage::RecapType::SOLICITED;
            // kucoin futures documentation is incorrect: https://docs.kucoin.com/futures/#message-channel-for-the-5-best-ask-bid-full-data-of-level-2
            // "ts" is actually milliseconds
            marketDataMessage.tp = this->isDerivatives ? TimePoint(std::chrono::milliseconds(std::stoll(data["ts"].GetString())))
                                                       : TimePoint(std::chrono::milliseconds(std::stoll(data["timestamp"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
            const std::map<MarketDataMessage::DataType, const char*> bidAsk{{MarketDataMessage::DataType::BID, "bids"},
                                                                            {MarketDataMessage::DataType::ASK, "asks"}};
            for (const auto& x : bidAsk) {
              int index = 0;
              for (const auto& y : data[x.second].GetArray()) {
                if (index >= maxMarketDepth) {
                  break;
                }
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(y[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(y[1].GetString())});
                marketDataMessage.data[x.first].emplace_back(std::move(dataPoint));
                ++index;
              }
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          } else if (subject == this->matchSubject) {
            const rj::Value& data = document["data"];
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            std::string exchangeSubscriptionId = document["topic"].GetString();
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divideNanoWhole(data[this->recentTradesTimeKey.c_str()].GetString()));
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(data["price"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(data["size"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(data["tradeId"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SEQUENCE_NUMBER, std::string(data["sequence"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(data["side"].GetString()) == "sell" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          } else if (subject == this->klineSubject) {
            const rj::Value& x = document["data"]["candles"].GetArray();
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
            std::string exchangeSubscriptionId = document["topic"].GetString();
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.tp = UtilTime::makeTimePoint(std::make_pair(std::stoll(x[0].GetString()), 0));
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, x[1].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, x[3].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, x[4].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, x[2].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, x[5].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::QUOTE_VOLUME, x[6].GetString()});
            marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (type == "welcome") {
          this->pingIntervalMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] =
              std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingInterval"));
          this->pongTimeoutMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] =
              std::stol(this->extraPropertyByConnectionIdMap.at(wsConnection.id).at("pingTimeout"));
          if (this->pingIntervalMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] <=
              this->pongTimeoutMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL]) {
            this->pongTimeoutMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] =
                this->pingIntervalMillisecondsByMethodMap[PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] - 1;
          }
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          Service::onOpen(hdl);
          WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
          this->startSubscribe(wsConnection);
#else
          Service::onOpen(wsConnectionPtr);
          this->startSubscribe(wsConnectionPtr);
#endif
        } else if (type == "pong") {
          auto now = UtilTime::now();
          this->lastPongTpByMethodByConnectionIdMap[wsConnection.id][PingPongMethod::WEBSOCKET_APPLICATION_LEVEL] = now;
        } else if (type == "ack") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          std::vector<std::string> correlationIdList;
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
            int id = std::stoi(document["id"].GetString());
            if (this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.find(wsConnection.id) !=
                    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.end() &&
                this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).find(id) !=
                    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).end()) {
              auto exchangeSubscriptionIdList = this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.at(wsConnection.id).at(id);
              for (const auto& exchangeSubscriptionId : exchangeSubscriptionIdList) {
                auto splitted = UtilString::split(exchangeSubscriptionId, ":");
                auto channelId = splitted.at(0);
                auto symbolId = splitted.at(1);
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
          message.setType(Message::Type::SUBSCRIPTION_STARTED);
          Element element;
          element.insert(CCAPI_INFO_MESSAGE, textMessage);
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
          element.insert(CCAPI_ERROR_MESSAGE, textMessage);
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
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        req.method(http::verb::get);
        auto target = this->getRecentTradesTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        req.method(http::verb::get);
        auto target = this->getRecentCandlesticksTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        if (this->isDerivatives) {
          this->appendParam(queryString, param,
                            {
                                {CCAPI_CANDLESTICK_INTERVAL_SECONDS, "granularity"},
                                {CCAPI_START_TIME_SECONDS, "from"},
                                {CCAPI_END_TIME_SECONDS, "to"},
                            },
                            {
                                {CCAPI_CANDLESTICK_INTERVAL_SECONDS, [that = shared_from_base<MarketDataServiceKucoinBase>()](
                                                                         const std::string& input) { return std::to_string(std::stoi(input) / 60); }},
                                {CCAPI_START_TIME_SECONDS, [that = shared_from_base<MarketDataServiceKucoinBase>()](
                                                               const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                                {CCAPI_END_TIME_SECONDS, [that = shared_from_base<MarketDataServiceKucoinBase>()](
                                                             const std::string& input) { return that->convertParamTimeSecondsToTimeMilliseconds(input); }},
                            });
        } else {
          this->appendParam(queryString, param,
                            {
                                {CCAPI_CANDLESTICK_INTERVAL_SECONDS, "type"},
                                {CCAPI_START_TIME_SECONDS, "startAt"},
                                {CCAPI_END_TIME_SECONDS, "endAt"},
                            },
                            {
                                {CCAPI_CANDLESTICK_INTERVAL_SECONDS,
                                 [that = shared_from_base<MarketDataServiceKucoinBase>()](const std::string& input) {
                                   return that->convertCandlestickIntervalSecondsToInterval(std::stoi(input), "", "min", "hour", "day", "week");
                                 }},
                            });
        }
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_MARKET_DEPTH: {
        req.method(http::verb::get);
        auto target = this->getMarketDepthTarget;
        ;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendSymbolId(queryString, symbolId, "symbol");
        std::string maxMarketDepthStr = mapGetWithDefault(param, std::string(CCAPI_MARKET_DEPTH_MAX));
        if (maxMarketDepthStr.empty()) {
          if (this->isDerivatives) {
            std::string toReplace = "depth";
            target.replace(target.find(toReplace), toReplace.length(), "snapshot");
          }
        } else {
          target = this->getMarketDepthTarget + (this->isDerivatives ? "" : "_") +
                   std::to_string(this->calculateMarketDepthAllowedByExchange(std::stoi(maxMarketDepthStr), std::vector<int>({20, 100})));
        }
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
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES:
      case Request::Operation::GET_HISTORICAL_TRADES: {
        for (const auto& x : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::makeTimePoint(UtilTime::divideNanoWhole(x[this->recentTradesTimeKey.c_str()].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["size"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SEQUENCE_NUMBER, std::string(x["sequence"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["side"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_RECENT_CANDLESTICKS:
      case Request::Operation::GET_HISTORICAL_CANDLESTICKS: {
        for (const auto& x : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
          marketDataMessage.tp = this->isDerivatives ? TimePoint(std::chrono::milliseconds(std::stoll(x[0].GetString())))
                                                     : TimePoint(std::chrono::seconds(std::stoll(x[0].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          if (this->isDerivatives) {
            dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, std::string(x[1].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, std::string(x[2].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, std::string(x[3].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, std::string(x[4].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, std::string(x[5].GetString())});
          } else {
            dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, std::string(x[1].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, std::string(x[3].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, std::string(x[4].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, std::string(x[2].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, std::string(x[5].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::QUOTE_VOLUME, std::string(x[6].GetString())});
          }
          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_MARKET_DEPTH: {
        const rj::Value& data = document["data"];
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.tp = this->isDerivatives ? TimePoint(std::chrono::nanoseconds(std::stoll(data["ts"].GetString())))
                                                   : TimePoint(std::chrono::milliseconds(std::stoll(data["time"].GetString())));
        for (const auto& x : data["bids"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, x[0].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, x[1].GetString()});
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : data["asks"].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, x[0].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, x[1].GetString()});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
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
  virtual void extractInstrumentInfo(Element& element, const rj::Value& x) {}
  bool isDerivatives{};
  std::string channelMarketTicker;
  std::string channelMarketLevel2Depth5;
  std::string channelMarketLevel2Depth50;
  std::string channelMarketLevel2;
  std::string channelMarketKlines;
  std::string tickerSubject;
  std::string tickerBestBidPriceKey;
  std::string tickerBestAskPriceKey;
  std::string matchSubject;
  std::string klineSubject;
  std::string recentTradesTimeKey;
  std::string level2Subject;
  std::string apiPassphraseName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_KUCOIN_BASE_H_
