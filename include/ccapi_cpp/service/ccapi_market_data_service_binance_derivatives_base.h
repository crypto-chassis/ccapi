#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_DERIVATIVES_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_DERIVATIVES_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES) || defined(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
#include "ccapi_cpp/service/ccapi_market_data_service_binance_base.h"
namespace ccapi {
class MarketDataServiceBinanceDerivativesBase : public MarketDataServiceBinanceBase {
 public:
  MarketDataServiceBinanceDerivativesBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          std::shared_ptr<ServiceContext> serviceContextPtr)
      : MarketDataServiceBinanceBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->isDerivatives = true;
  }
  virtual ~MarketDataServiceBinanceDerivativesBase() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, const WsConnection& wsConnection,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliSeconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      int marketDepthSubscribedToExchange = 1;
      marketDepthSubscribedToExchange = this->calculateMarketDepthSubscribedToExchange(marketDepthRequested, std::vector<int>({1, 5, 10, 20}));
      if (marketDepthSubscribedToExchange == 1) {
        channelId = CCAPI_WEBSOCKET_BINANCE_BASE_CHANNEL_BOOK_TICKER;
      } else {
        std::string updateSpeed;
        if (conflateIntervalMilliSeconds < 250) {
          updateSpeed = "100ms";
        } else if (conflateIntervalMilliSeconds >= 500) {
          updateSpeed = "500ms";
        }
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::to_string(marketDepthSubscribedToExchange);
        if (!updateSpeed.empty()) {
          channelId += "&UPDATE_SPEED=" + updateSpeed;
        }
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = marketDepthSubscribedToExchange;
      }
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_MARGIN_ASSET, x["marginAsset"].GetString());
    element.insert(CCAPI_UNDERLYING_SYMBOL, x["pair"].GetString());
    for (const auto& y : x["filters"].GetArray()) {
      std::string filterType = y["filterType"].GetString();
      if (filterType == "PRICE_FILTER") {
        element.insert(CCAPI_ORDER_PRICE_INCREMENT, y["tickSize"].GetString());
      } else if (filterType == "LOT_SIZE") {
        element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, y["stepSize"].GetString());
        element.insert(CCAPI_ORDER_QUANTITY_MIN, y["minQty"].GetString());
      } else if (filterType == "MIN_NOTIONAL") {
        element.insert(CCAPI_ORDER_PRICE_TIMES_QUANTITY_MIN, y["notional"].GetString());
      }
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
        for (const auto& x : document["symbols"].GetArray()) {
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
        for (const auto& x : document["symbols"].GetArray()) {
          Element element;
          this->extractInstrumentInfo(element, x);
          elementList.push_back(element);
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      default:
        MarketDataServiceBinanceBase::convertTextMessageToMarketDataMessage(request, textMessage, timeReceived, event, marketDataMessageList);
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_BINANCE_DERIVATIVES_BASE_H_
