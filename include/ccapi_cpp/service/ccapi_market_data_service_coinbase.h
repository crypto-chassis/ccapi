#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_COINBASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_COINBASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceCoinbase : public MarketDataService {
 public:
  MarketDataServiceCoinbase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                            ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_COINBASE;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_COINBASE_API_KEY;
    this->apiSecretName = CCAPI_COINBASE_API_SECRET;
    this->apiPassphraseName = CCAPI_COINBASE_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    this->getRecentTradesTarget = "/products/<product-id>/trades";
    this->getInstrumentTarget = "/products/<product-id>";
    this->getInstrumentsTarget = "/products";
  }

  virtual ~MarketDataServiceCoinbase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("type", rj::Value("subscribe").Move(), allocator);
    rj::Value channels(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      rj::Value channel(rj::kObjectType);
      rj::Value symbolIds(rj::kArrayType);
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        symbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        std::string exchangeSubscriptionId = channelId + "|" + symbolId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
      channel.AddMember("name", rj::Value(channelId.c_str(), allocator).Move(), allocator);
      channel.AddMember("product_ids", symbolIds, allocator);
      channels.PushBack(channel, allocator);
      rj::Value heartbeatChannel(rj::kObjectType);
      heartbeatChannel.AddMember("name", rj::Value("heartbeat").Move(), allocator);
      rj::Value heartbeatSymbolIds(rj::kArrayType);
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        heartbeatSymbolIds.PushBack(rj::Value(subscriptionListBySymbolId.first.c_str(), allocator).Move(), allocator);
      }
      heartbeatChannel.AddMember("product_ids", heartbeatSymbolIds, allocator);
      channels.PushBack(heartbeatChannel, allocator);
    }
    document.AddMember("channels", channels, allocator);
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
    auto type = std::string(document["type"].GetString());
    if (type == "l2update") {
      auto symbolId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_COINBASE_CHANNEL_LEVEL2) + "|" + symbolId;
      MarketDataMessage marketDataMessage;
      marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      marketDataMessage.tp = UtilTime::parse(std::string(document["time"].GetString()));
      marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
      const rj::Value& changes = document["changes"];
      for (const auto& change : changes.GetArray()) {
        auto side = std::string(change[0].GetString());
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(change[1].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(change[2].GetString()));
        if (side == "buy") {
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        } else {
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
      }
      marketDataMessageList.emplace_back(std::move(marketDataMessage));
    } else if (type == "match") {
      auto symbolId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_COINBASE_CHANNEL_MATCH) + "|" + symbolId;
      MarketDataMessage marketDataMessage;
      marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      marketDataMessage.tp = UtilTime::parse(std::string(document["time"].GetString()));
      marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
      MarketDataMessage::TypeForDataPoint dataPoint;
      dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(document["price"].GetString()));
      dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(document["size"].GetString()));
      dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, document["trade_id"].GetString());
      dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(document["side"].GetString()) == "buy" ? "1" : "0");
      marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
      marketDataMessageList.emplace_back(std::move(marketDataMessage));
    } else if (type == "snapshot") {
      auto symbolId = std::string(document["product_id"].GetString());
      auto exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_COINBASE_CHANNEL_LEVEL2) + "|" + symbolId;
      MarketDataMessage marketDataMessage;
      marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
      marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
      marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
      marketDataMessage.tp = timeReceived;
      const rj::Value& bids = document["bids"];
      for (const auto& x : bids.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
        marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
      }
      const rj::Value& asks = document["asks"];
      for (const auto& x : asks.GetArray()) {
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x[0].GetString()));
        dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x[1].GetString()));
        marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
      }
      marketDataMessageList.emplace_back(std::move(marketDataMessage));
    } else if (type == "subscriptions") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
          this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        for (const auto& x : document["channels"].GetArray()) {
          std::string channelId = x["name"].GetString();
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).find(channelId) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).end()) {
            for (const auto& y : x["product_ids"].GetArray()) {
              std::string symbolId = y.GetString();
              if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).find(symbolId) !=
                  this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).end()) {
                std::vector<std::string> correlationIdList_2 =
                    this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
                correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
              }
            }
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
                                          {"<product-id>", symbolId},
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
                                          {"<product-id>", symbolId},
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
    element.insert(CCAPI_INSTRUMENT, x["id"].GetString());
    element.insert(CCAPI_INSTRUMENT_STATUS, x["status"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["base_currency"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote_currency"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["quote_increment"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["base_increment"].GetString());
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = UtilTime::parse(std::string(x["time"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["size"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["trade_id"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "buy" ? "1" : "0");
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

  std::vector<std::string> createSendStringListFromSubscriptionList(std::shared_ptr<WsConnection> wsConnectionPtr,
                                                                    const std::vector<Subscription>& subscriptionList, const TimePoint& now,
                                                                    const std::map<std::string, std::string>& credential) override {
    auto instrumentGroup = wsConnectionPtr->group;
    for (const auto& subscription : wsConnectionPtr->subscriptionList) {
      auto instrument = subscription.getInstrument();
      this->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
      if (subscription.getField() == CCAPI_GENERIC_PUBLIC_SUBSCRIPTION) {
        this->correlationIdByConnectionIdMap.insert({wsConnectionPtr->id, subscription.getCorrelationId()});
      } else {
        this->prepareSubscription(wsConnectionPtr, subscription);
      }
    }

    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    auto timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    auto preSignedText = timestamp;
    preSignedText += "GET";
    preSignedText += "/users/self/verify";
    auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), preSignedText));
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("type", rj::Value("subscribe").Move(), allocator);
    rj::Value channels(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      rj::Value channel(rj::kObjectType);
      rj::Value symbolIds(rj::kArrayType);
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        symbolIds.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        std::string exchangeSubscriptionId = channelId + "|" + symbolId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
      channel.AddMember("name", rj::Value(channelId.c_str(), allocator).Move(), allocator);
      channel.AddMember("product_ids", symbolIds, allocator);
      channels.PushBack(channel, allocator);
      rj::Value heartbeatChannel(rj::kObjectType);
      heartbeatChannel.AddMember("name", rj::Value("heartbeat").Move(), allocator);
      rj::Value heartbeatSymbolIds(rj::kArrayType);
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        heartbeatSymbolIds.PushBack(rj::Value(subscriptionListBySymbolId.first.c_str(), allocator).Move(), allocator);
      }
      heartbeatChannel.AddMember("product_ids", heartbeatSymbolIds, allocator);
      channels.PushBack(heartbeatChannel, allocator);
    }
    document.AddMember("channels", channels, allocator);
    document.AddMember("signature", rj::Value(signature.c_str(), allocator).Move(), allocator);
    document.AddMember("key", rj::Value(apiKey.c_str(), allocator).Move(), allocator);
    document.AddMember("passphrase", rj::Value(apiPassphrase.c_str(), allocator).Move(), allocator);
    document.AddMember("timestamp", rj::Value(timestamp.c_str(), allocator).Move(), allocator);
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_COINBASE_H_
