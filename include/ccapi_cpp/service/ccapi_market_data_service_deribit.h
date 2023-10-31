#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_DERIBIT_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_DERIBIT_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceDeribit : public MarketDataService {
 public:
  MarketDataServiceDeribit(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                           ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_DERIBIT;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/api/v2";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    //     try {
    //       this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
    // #else
    //     try {
    //       this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    //     } catch (const std::exception& e) {
    //       CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    //     }
    // #endif
    this->restTarget = "/api/v2";
    this->getRecentTradesTarget = "/public/get_last_trades_by_instrument";
    this->getInstrumentTarget = "/public/get_instrument";
    this->getInstrumentsTarget = "/public/get_instruments";
  }
  virtual ~MarketDataServiceDeribit() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
  void onOpen(wspp::connection_hdl hdl) override {
    MarketDataService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    auto now = UtilTime::now();
    this->appendParam(document, allocator, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), "public/set_heartbeat",
                      {
                          {"interval", "10"},
                      });
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string msg = stringBuffer.GetString();
    ErrorCode ec;
    this->send(hdl, msg, wspp::frame::opcode::text, ec);
    if (ec) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
    }
  }
  void onClose(wspp::connection_hdl hdl) override {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->subscriptionJsonrpcIdSetByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
  }
#else
  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    MarketDataService::onOpen(wsConnectionPtr);
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    auto now = UtilTime::now();
    this->appendParam(document, allocator, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), "public/set_heartbeat",
                      {
                          {"interval", "10"},
                      });
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string msg = stringBuffer.GetString();
    ErrorCode ec;
    this->send(wsConnectionPtr, msg, ec);
    if (ec) {
      this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
    }
  }
  void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    this->subscriptionJsonrpcIdSetByConnectionIdMap.erase(wsConnectionPtr->id);
    MarketDataService::onClose(wsConnectionPtr, ec);
  }
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = CCAPI_WEBSOCKET_DERIBIT_CHANNEL_QUOTE;
      } else if (marketDepthRequested <= 20) {
        int marketDepthSubscribedToExchange = 1;
        marketDepthSubscribedToExchange = this->calculateMarketDepthAllowedByExchange(marketDepthRequested, std::vector<int>({10, 20}));
        channelId = CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK;
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
      } else {
        channelId = CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK_TBT;
      }
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override {
    std::vector<std::string> sendStringList;
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("jsonrpc", rj::Value("2.0").Move(), allocator);
    document.AddMember("method", rj::Value("public/subscribe").Move(), allocator);
    auto now = UtilTime::now();
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    document.AddMember("id", rj::Value(requestId).Move(), allocator);
    this->subscriptionJsonrpcIdSetByConnectionIdMap[wsConnection.id].insert(requestId);
    rj::Value channels(rj::kArrayType);
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;
        if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_QUOTE || channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        std::string exchangeSubscriptionId(channelId);
        std::map<std::string, std::string> replaceMap;
        if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_QUOTE) {
          replaceMap = {
              {"{instrument_name}", symbolId},
          };
        } else if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK) {
          int marketDepthSubscribedToExchange =
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          replaceMap = {
              {"{instrument_name}", symbolId},
              {"{group}", "none"},
              {"{depth}", std::to_string(marketDepthSubscribedToExchange)},
              {"{interval}", "100ms"},
          };
        } else if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK_TBT) {
          replaceMap = {
              {"{instrument_name}", symbolId},
              {"{interval}", "raw"},
          };
        } else if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_TRADES) {
          replaceMap = {
              {"{instrument_name}", symbolId},
              {"{interval}", "raw"},
          };
        }
        for (const auto& x : replaceMap) {
          auto toReplace = x.first;
          auto replacement = x.second;
          if (exchangeSubscriptionId.find(toReplace) != std::string::npos) {
            exchangeSubscriptionId.replace(exchangeSubscriptionId.find(toReplace), toReplace.length(), replacement);
          }
        }
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
        channels.PushBack(rj::Value(exchangeSubscriptionId.c_str(), allocator).Move(), allocator);
      }
    }
    rj::Value params(rj::kObjectType);
    params.AddMember("channels", channels, allocator);
    document.AddMember("params", params, allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string sendString = stringBuffer.GetString();
    sendStringList.push_back(sendString);
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
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    auto it = document.FindMember("result");
    if (it == document.MemberEnd()) {
      std::string method = document["method"].GetString();
      const rj::Value& params = document["params"];
      if (method == "subscription") {
        std::string exchangeSubscriptionId = params["channel"].GetString();
        const std::string& channelId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        const std::string& symbolId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
        const rj::Value& data = params["data"];
        if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK || channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_QUOTE ||
            channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK_TBT) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(data["timestamp"].GetString())));
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK) {
            if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
              marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            } else {
              marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            }
            auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
            int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
            int bidIndex = 0;
            for (const auto& x : data["bids"].GetArray()) {
              if (bidIndex >= maxMarketDepth) {
                break;
              }
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
              ++bidIndex;
            }
            int askIndex = 0;
            for (const auto& x : data["asks"].GetArray()) {
              if (askIndex >= maxMarketDepth) {
                break;
              }
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[1].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
              ++askIndex;
            }
          } else if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_QUOTE) {
            if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
              marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            } else {
              marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            }
            {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data["best_bid_price"].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["best_bid_amount"].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            }
            {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(data["best_ask_price"].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(data["best_ask_amount"].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
          } else if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_BOOK_TBT) {
            auto it = data.FindMember("prev_change_id");
            if (it == data.MemberEnd() || it->value.IsNull()) {
              marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            } else {
              marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            }
            for (const auto& x : data["bids"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[1].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[2].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
            }
            for (const auto& x : data["asks"].GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[1].GetString())});
              dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(x[2].GetString())});
              marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
            }
          }
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        } else if (channelId == CCAPI_WEBSOCKET_DERIBIT_CHANNEL_TRADES) {
          for (const auto& x : data.GetArray()) {
            MarketDataMessage marketDataMessage;
            marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["timestamp"].GetString())));
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["trade_id"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::SEQUENCE_NUMBER, std::string(x["trade_seq"].GetString())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
            marketDataMessageList.emplace_back(std::move(marketDataMessage));
          }
        }
      } else if (method == "heartbeat") {
        std::string type = document["params"]["type"].GetString();
        if (type == "test_request") {
          rj::Document document;
          document.SetObject();
          rj::Document::AllocatorType& allocator = document.GetAllocator();
          auto now = UtilTime::now();
          this->appendParam(document, allocator, std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), "/public/test", {});
          rj::StringBuffer stringBuffer;
          rj::Writer<rj::StringBuffer> writer(stringBuffer);
          document.Accept(writer);
          std::string msg = stringBuffer.GetString();
          ErrorCode ec;
#ifdef CCAPI_LEGACY_USE_WEBSOCKETPP
          this->send(hdl, msg, wspp::frame::opcode::text, ec);
#else
          this->send(wsConnectionPtr, msg, ec);
#endif
          if (ec) {
            this->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, ec, "request");
          }
        }
      }
    } else {
      auto it = document.FindMember("id");
      if (it != document.MemberEnd()) {
        int64_t id = std::stoll(it->value.GetString());
        if (this->subscriptionJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).find(id) !=
            this->subscriptionJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).end()) {
          if (document["result"].GetArray().Empty()) {
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
            event.setType(Event::Type::SUBSCRIPTION_STATUS);
            std::vector<Message> messageList;
            Message message;
            message.setTimeReceived(timeReceived);
            std::vector<std::string> correlationIdList;
            if (this->correlationIdListByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id) !=
                this->correlationIdListByConnectionIdChannelIdSymbolIdMap.end()) {
              for (const auto& x : document["result"].GetArray()) {
                std::string exchangeSubscriptionId = x.GetString();
                const std::string& channelId =
                    this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
                const std::string& symbolId =
                    this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
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
            message.setCorrelationIdList(correlationIdList);
            message.setType(Message::Type::SUBSCRIPTION_STARTED);
            Element element;
            element.insert(CCAPI_INFO_MESSAGE, textMessage);
            message.setElementList({element});
            messageList.emplace_back(std::move(message));
            event.setMessageList(messageList);
          }
        }
        this->subscriptionJsonrpcIdSetByConnectionIdMap.at(wsConnection.id).erase(id);
      }
    }
  }
  void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, int64_t requestId, const std::string& method,
                   const std::map<std::string, std::string>& param, const std::map<std::string, std::string> standardizationMap = {}) {
    document.AddMember("jsonrpc", rj::Value("2.0").Move(), allocator);
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
  void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
    document["params"].AddMember("instrument_name", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
  }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    int64_t requestId = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    switch (request.getOperation()) {
      case Request::Operation::GENERIC_PUBLIC_REQUEST: {
        MarketDataService::convertRequestForRestGenericPublicRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::GET_RECENT_TRADES: {
        req.method(http::verb::post);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(document, allocator, requestId, this->getRecentTradesTarget, param,
                          {
                              {CCAPI_LIMIT, "count"},
                          });
        if (document["params"].FindMember("sorting") == document["params"].MemberEnd()) {
          document["params"].AddMember("sorting", rj::Value("desc").Move(), allocator);
        }
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        req.target(this->restTarget);
      } break;
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::post);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        this->appendParam(document, allocator, requestId, this->getInstrumentTarget, {});
        this->appendSymbolId(document, allocator, symbolId);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        req.target(this->restTarget);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::post);
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(document, allocator, requestId, this->getInstrumentsTarget, param,
                          {
                              {CCAPI_EM_ASSET, "currency"},
                          });
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        auto body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
        req.target(this->restTarget);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["instrument_name"].GetString());
    element.insert(CCAPI_MARGIN_ASSET, x["base_currency"].GetString());
    element.insert(CCAPI_UNDERLYING_SYMBOL, x["base_currency"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["tick_size"].GetString());
    element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, x["contract_size"].GetString());
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    rj::Document document;
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document["result"]["trades"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["timestamp"].GetString())));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["trade_id"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SEQUENCE_NUMBER, std::string(x["trade_seq"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["direction"].GetString()) == "sell" ? "1" : "0"});
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
  std::map<std::string, std::set<int64_t>> subscriptionJsonrpcIdSetByConnectionIdMap;
  std::string restTarget;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_DERIBIT_H_
