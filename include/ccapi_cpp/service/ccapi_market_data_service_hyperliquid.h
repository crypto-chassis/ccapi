#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HYPERLIQUID_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HYPERLIQUID_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_HYPERLIQUID
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceHyperliquid : public MarketDataService {
 public:
  MarketDataServiceHyperliquid(std::function<void(Event& , Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContext* serviceContextPtr): MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HYPERLIQUID;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->getInstrumentsTarget = "/info";
  }
  virtual ~MarketDataServiceHyperliquid() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifndef CCAPI_CANDLE_INTERVAL
#define CCAPI_CANDLE_INTERVAL "CANDLE_INTERVAL"
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    if (field == CCAPI_MARKET_DEPTH) {
      int maxMarketDepth = 1;
      if (optionMap.find(CCAPI_MARKET_DEPTH_MAX) != optionMap.end()) {
        maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      }
      if (maxMarketDepth == 1) {
        channelId = CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO;
      } else {
        channelId = CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2BOOK;
      }
    } else if (field == CCAPI_TRADE) {
      channelId = CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_TRADES;
    } else if (field == CCAPI_CANDLESTICK) {
      channelId = CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_CANDLE;
    }
  }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;
    auto& wsConnection = *wsConnectionPtr;
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        std::string exchangeChannelId = channelId;
        
        std::string exchangeSubscriptionId = exchangeChannelId + ":" + symbolId;

        if (exchangeChannelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2BOOK || exchangeChannelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_TRADES ||
            exchangeChannelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_CANDLE || exchangeChannelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
          if (exchangeChannelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2BOOK) {
            this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
          }

          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();

          document.AddMember("method", rj::Value("subscribe").Move(), allocator);

          rj::Value subscription(rj::kObjectType);
          subscription.AddMember("type", rj::Value(exchangeChannelId.c_str(), allocator).Move(), allocator);
          subscription.AddMember("coin", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
          if (exchangeChannelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_CANDLE) {
            auto optionMap = subscriptionListBySymbolId.second.at(0).getOptionMap();
            std::string interval = optionMap.find(CCAPI_CANDLE_INTERVAL) != optionMap.end() ? optionMap.at(CCAPI_CANDLE_INTERVAL) : "1m";
            subscription.AddMember("interval", rj::Value(interval.c_str(), allocator).Move(), allocator);
          }

          document.AddMember("subscription", subscription, allocator);

          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string sendString = stringBuffer.GetString();
          sendStringList.push_back(sendString);

          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        }
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
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());

    if (document.HasMember("channel") && document["channel"].IsString()) {
      std::string channel = document["channel"].GetString();
      if (channel != "subscriptionResponse" && channel != CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
          // ignore
      } else {
           std::cout << "DEBUG: Received message on channel: " << channel << " Body: " << textMessage << std::endl;
      }
      
      if (channel == "subscriptionResponse") {
        // Extract channelId and symbolId from the subscription response
        const rj::Value& data = document["data"];
        const rj::Value& subscription = data["subscription"];
        std::string channelId = subscription["type"].GetString();
        std::string symbolId = subscription["coin"].GetString();
        std::string channelIdToCheck = channelId;
        if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2BOOK || channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
          channelIdToCheck = CCAPI_MARKET_DEPTH;
        } else if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_TRADES) {
          channelIdToCheck = CCAPI_TRADE;
        } else if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_CANDLE) {
          channelIdToCheck = CCAPI_CANDLESTICK;
        }
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;

        // Handle subscription response
        event.setType(Event::Type::SUBSCRIPTION_STATUS);
        std::vector<Message> messageList;
        Message message;
        message.setTimeReceived(timeReceived);
        std::vector<std::string> correlationIdList;
        if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
            this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
          if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).find(channelIdToCheck) !=
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).end()) {
            if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelIdToCheck).find(symbolId) !=
                this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelIdToCheck).end()) {
              std::vector<std::string> correlationIdList_2 =
                  this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelIdToCheck).at(symbolId);
              correlationIdList.insert(correlationIdList.end(), correlationIdList_2.begin(), correlationIdList_2.end());
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
      } else if (channel == "error") {
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
      } else {
        const rj::Value& data = document["data"];
        std::string coin;
        if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_TRADES) {
          coin = data[0]["coin"].GetString();
        } else if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2BOOK) {
          coin = data["coin"].GetString();
        } else if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_CANDLE) {
          coin = data["s"].GetString();
        } else if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
          coin = data["coin"].GetString();
        }
        std::string exchangeSubscriptionId = channel + ":" + coin;
        const std::string& channelId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        const std::string& symbolId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);

        if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_TRADES) {
          const rj::Value& data = document["data"];
          for (const auto& trade : data.GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(trade["time"].GetString())));

            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(trade["px"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(trade["sz"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, trade["tid"].GetString()});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(trade["side"].GetString()) == "B" ? "1" : "0"});

            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        } else if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2BOOK) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          // l2Book is always a snapshot
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(document["data"]["time"].GetString())));

          // Explicitly ensure l2UpdateIsReplace is true for this connection/channel/symbol
          // This is critical because Hyperliquid sends full snapshots, so we must clear the previous book state
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;

          // std::string coin = document["data"]["coin"].GetString();
          const rj::Value& bids = document["data"]["levels"][0];
          const rj::Value& asks = document["data"]["levels"][1];
          
          auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
          int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));

          auto& bidVec = marketDataMessage.data[MarketDataMessage::DataType::BID];
          int bidIndex = 0;
          for (const auto& bid : bids.GetArray()) {
            if (bidIndex >= maxMarketDepth) {
              break;
            }
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(bid["px"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(bid["sz"].GetString())});
            bidVec.emplace_back(std::move(dataPoint));
            ++bidIndex;
          }
          
          auto& askVec = marketDataMessage.data[MarketDataMessage::DataType::ASK];
          int askIndex = 0;
          for (const auto& ask : asks.GetArray()) {
            if (askIndex >= maxMarketDepth) {
              break;
            }
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(ask["px"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(ask["sz"].GetString())});
            askVec.emplace_back(std::move(dataPoint));
            ++askIndex;
          }

          marketDataMessageList.push_back(std::move(marketDataMessage));
        } else if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_CANDLE) {
          const rj::Value& data = document["data"];
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(data["t"].GetInt64()));

          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::OPEN_PRICE, UtilString::normalizeDecimalStringView(data["o"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::HIGH_PRICE, UtilString::normalizeDecimalStringView(data["h"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::LOW_PRICE, UtilString::normalizeDecimalStringView(data["l"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::CLOSE_PRICE, UtilString::normalizeDecimalStringView(data["c"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::VOLUME, UtilString::normalizeDecimalStringView(data["v"].GetString())});

          marketDataMessage.data[MarketDataMessage::DataType::CANDLESTICK].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        } else if (channel == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
          const rj::Value& data = document["data"];
          if (data.IsObject() && data.HasMember("bbo") && data["bbo"].IsArray() && data["bbo"].Size() >= 2) {
              MarketDataMessage marketDataMessage;
              marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
              marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
              marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
              if (data.HasMember("time") && data["time"].IsInt64()) {
                  marketDataMessage.tp = TimePoint(std::chrono::milliseconds(data["time"].GetInt64()));
              } else {
                  marketDataMessage.tp = timeReceived;
              }
               
              const rj::Value& bids = data["bbo"][0];
              const rj::Value& asks = data["bbo"][1];

              auto& bidVec = marketDataMessage.data[MarketDataMessage::DataType::BID];
              if (!bids.IsNull() && bids.IsObject()) {
                   if (bids.HasMember("px") && bids["px"].IsString() && bids.HasMember("sz") && bids["sz"].IsString()) {
                       MarketDataMessage::TypeForDataPoint dataPoint;
                       dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(bids["px"].GetString())});
                       dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(bids["sz"].GetString())});
                       bidVec.emplace_back(std::move(dataPoint));
                   }
              }

              auto& askVec = marketDataMessage.data[MarketDataMessage::DataType::ASK];
              if (!asks.IsNull() && asks.IsObject()) {
                   if (asks.HasMember("px") && asks["px"].IsString() && asks.HasMember("sz") && asks["sz"].IsString()) {
                       MarketDataMessage::TypeForDataPoint dataPoint;
                       dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(asks["px"].GetString())});
                       dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(asks["sz"].GetString())});
                       askVec.emplace_back(std::move(dataPoint));
                   }
              }

              marketDataMessageList.emplace_back(std::move(marketDataMessage));
          } else {
              // std::cout << "DEBUG: bbo message missing required fields" << std::endl;
          }
        }
      }
    }
  }
  
  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                           std::vector<MarketDataMessage>& marketDataMessageList) override {
    std::string textMessage(textMessageView);
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        this->extractInstrumentInfoFromResponse(elementList, document);
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PUBLIC_REQUEST: {
        MarketDataService::convertRequestForRestGenericPublicRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::post);
        req.target(this->getInstrumentsTarget);
        req.set(beast::http::field::content_type, "application/json");
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("type", rj::Value("meta").Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param,
                   const std::map<std::string, std::string>& regularizationMap, const std::map<std::string, std::string>& convertMap) {
    for (const auto& kv : param) {
      auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
      auto value = kv.second;
      auto it = convertMap.find(key);
      if (it != convertMap.end()) {
        value = it->second;
      }
      document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
    }
  }

  void extractInstrumentInfoFromResponse(std::vector<Element>& elementList, const rj::Value& response) {
    int marketId = 0;
    for (const auto& x : response["universe"].GetArray()) {
      Element element;
      element.insert(CCAPI_INSTRUMENT, x["name"].GetString());
      element.insert(CCAPI_BASE_ASSET, x["name"].GetString());
      element.insert(CCAPI_QUOTE_ASSET, "USDC");
      // element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["tickSz"].GetString());
      int amountPrecision = std::stoi(x["szDecimals"].GetString());
      if (amountPrecision > 0) {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "0." + std::string(amountPrecision - 1, '0') + "1");
        element.insert(CCAPI_ORDER_QUANTITY_MIN, "0." + std::string(amountPrecision - 1, '0') + "1");
      } else {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1");
        element.insert(CCAPI_ORDER_QUANTITY_MIN, "1");
      }
      element.insert(CCAPI_MARGIN_ASSET, "USDC");
      element.insert(CCAPI_UNDERLYING_SYMBOL, x["name"].GetString());
      element.insert(CCAPI_CONTRACT_SIZE, "1");
      elementList.emplace_back(element);
      ++marketId;
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_
