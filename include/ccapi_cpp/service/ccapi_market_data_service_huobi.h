#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/service/ccapi_market_data_service_huobi_base.h"
namespace ccapi {
class MarketDataServiceHuobi : public MarketDataServiceHuobiBase {
 public:
  MarketDataServiceHuobi(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                         std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceHuobiBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HUOBI;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->setHostWsFromUrlWs(this->baseUrlWs);
    try {
      this->tcpResolverResultsRest = this->resolver.resolve(this->hostRest, this->portRest);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#ifndef CCAPI_USE_BOOST_BEAST_WEBSOCKET
#else
    try {
      this->tcpResolverResultsWs = this->resolverWs.resolve(this->hostWs, this->portWs);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
#endif
    this->getRecentTradesTarget = "/market/history/trade";
    this->getInstrumentTarget = "/v1/common/symbols";
    this->getInstrumentsTarget = "/v1/common/symbols";
  }
  virtual ~MarketDataServiceHuobi() {}
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    CCAPI_LOGGER_TRACE("");
    if (field == CCAPI_MARKET_DEPTH) {
      CCAPI_LOGGER_TRACE("");
      if (conflateIntervalMilliSeconds < 100) {
        CCAPI_LOGGER_TRACE("");
        if (marketDepthRequested == 1) {
          CCAPI_LOGGER_TRACE("");
          channelId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO;
        } else {
          CCAPI_LOGGER_TRACE("");
          channelId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BY_PRICE_REFRESH_UPDATE;
          int marketDepthSubscribedToExchange = 1;
          marketDepthSubscribedToExchange = this->calculateMarketDepthSubscribedToExchange(marketDepthRequested, std::vector<int>({5, 10, 20}));
          channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
          this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
        }
      } else if (conflateIntervalMilliSeconds < 1000) {
        if (marketDepthRequested == 1) {
          channelId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_BBO;
        } else {
          channelId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH;
        }
      } else {
        channelId = CCAPI_WEBSOCKET_HUOBI_CHANNEL_MARKET_DEPTH;
      }
    }
  }
  bool doesHttpBodyContainError(const std::string& body) override { return body.find("err-code") != std::string::npos; }
  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        req.target(target);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
        req.target(target);
      } break;
      default:
        MarketDataServiceHuobiBase::convertRequestForRest(req, request, now, symbolId, credential);
    }
  }
  void convertTextMessageToMarketDataMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_INSTRUMENT: {
        rj::Document document;
        document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
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
        rj::Document document;
        document.Parse<rj::kParseNumbersAsStringsFlag>(textMessage.c_str());
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
        MarketDataServiceHuobiBase::convertTextMessageToMarketDataMessage(request, textMessage, timeReceived, event, marketDataMessageList);
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_H_
