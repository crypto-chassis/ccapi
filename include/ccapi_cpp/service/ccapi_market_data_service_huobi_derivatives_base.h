#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_DERIVATIVES_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_DERIVATIVES_BASE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP) || defined(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)
#include "ccapi_cpp/service/ccapi_market_data_service_huobi_base.h"

namespace ccapi {

class MarketDataServiceHuobiDerivativesBase : public MarketDataServiceHuobiBase {
 public:
  MarketDataServiceHuobiDerivativesBase(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                        ServiceContext* serviceContextPtr)
      : MarketDataServiceHuobiBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->isDerivatives = true;
  }

  virtual ~MarketDataServiceHuobiDerivativesBase() {}

  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    auto conflateIntervalMilliseconds = std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS));
    if (field == CCAPI_MARKET_DEPTH) {
      if (conflateIntervalMilliseconds < 1000) {
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

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override { return bodyView.find("err_code") != std::string::npos; }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_INSTRUMENT: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        std::string queryString;
        this->appendSymbolId(queryString, symbolId, "contract_code");
        req.target(target + "?" + queryString);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentTarget;
        req.target(target);
      } break;
      default:
        MarketDataServiceHuobiBase::convertRequestForRest(req, request, now, symbolId, credential);
    }
  }

  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["symbol"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, UtilString::normalizeDecimalStringView(x["price_tick"].GetString()));
    element.insert(CCAPI_CONTRACT_SIZE, UtilString::normalizeDecimalStringView(x["contract_size"].GetString()));
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_INSTRUMENT: {
        this->jsonDocumentAllocator.Clear();
        rj::Document document(&this->jsonDocumentAllocator);
        document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        for (const auto& x : document["data"].GetArray()) {
          if (std::string_view(x["contract_code"].GetString()) == request.getInstrument()) {
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
        this->jsonDocumentAllocator.Clear();
        rj::Document document(&this->jsonDocumentAllocator);
        document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
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
        MarketDataServiceHuobiBase::convertTextMessageToMarketDataMessage(request, textMessageView, timeReceived, event, marketDataMessageList);
    }
  }
};

} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_HUOBI_DERIVATIVES_BASE_H_
