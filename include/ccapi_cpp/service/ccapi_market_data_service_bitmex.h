#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITMEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITMEX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_BITMEX
#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceBitmex : public MarketDataService {
 public:
  MarketDataServiceBitmex(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_BITMEX;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/realtime";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->getRecentTradesTarget = "/api/v1/trade";
    this->getInstrumentTarget = "/api/v1/instrument";
    this->getInstrumentsTarget = "/api/v1/instrument";
    // this->convertNumberToStringInJsonRegex = std::regex("(\\[|,|\":)(-?\\d+\\.?\\d*)");
  }

  virtual ~MarketDataServiceBitmex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      if (optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) == CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT) {
        channelId = CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2;
      } else {
        if (marketDepthRequested == 1) {
          channelId = CCAPI_WEBSOCKET_BITMEX_CHANNEL_QUOTE;
        } else if (marketDepthRequested <= 10) {
          channelId = CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10;
        } else if (marketDepthRequested <= 25) {
          channelId = CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2_25;
        }
      }
    }
  }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override { this->send(wsConnectionPtr, "ping", ec); }

  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    this->priceByConnectionIdChannelIdSymbolIdPriceIdMap.erase(wsConnectionPtr->id);
    MarketDataService::onClose(wsConnectionPtr, ec);
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
        if (channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_QUOTE || channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        args.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
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
      if (document.IsObject() && document.HasMember("table")) {
        std::string channelId = document["table"].GetString();
        if (channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10 || channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_QUOTE) {
          std::string action = document["action"].GetString();
          MarketDataMessage::RecapType recapType;
          if (action == "partial") {
            recapType = MarketDataMessage::RecapType::SOLICITED;
          } else {
            recapType = MarketDataMessage::RecapType::NONE;
          }
          int i = 0;
          std::string symbolId;
          std::string exchangeSubscriptionId;
          for (const auto& x : document["data"].GetArray()) {
            if (i == 0) {
              symbolId = x["symbol"].GetString();
              exchangeSubscriptionId = channelId + ":" + symbolId;
            }
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
            marketDataMessage.recapType = recapType;
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.tp = UtilTime::parse(std::string(x["timestamp"].GetString()));
            if (channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_QUOTE) {
              MarketDataMessage::TypeForDataPoint dataPointBid;
              dataPointBid.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["bidPrice"].GetString())});
              dataPointBid.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["bidSize"].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPointBid));
              MarketDataMessage::TypeForDataPoint dataPointAsk;
              dataPointAsk.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["askPrice"].GetString())});
              dataPointAsk.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["askSize"].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPointAsk));
            } else {
              for (const auto& y : x["bids"].GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(y[0].GetString()));
                dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(y[1].GetString()));
                marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
              }
              for (const auto& y : x["asks"].GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(y[0].GetString()));
                dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(y[1].GetString()));
                marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
              }
            }
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
            ++i;
          }
        } else if (channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2 || channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2_25) {
          std::string action = document["action"].GetString();
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          marketDataMessage.tp = timeReceived;
          marketDataMessage.recapType = action == "partial" ? MarketDataMessage::RecapType::SOLICITED : MarketDataMessage::RecapType::NONE;
          int i = 0;
          std::string symbolId;
          std::string exchangeSubscriptionId;
          for (const auto& x : document["data"].GetArray()) {
            if (i == 0) {
              symbolId = x["symbol"].GetString();
              exchangeSubscriptionId = channelId + ":" + symbolId;
              marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            }
            MarketDataMessage::TypeForDataPoint dataPoint;
            std::string_view price;
            std::string_view size;
            std::string priceId = x["id"].GetString();
            if (action == "insert" || action == "partial") {
              price = UtilString::normalizeDecimalStringView(x["price"].GetString());
              size = UtilString::normalizeDecimalStringView(x["size"].GetString());
              this->priceByConnectionIdChannelIdSymbolIdPriceIdMap[wsConnectionPtr->id][channelId][symbolId][priceId] = price;
            } else {
              price = this->priceByConnectionIdChannelIdSymbolIdPriceIdMap[wsConnectionPtr->id][channelId][symbolId][priceId];
              if (price.empty()) {
                this->onIncorrectStatesFound(wsConnectionPtr, textMessageView, timeReceived, exchangeSubscriptionId,
                                             "bitmex update for missing item came through on wsConnection = " + toString(*wsConnectionPtr) +
                                                 ", channelId = " + channelId + ", symbolId = " + symbolId + ", priceId = " + priceId + ". Data: " +
                                                 toString(this->priceByConnectionIdChannelIdSymbolIdPriceIdMap[wsConnectionPtr->id][channelId][symbolId]));
              }
              if (action == "update") {
                size = UtilString::normalizeDecimalStringView(x["size"].GetString());
              } else if (action == "delete") {
                size = "0";
              }
            }
            std::string side = x["side"].GetString();
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, price);
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, size);
            marketDataMessage.data[side == "Buy" ? MarketDataMessage::DataType::BID : MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            ++i;
          }
          if (i > 0) {
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (channelId == CCAPI_WEBSOCKET_BITMEX_CHANNEL_TRADE) {
          std::string action = document["action"].GetString();
          for (const auto& x : document["data"].GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            marketDataMessage.tp = UtilTime::parse(std::string(x["timestamp"].GetString()));
            marketDataMessage.recapType = action == "partial" ? MarketDataMessage::RecapType::SOLICITED : MarketDataMessage::RecapType::NONE;
            std::string symbolId = x["symbol"].GetString();
            marketDataMessage.exchangeSubscriptionId = channelId + ":" + symbolId;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["size"].GetString()));
            auto timePair = UtilTime::divide(marketDataMessage.tp);
            dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["trdMatchID"].GetString());
            dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "Sell" ? "1" : "0");
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        }
      } else if (document.IsObject() && document.HasMember("request") && std::string_view(document["request"]["op"].GetString()) == "subscribe") {
        bool success = document.HasMember("success") && document["success"].GetBool();
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        std::vector<Message> messageList;
        Message message;
        message.setTimeReceived(timeReceived);
        std::vector<std::string> correlationIdList;
        if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
          for (const auto& x : document["request"]["args"].GetArray()) {
            auto splitted = UtilString::split(x.GetString(), ":");
            std::string channelId = splitted.at(0);
            std::string symbolId = splitted.at(1);
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
        }
        message.setCorrelationIdList(correlationIdList);
        message.setType(success ? Message::Type::SUBSCRIPTION_STARTED : Message::Type::SUBSCRIPTION_FAILURE);
        Element element;
        element.insert(success ? CCAPI_INFO_MESSAGE : CCAPI_ERROR_MESSAGE, textMessageView);
        message.setElementList({element});
        messageList.emplace_back(std::move(message));
        event.setMessageList(messageList);
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
                              {CCAPI_LIMIT, "count"},
                              {"reverse", "true"},
                          });
        this->appendSymbolId(queryString, symbolId, "symbol");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
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
    element.insert(CCAPI_SETTLE_ASSET, x["settlCurrency"].GetString());
    element.insert(CCAPI_UNDERLYING_SYMBOL, x["referenceSymbol"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["tickSize"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSize"].GetString());
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
          marketDataMessage.tp = UtilTime::parse(std::string(x["timestamp"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["size"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["trdMatchID"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "Sell" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document.GetArray()) {
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

  std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>> priceByConnectionIdChannelIdSymbolIdPriceIdMap;
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BITMEX_H_
