#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_CRYPTOCOM_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_CRYPTOCOM_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceCryptocom : public MarketDataService {
 public:
  MarketDataServiceCryptocom(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                             ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_CRYPTOCOM;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/exchange/v1/market";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->hostHttpHeaderValueIgnorePort = true;
    this->shouldAlignSnapshot = true;
    this->getRecentTradesTarget = "/exchange/v1/public/get-trades";
    this->getInstrumentTarget = "/exchange/v1/public/get-instruments";
    this->getInstrumentsTarget = "/exchange/v1/public/get-instruments";
  }

  virtual ~MarketDataServiceCryptocom() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      int marketDepthSubscribedToExchange = 1;
      marketDepthSubscribedToExchange = this->calculateMarketDepthAllowedByExchange(marketDepthRequested, std::vector<int>({10, 50}));
      channelId = CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_BOOK;
      this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = marketDepthSubscribedToExchange;
    }
  }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    auto now = UtilTime::now();
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    document.AddMember("id", rj::Value(requestId).Move(), allocator);
    document.AddMember("method", rj::Value("subscribe").Move(), allocator);
    rj::Value channels(rj::kArrayType);
    std::string bookSubscriptionType;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        std::string exchangeSubscriptionId(channelId);
        std::map<std::string, std::string> replaceMap;
        if (channelId == CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_BOOK) {
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          replaceMap = {
              {"{instrument_name}", symbolId},
              {"{depth}", std::to_string(marketDepthSubscribedToExchange)},
          };
          bookSubscriptionType = "SNAPSHOT_AND_UPDATE";
        } else if (channelId == CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_TRADE) {
          replaceMap = {
              {"{instrument_name}", symbolId},
          };
        }
        for (const auto& x : replaceMap) {
          auto toReplace = x.first;
          auto replacement = x.second;
          if (exchangeSubscriptionId.find(toReplace) != std::string::npos) {
            exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), replacement);
          }
        }
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        channels.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
      }
    }
    rj::Value params(rj::kObjectType);
    params.AddMember("channels", channels, allocator);
    if (!bookSubscriptionType.empty()) {
      params.AddMember("book_subscription_type", rj::Value(bookSubscriptionType.c_str(), allocator).Move(), allocator);
    }
    document.AddMember("params", params, allocator);
    document.AddMember("nonce", rj::Value(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
    return sendStringList;
  }

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    std::string id = document["id"].GetString();
    if (id == "-1") {
      std::string method = document["method"].GetString();
      const rj::Value& result = document["result"];
      if (method == "subscribe") {
        std::string exchangeSubscriptionId = result["subscription"].GetString();
        const std::string& channelId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        const std::string& symbolId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
        if (channelId == CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_BOOK) {
          bool isUpdate = std::string_view(result["channel"].GetString()) == "book.update";
          for (const auto& datum : result["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["tt"].GetString())));
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            if (channelId == CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_BOOK) {
              if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
                marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
              } else {
                marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
              }
              for (const auto& x : isUpdate ? datum["update"]["bids"].GetArray() : datum["bids"].GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
                dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
                marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
              }
              for (const auto& x : isUpdate ? datum["update"]["asks"].GetArray() : datum["asks"].GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
                dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
                marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
              }
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (channelId == CCAPI_WEBSOCKET_CRYPTOCOM_CHANNEL_TRADE) {
          for (const auto& x : result["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["t"].GetString())));
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["p"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["q"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["d"].GetString());
            dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["s"].GetString()) == "SELL" ? "1" : "0");
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        }
      }
    } else {
      std::string method = document["method"].GetString();
      if (method == "subscribe") {
        if (std::string(document["code"].GetString()) != "0") {
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
        } else {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          std::vector<std::string> correlationIdList;
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
            for (const auto& subscription : wsConnectionPtr->subscriptionList) {
              correlationIdList.push_back(subscription.getCorrelationId());
            }
          }
          message.setCorrelationIdList(correlationIdList);
          message.setType(Message::Type::SUBSCRIPTION_STARTED);
          Element element;
          element.insert(CCAPI_INFO_MESSAGE, textMessageView);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          event.setMessageList(messageList);
        }
      } else if (method == "public/heartbeat") {
        std::string msg = R"({"id":)" + id + R"(,"method":"public/respond-heartbeat"})";
        ErrorCode ec;

        this->send(wsConnectionPtr, msg, ec);

        if (ec) {
          this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
        }
      }
    }
  }

  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, int64_t requestId, const std::string& method,
                   const std::map<std::string, std::string>& param, const std::map<std::string, std::string> standardizationMap = {}) {
    document.AddMember("id", rj::Value(requestId).Move(), allocator);
    document.AddMember("method", rj::Value(method.c_str(), allocator).Move(), allocator);
    rj::Value params(rj::kObjectType);
    for (const auto& kv : param) {
      auto key = standardizationMap.find(kv.first) != standardizationMap.end() ? standardizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      if (value != "null") {
        if (value == "true" || value == "false") {
          params.AddMember(rj::Value(key.c_str(), allocator).Move(), value == "true", allocator);
        } else {
          params.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
    }
    document.AddMember("params", params, allocator);
  }

  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> standardizationMap = {},
                   const std::map<std::string_view, std::function<std::string(const std::string&)>> conversionMap = {}) {
    MarketDataService::appendParam(queryString, param, standardizationMap, conversionMap);
  }

  void prepareReq(http::request<http::string_body>& req) {
    req.set(beast::http::field::content_type, "application/json");
    req.method(http::verb::get);
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    this->prepareReq(req);
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
                              {CCAPI_LIMIT, "count"},
                          });
        this->appendSymbolId(queryString, symbolId, "instrument_name");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.target(this->getInstrumentTarget);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.target(this->getInstrumentsTarget);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["base_ccy"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote_ccy"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["price_tick_size"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["qty_tick_size"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_MIN, x["qty_tick_size"].GetString());
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"]["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["t"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["p"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["q"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["d"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["s"].GetString()) == "SELL" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        const rj::Value& instruments = document["result"]["data"];
        std::vector<Element> elementList;
        for (const auto& x : instruments.GetArray()) {
          if (std::string_view(x["symbol"].GetString()) == request.getInstrument()) {
            Element element;
            this->extractInstrumentInfo(element, x);
            elementList.push_back(element);
            break;
          }
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        const rj::Value& instruments = document["result"]["data"];
        std::vector<Element> elementList;
        for (const auto& x : instruments.GetArray()) {
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

  void subscribeToExchange(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::seconds(1)));
    timerPtr->async_wait([wsConnectionPtr, that = shared_from_base<MarketDataServiceCryptocom>()](ErrorCode const& ec) {
      if (ec) {
        return;
      }
      that->MarketDataService::subscribeToExchange(wsConnectionPtr);
    });
    this->firstSubscribeDelayTimerMapByConnectionIdMap[wsConnectionPtr->id] = timerPtr;
  }

  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    if (this->firstSubscribeDelayTimerMapByConnectionIdMap.find(wsConnectionPtr->id) != this->firstSubscribeDelayTimerMapByConnectionIdMap.end()) {
      this->firstSubscribeDelayTimerMapByConnectionIdMap.at(wsConnectionPtr->id)->cancel();
      this->firstSubscribeDelayTimerMapByConnectionIdMap.erase(wsConnectionPtr->id);
    }
    MarketDataService::onClose(wsConnectionPtr, ec);
  }

  std::map<std::string, TimerPtr> firstSubscribeDelayTimerMapByConnectionIdMap;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_CRYPTOCOM_H_
