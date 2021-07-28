#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceOkex : public MarketDataService {
 public:
  MarketDataServiceOkex(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                        std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_OKEX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + CCAPI_OKEX_PUBLIC_WS_PATH;
    this->needDecompressWebsocketMessage = true;
    ErrorCode ec = this->inflater.init(false);
    if (ec) {
      CCAPI_LOGGER_FATAL(ec.message());
    }
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->getRecentTradesTarget = "/api/v5/market/trades";
    this->getInstrumentTarget = "/api/v5/public/instruments";
  }
  virtual ~MarketDataServiceOkex() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
bool doesHttpBodyContainError(const Request& request, const std::string& body) override { return !std::regex_search(body, std::regex("\"code\":\\s*\"0\"")); }
void prepareSubscriptionDetail(std::string& channelId, const std::string& field, const WsConnection& wsConnection, const std::string& symbolId,
                                 const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    CCAPI_LOGGER_TRACE("marketDepthRequested = " + toString(marketDepthRequested));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    CCAPI_LOGGER_TRACE("conflateIntervalMilliSeconds = " + toString(conflateIntervalMilliSeconds));
    if (field == CCAPI_MARKET_DEPTH) {
      if (conflateIntervalMilliSeconds < 100) {
        if (marketDepthRequested <= 50) {
          channelId = CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH50_L2_TBT;
        } else {
          channelId = CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH400_L2_TBT;
        }
      } else {
        if (marketDepthRequested <= 5) {
          channelId = CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH5;
        } else {
          channelId = CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH400;
        }
      }
    }
  }
  void pingOnApplicationLevel(wspp::connection_hdl hdl, ErrorCode& ec) override { this->send(hdl, "ping", wspp::frame::opcode::text, ec); }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("op", rj::Value("subscribe").Move(), allocator);
    rj::Value args(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        if (channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH5) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId = UtilString::split(channelId, "?").at(0) + ":" + symbolId;
        rj::Value arg(rj::kObjectType);
        arg.AddMember("channel", rj::Value(channelId.c_str(), allocator).Move(), allocator);
        arg.AddMember("instId", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        args.PushBack(arg, allocator);
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
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
        csData.push_back(toString(i1->first));
        csData.push_back(i1->second);
        ++i1;
      }
      if (i2 != snapshotAsk.end()) {
        csData.push_back(toString(i2->first));
        csData.push_back(i2->second);
        ++i2;
      }
      ++i;
    }
    std::string csStr = UtilString::join(csData, ":");
    CCAPI_LOGGER_DEBUG("csStr = " + csStr);
    uint_fast32_t csCalc = UtilAlgorithm::crc(csStr.begin(), csStr.end());
    return intToHex(csCalc);
  }
  void processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    if (textMessage != "pong") {
      rj::Document document;
      document.Parse(textMessage.c_str());
      if (document.IsObject() && document.HasMember("arg")) {
        const rj::Value& arg = document["arg"];
        std::string channelId = arg["channel"].GetString();
        std::string symbolId = arg["instId"].GetString();
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;
        if (document.HasMember("event")) {
          std::string eventStr = document["event"].GetString();
          if (eventStr == "subscribe") {
            event.setType(Event::Type::SUBSCRIPTION_STATUS);
            std::vector<Message> messageList;
            Message message;
            message.setTimeReceived(timeReceived);
            std::vector<std::string> correlationIdList;
            if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
                this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
              const rj::Value& arg = document["arg"];
              std::string channelId = arg["channel"].GetString();
              std::string symbolId = arg["instId"].GetString();
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
            message.setCorrelationIdList(correlationIdList);
            message.setType(Message::Type::SUBSCRIPTION_STARTED);
            Element element;
            element.insert(CCAPI_INFO_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.push_back(std::move(message));
            event.setMessageList(messageList);
          } else if (eventStr == "error") {
            event.setType(Event::Type::SUBSCRIPTION_STATUS);
            std::vector<Message> messageList;
            Message message;
            message.setTimeReceived(timeReceived);
            message.setType(Message::Type::SUBSCRIPTION_FAILURE);
            Element element;
            element.insert(CCAPI_ERROR_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.push_back(std::move(message));
            event.setMessageList(messageList);
          }
        } else {
          if (channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH5 || channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH400 ||
              channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH50_L2_TBT || channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH400_L2_TBT) {
            std::string action = channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH5 ? "" : document["action"].GetString();
            CCAPI_LOGGER_TRACE("action = " + toString(action));
            for (const auto& datum : document["data"].GetArray()) {
              CCAPI_LOGGER_TRACE("this->sessionOptions.enableCheckOrderBookChecksum = " + toString(this->sessionOptions.enableCheckOrderBookChecksum));
              if (this->sessionOptions.enableCheckOrderBookChecksum) {
                auto it = datum.FindMember("checksum");
                if (it != datum.MemberEnd()) {
                  this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnection.id][symbolId] =
                      intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(it->value.GetInt())));
                }
              }
              CCAPI_LOGGER_TRACE("this->orderBookChecksumByConnectionIdSymbolIdMap = " + toString(this->orderBookChecksumByConnectionIdSymbolIdMap));
              MarketDataMessage marketDataMessage;
              marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
              CCAPI_LOGGER_TRACE("marketDataMessage.tp = " + toString(marketDataMessage.tp));
              marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
              marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
              if (channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_PUBLIC_DEPTH5) {
                if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
                  marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
                } else {
                  marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
                }
              } else {
                marketDataMessage.recapType = action == "update" ? MarketDataMessage::RecapType::NONE : MarketDataMessage::RecapType::SOLICITED;
              }
              for (const auto& x : datum["bids"].GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
                marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
              }
              for (const auto& x : datum["asks"].GetArray()) {
                MarketDataMessage::TypeForDataPoint dataPoint;
                dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
                marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
              }
              marketDataMessageList.push_back(std::move(marketDataMessage));
            }
          } else if (channelId == CCAPI_WEBSOCKET_OKEX_CHANNEL_TRADE) {
            for (const auto& datum : document["data"].GetArray()) {
              MarketDataMessage marketDataMessage;
              marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
              marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
              marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
              marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(datum["px"].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(datum["sz"].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, datum["tradeId"].GetString()});
              dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(datum["side"].GetString()) == "sell" ? "1" : "0"});
              marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
              marketDataMessageList.push_back(std::move(marketDataMessage));
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
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& datum : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(datum["ts"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(datum["px"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(datum["sz"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, datum["tradeId"].GetString()});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(datum["side"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["data"].GetArray()) {
          if (std::string(x["instId"].GetString()) == request.getInstrument()) {
            Element element;
            element.insert(CCAPI_BASE_ASSET, x["baseCcy"].GetString());
            element.insert(CCAPI_QUOTE_ASSET, x["quoteCcy"].GetString());
            element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["tickSz"].GetString());
            element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["lotSz"].GetString());
            element.insert(CCAPI_MARGIN_ASSET, x["settleCcy"].GetString());
            element.insert(CCAPI_UNDERLYING_SYMBOL, x["uly"].GetString());
            message.setElementList({element});
            break;
          }
        }
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_OKEX_H_
