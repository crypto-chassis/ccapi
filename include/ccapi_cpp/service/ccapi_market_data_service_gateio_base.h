#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_GATEIO) || defined(CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGateioBase : public MarketDataService {
 public:
  MarketDataServiceGateioBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                              std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~MarketDataServiceGateioBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override {
    auto now = UtilTime::now();
    this->send(hdl, "{\"time\":\"" + std::to_string(UtilTime::getUnixTimestamp(now)) + "\",\"channel\":\"spot.ping\"}", wspp::frame::opcode::text, ec);
  }
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = CCAPI_WEBSOCKET_GATEIO_CHANNEL_BOOK_TICKER;
      } else {
        int marketDepthSubscribedToExchange = 1;
        marketDepthSubscribedToExchange = this->calculateMarketDepthSubscribedToExchange(marketDepthRequested, std::vector<int>({5, 10, 20}));
        std::string updateSpeed;
        if (conflateIntervalMilliSeconds < 1000) {
          updateSpeed = "100ms";
        } else {
          updateSpeed = "1000ms";
        }
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
        if (!updateSpeed.empty()) {
          channelId += "&UPDATE_SPEED=" + updateSpeed;
        }
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
      }
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    auto now = UtilTime::now();
    std::vector<std::string> sendStringList;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      if (channelId == CCAPI_WEBSOCKET_GATEIO_CHANNEL_BOOK_TICKER || channelId == CCAPI_WEBSOCKET_GATEIO_CHANNEL_TRADES) {
        std::vector<std::string> exchangeSubscriptionIdList;
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("event", rj::Value("subscribe").Move(), allocator);
        document.AddMember("time", rj::Value(UtilTime::getUnixTimestamp(now)).Move(), allocator);
        document.AddMember("channel", rj::Value(channelId.c_str(), allocator).Move(), allocator);
        rj::Value payload(rj::kArrayType);
        for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListByInstrument.first;
          if (channelId == CCAPI_WEBSOCKET_GATEIO_CHANNEL_BOOK_TICKER) {
            this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          }
          payload.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          std::string exchangeSubscriptionId = channelId + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
        }
        document.AddMember("payload", payload, allocator);
        document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
        this->exchangeSubscriptionIdListByExchangeJsonPayloadIdMap[this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] = exchangeSubscriptionIdList;
        this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string sendString = stringBuffer.GetString();
        sendStringList.push_back(sendString);
      } else if (channelId.rfind(CCAPI_WEBSOCKET_GATEIO_CHANNEL_ORDER_BOOK, 0) == 0) {
        for (const auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
          auto symbolId = subscriptionListByInstrument.first;
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          document.AddMember("event", rj::Value("subscribe").Move(), allocator);
          document.AddMember("time", rj::Value(UtilTime::getUnixTimestamp(now)).Move(), allocator);
          document.AddMember("channel", rj::Value(CCAPI_WEBSOCKET_GATEIO_CHANNEL_ORDER_BOOK).Move(), allocator);
          rj::Value payload(rj::kArrayType);
          payload.PushBack(rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          payload.PushBack(rj::Value(std::to_string(marketDepthSubscribedToExchange).c_str(), allocator).Move(), allocator);
          auto splitted = UtilString::split(channelId, "?");
          if (splitted.size() == 2) {
            auto mapped = Url::convertQueryStringToMap(splitted.at(1));
            if (mapped.find("UPDATE_SPEED") != mapped.end()) {
              payload.PushBack(rj::Value(mapped.at("UPDATE_SPEED").c_str(), allocator).Move(), allocator);
            }
          }
          document.AddMember("payload", payload, allocator);
          std::string exchangeSubscriptionId = std::string(CCAPI_WEBSOCKET_GATEIO_CHANNEL_ORDER_BOOK) + "|" + symbolId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
          std::vector<std::string> exchangeSubscriptionIdList;
          exchangeSubscriptionIdList.push_back(exchangeSubscriptionId);
          document.AddMember("id", rj::Value(this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]).Move(), allocator);
          this->exchangeSubscriptionIdListByExchangeJsonPayloadIdMap[this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id]] =
              exchangeSubscriptionIdList;
          this->exchangeJsonPayloadIdByConnectionIdMap[wsConnection.id] += 1;
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          sendStringList.push_back(sendString);
        }
      }
    }
    return sendStringList;
  }
  void processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    if (document.HasMember("id") && !document["id"].IsNull()) {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      std::vector<Message> messageList;
      Message message;
      message.setTimeReceived(timeReceived);
      std::vector<std::string> correlationIdList;
      if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) != this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
        int id = std::stoi(document["id"].GetString());
        if (this->exchangeSubscriptionIdListByExchangeJsonPayloadIdMap.find(id) != this->exchangeSubscriptionIdListByExchangeJsonPayloadIdMap.end()) {
          for (const auto& exchangeSubscriptionId : this->exchangeSubscriptionIdListByExchangeJsonPayloadIdMap.at(id)) {
            auto splitted = UtilString::split(exchangeSubscriptionId, "|");
            std::string channelId = splitted.at(0);
            std::string symbolId = splitted.at(1);
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
      bool hasError = document.HasMember("error") && !document["error"].IsNull();
      message.setType(!hasError ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
      Element element;
      element.insert(!hasError ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessage);
      message.setElementList({element});
      messageList.push_back(std::move(message));
      event.setMessageList(messageList);
    } else if (document.HasMember("event") && std::string(document["event"].GetString()) == "update") {
      std::string channel = document["channel"].GetString();
      const rj::Value& result = document["result"];
      auto it = result.FindMember("currency_pair");
      std::string currencyPair = it != result.MemberEnd() ? it->value.GetString() : result["s"].GetString();
      MarketDataMessage marketDataMessage;
      std::string exchangeSubscriptionId = channel + "|" + currencyPair;
      std::string channelId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
      std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
      auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
      if (channelId == CCAPI_WEBSOCKET_GATEIO_CHANNEL_BOOK_TICKER) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(result["t"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(result["b"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(result["B"].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
        }
        {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(result["a"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(result["A"].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
        }
        marketDataMessageList.push_back(std::move(marketDataMessage));
      } else if (channelId.rfind(CCAPI_WEBSOCKET_GATEIO_CHANNEL_ORDER_BOOK, 0) == 0) {
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        marketDataMessage.recapType = this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                                          ? MarketDataMessage::RecapType::NONE
                                          : MarketDataMessage::RecapType::SOLICITED;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(result["t"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        int bidIndex = 0;
        int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
        for (const auto& x : result["bids"].GetArray()) {
          if (bidIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
          ++bidIndex;
        }
        int askIndex = 0;
        for (const auto& x : result["asks"].GetArray()) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
          marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
          ++askIndex;
        }
        marketDataMessageList.push_back(std::move(marketDataMessage));
      } else if (channelId == CCAPI_WEBSOCKET_GATEIO_CHANNEL_TRADES) {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.tp = UtilTime::makeTimePointMilli(UtilTime::divideMilli(result["create_time_ms"].GetString()));
        marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        MarketDataMessage::TypeForDataPoint dataPoint;
        dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(result["price"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(result["amount"].GetString()))});
        dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(result["id"].GetString())});
        dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(result["side"].GetString()) == "sell" ? "1" : "0"});
        marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
        marketDataMessageList.push_back(std::move(marketDataMessage));
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
        this->appendSymbolId(queryString, symbolId, "currency_pair");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        this->substituteParam(target, {
                                          {"{currency_pair}", symbolId},
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
    element.insert(CCAPI_INSTRUMENT, x["id"].GetString());
    element.insert(CCAPI_BASE_ASSET, x["base"].GetString());
    element.insert(CCAPI_QUOTE_ASSET, x["quote"].GetString());
    int precision = std::stoi(x["precision"].GetString());
    if (precision > 0) {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "0." + std::string(precision - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1");
    }
    int amountPrecision = std::stoi(x["amount_precision"].GetString());
    if (amountPrecision > 0) {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(amountPrecision - 1, '0') + "1");
    } else {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1");
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
          marketDataMessage.tp = UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["create_time_ms"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["id"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["side"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_BASE_H_
