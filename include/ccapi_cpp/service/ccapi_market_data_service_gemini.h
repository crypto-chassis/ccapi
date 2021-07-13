#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/service/ccapi_market_data_service.h"
namespace ccapi {
class MarketDataServiceGemini : public MarketDataService {
 public:
  MarketDataServiceGemini(std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataService(wsEventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GEMINI;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v1/marketdata";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->getRecentTradesTarget = "/v1/trades/:symbol";
  }
  virtual ~MarketDataServiceGemini() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  void prepareSubscriptionDetail(std::string& channelId, const std::string& field, const WsConnection& wsConnection, const std::string& symbolId,
                                 const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    CCAPI_LOGGER_TRACE("marketDepthRequested = " + toString(marketDepthRequested));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    CCAPI_LOGGER_TRACE("conflateIntervalMilliSeconds = " + toString(conflateIntervalMilliSeconds));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        int marketDepthSubscribedToExchange = 1;
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
      }
    }
  }
  std::vector<std::string> createSendStringList(const WsConnection& wsConnection) override { return std::vector<std::string>(); }
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataService::onOpen(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;
      for (auto& subscriptionListByInstrument : subscriptionListByChannelIdSymbolId.second) {
        auto symbolId = subscriptionListByInstrument.first;
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
        if (marketDepthSubscribedToExchange == 1) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
        }
        auto exchangeSubscriptionId = wsConnection.url;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->sequenceByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void processTextMessage(WsConnection& wsConnection, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    rj::Document document;
    document.Parse(textMessage.c_str());
    auto type = std::string(document["type"].GetString());
    CCAPI_LOGGER_TRACE("type = " + type);
    if (this->sessionOptions.enableCheckSequence) {
      int sequence = document["socket_sequence"].GetInt();
      if (!this->checkSequence(wsConnection, sequence)) {
        this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, "");
        return;
      }
    }
    if (type == "update") {
      MarketDataMessage marketDataMessage;
      marketDataMessage.exchangeSubscriptionId = wsConnection.url;
      TimePoint time = timeReceived;
      auto it = document.FindMember("timestampms");
      if (it != document.MemberEnd()) {
        time = TimePoint(std::chrono::milliseconds(it->value.GetInt64()));
      }
      for (auto& event : document["events"].GetArray()) {
        auto gType = std::string(event["type"].GetString());
        if (gType == "change") {
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(event["price"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(event["remaining"].GetString())});
          auto isBid = std::string(event["side"].GetString()) == "bid";
          std::string reason = event["reason"].GetString();
          if (reason == "place" || reason == "cancel" || reason == "trade") {
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = time;
            if (isBid) {
              marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          } else if (reason == "initial") {
            marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            marketDataMessage.tp = time;
            if (isBid) {
              marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          } else if (reason == "top-of-book") {
            marketDataMessage.tp = time;
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            if (isBid) {
              marketDataMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
            } else {
              marketDataMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
            }
          }
        } else if (gType == "trade") {
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          std::string makerSide = event["makerSide"].GetString();
          if (makerSide == "bid" || makerSide == "ask") {
            marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
            marketDataMessage.tp = time;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(event["price"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(event["amount"].GetString()))});
            dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(event["tid"].GetInt64())});
            dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, makerSide == "bid" ? "1" : "0"});
            marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          }
        }
      }
      marketDataMessageList.push_back(std::move(marketDataMessage));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto instrument = subscription.getInstrument();
    auto symbolId = this->convertInstrumentToWebsocketSymbolId(instrument);
    auto field = subscription.getField();
    auto parameterList = UtilString::split(this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(this->exchangeName).at(field), ",");
    std::set<std::string> parameterSet(parameterList.begin(), parameterList.end());
    std::string url = this->baseUrl + "/" + symbolId;
    url += "?";
    if ((parameterSet.find(CCAPI_WEBSOCKET_GEMINI_PARAMETER_BIDS) != parameterSet.end() ||
         parameterSet.find(CCAPI_WEBSOCKET_GEMINI_PARAMETER_OFFERS) != parameterSet.end())) {
      auto optionMap = subscription.getOptionMap();
      if (std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)) == 1) {
        parameterSet.insert(CCAPI_WEBSOCKET_GEMINI_PARAMETER_TOP_OF_BOOK);
      }
    }
    parameterSet.insert("heartbeat");
    bool isFirstParameter = true;
    for (auto const& parameter : parameterSet) {
      if (isFirstParameter) {
        isFirstParameter = false;
      } else {
        url += "&";
      }
      url += parameter + "=true";
    }
    return url;
  }
  bool checkSequence(const WsConnection& wsConnection, int sequence) {
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) == this->sequenceByConnectionIdMap.end()) {
      if (sequence != this->sessionConfigs.getInitialSequenceByExchangeMap().at(this->exchangeName)) {
        CCAPI_LOGGER_WARN("incorrect initial sequence, wsConnection = " + toString(wsConnection));
        return false;
      }
      this->sequenceByConnectionIdMap.insert(std::pair<std::string, int>(wsConnection.id, sequence));
      return true;
    } else {
      CCAPI_LOGGER_DEBUG("sequence: previous = " + toString(this->sequenceByConnectionIdMap[wsConnection.id]) + ", current = " + toString(sequence) +
                         ", wsConnection = " + toString(wsConnection));
      if (sequence - this->sequenceByConnectionIdMap[wsConnection.id] == 1) {
        this->sequenceByConnectionIdMap[wsConnection.id] = sequence;
        return true;
      } else {
        return false;
      }
    }
  }
  void onOutOfSequence(WsConnection& wsConnection, int sequence, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived,
                       const std::string& exchangeSubscriptionId) {
    int previous = 0;
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) != this->sequenceByConnectionIdMap.end()) {
      previous = this->sequenceByConnectionIdMap[wsConnection.id];
    }
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::INCORRECT_STATE_FOUND,
                  "out of sequence: previous = " + toString(previous) + ", current = " + toString(sequence) + ", connection = " + toString(wsConnection) +
                      ", textMessage = " + textMessage + ", timeReceived = " + UtilTime::getISOTimestamp(timeReceived));
    ErrorCode ec;
    this->close(wsConnection, hdl, websocketpp::close::status::normal, "out of sequence", ec);
    if (ec) {
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
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
                                          {":symbol", symbolId},
                                      });
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_LIMIT, "limit_trades"},
                          });
        req.target(target + "?" + queryString);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  std::vector<MarketDataMessage> convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage,
                                                                       const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    std::vector<MarketDataMessage> marketDataMessageList;
    switch (request.getOperation()) {
      case Request::Operation::GET_RECENT_TRADES: {
        for (const auto& x : document.GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(x["timestampms"].GetInt64()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(std::string(x["amount"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::to_string(x["tid"].GetInt64())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string(x["type"].GetString()) == "sell" ? "1" : "0"});
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].push_back(std::move(dataPoint));
          marketDataMessageList.push_back(std::move(marketDataMessage));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return marketDataMessageList;
  }
  std::map<std::string, int> sequenceByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GEMINI_H_
