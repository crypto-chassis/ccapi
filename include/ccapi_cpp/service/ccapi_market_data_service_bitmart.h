#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITMART_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITMART_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITMART
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceBitmart : public MarketDataService {
 public:
  MarketDataServiceBitmart(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                           ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITMART;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/api?protocol=1.1";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_BITMART_API_KEY;
    this->apiSecretName = CCAPI_BITMART_API_SECRET;
    this->apiMemo = CCAPI_BITMART_API_MEMO;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiMemo});
    this->getRecentTradesTarget = "/spot/v1/symbols/trades";
    this->getInstrumentsTarget = "/spot/v1/symbols/details";
  }

  virtual ~MarketDataServiceBitmart() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested <= 5) {
        channelId = CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH5;
      } else if (marketDepthRequested <= 20) {
        channelId = CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH20;
      } else {
        channelId = CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH50;
      }
    }
  }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }

  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    this->subscriptionStartedByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    MarketDataService::onClose(wsConnectionPtr, ec);
  }

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override {
    return !std::regex_search(bodyView.begin(), bodyView.end(), std::regex("\"code\":\\s*1000"));
  }

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
        if (channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH5 || channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH20 ||
            channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH50) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = UtilString::split(channelId, "?").at(0) + ":" + symbolId;
        args.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
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

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    if (textMessageView != "pong") {
      this->jsonDocumentAllocator.Clear();
      rj::Document document(&this->jsonDocumentAllocator);
      document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
      auto it = document.FindMember("errorCode");
      std::string errorCode = it != document.MemberEnd() ? it->value.GetString() : "";
      if (errorCode.empty()) {
        std::string channelId = document["table"].GetString();
        std::string symbolId = document["data"][0]["symbol"].GetString();
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;
        if (!this->subscriptionStartedByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
          const auto& subscriptionList = this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          std::vector<std::string> correlationIdList;
          for (const auto& subscription : subscriptionList) {
            correlationIdList.push_back(subscription.getCorrelationId());
          }
          Event event;
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList(correlationIdList);
          message.setType(Message::Type::SUBSCRIPTION_STARTED);
          Element element;
          element.insert(CCAPI_INFO_MESSAGE, textMessageView);
          message.setElementList({element});
          messageList.emplace_back(std::move(message));
          event.addMessages(messageList);
          this->eventHandler(event, nullptr);
        }
        if (channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH5 || channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH20 ||
            channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_PUBLIC_DEPTH50) {
          for (const auto& datum : document["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ms_t"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
              marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            } else {
              marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
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
        } else if (channelId == CCAPI_WEBSOCKET_BITMART_CHANNEL_TRADE) {
          MarketDataMessage::RecapType recapType = MarketDataMessage::RecapType::NONE;
          if (!this->subscriptionStartedByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
            recapType = MarketDataMessage::RecapType::SOLICITED;
          }
          for (const auto& datum : document["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            marketDataMessage.recapType = recapType;
            marketDataMessage.tp = TimePoint(std::chrono::seconds(std::stoll(datum["s_t"].GetString())));
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(datum["price"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(datum["size"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(datum["side"].GetString()) == "buy" ? "1" : "0");
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        }
        if (!this->subscriptionStartedByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
          this->subscriptionStartedByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
      } else {
        std::string eventStr = document["event"].GetString();
        const auto& splitted = UtilString::split(eventStr, ':');
        if (splitted.at(0) == "subscribe") {
          event.setType(Event::Type::SUBSCRIPTION_STATUS);
          std::vector<Message> messageList;
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::SUBSCRIPTION_FAILURE);
          Element element;
          element.insert(CCAPI_ERROR_MESSAGE, textMessageView);
          message.setElementList({element});
          const auto& channelId = splitted.at(1);
          const auto& symbolId = splitted.at(2);
          const auto& subscriptionList = this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          std::vector<std::string> correlationIdList;
          for (const auto& subscription : subscriptionList) {
            correlationIdList.push_back(subscription.getCorrelationId());
          }
          message.setCorrelationIdList(correlationIdList);
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
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "N"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
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
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["base_currency"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote_currency"].GetString());
    int priceMaxPrecision = std::stoi(x["price_max_precision"].GetString());
    if (priceMaxPrecision > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(priceMaxPrecision - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1" + std::string(-priceMaxPrecision, '0'));
    }
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, UtilString::normalizeDecimalStringView(x["quote_increment"].GetString()));
    element.insert(CCAPI_ORDER_QUANTITY_MIN, UtilString::normalizeDecimalStringView(x["base_min_size"].GetString()));
    element.insert(CCAPI_ORDER_QUOTE_QUANTITY_MIN,
                   ConvertDecimalToString(std::max(Decimal(x["min_buy_amount"].GetString()), Decimal(x["min_sell_amount"].GetString()))));
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& datum : document["data"]["trades"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["order_time"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(datum["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(datum["count"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(datum["type"].GetString()) == "buy" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["data"]["symbols"].GetArray()) {
          if (std::string_view(x["symbol"].GetString()) == request.getInstrument()) {
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
        for (const auto& x : document["data"]["symbols"].GetArray()) {
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
    auto apiPassphrase = mapGetWithDefault(credential, this->apiMemo);
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

  std::string apiMemo;
  std::map<std::string, std::map<std::string, std::map<std::string, bool>>> subscriptionStartedByConnectionIdChannelIdSymbolIdMap;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITMART_H_
