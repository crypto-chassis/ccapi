#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO
#include "ccapi_cpp/service/ccapi_market_data_service_gateio_base.h"

namespace ccapi {

class MarketDataServiceGateio : public MarketDataServiceGateioBase {
 public:
  MarketDataServiceGateio(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                          ServiceContext* serviceContextPtr)
      : MarketDataServiceGateioBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GATEIO;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws/v4/";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    // this->setHostWsFromUrlWs(this->baseUrlWs);
    this->apiKeyName = CCAPI_GATEIO_API_KEY;
    this->setupCredential({this->apiKeyName});
    std::string prefix = "/api/v4";
    this->getRecentTradesTarget = prefix + "/spot/trades";
    this->getInstrumentTarget = prefix + "/spot/currency_pairs/{currency_pair}";
    this->getInstrumentsTarget = prefix + "/spot/currency_pairs";
    this->getBbosTarget = "/api/v4/spot/tickers";
    this->websocketChannelTrades = CCAPI_WEBSOCKET_GATEIO_CHANNEL_TRADES;
    this->websocketChannelBookTicker = CCAPI_WEBSOCKET_GATEIO_CHANNEL_BOOK_TICKER;
    this->websocketChannelOrderBook = CCAPI_WEBSOCKET_GATEIO_CHANNEL_ORDER_BOOK;
    this->websocketChannelCandlesticks = CCAPI_WEBSOCKET_GATEIO_CHANNEL_CANDLESTICKS;
    this->symbolName = "currency_pair";
  }

  virtual ~MarketDataServiceGateio() {}

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.set("Accept", "application/json");
    req.set(beast::http::field::content_type, "application/json");
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
        this->appendSymbolId(queryString, symbolId, symbolName);
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
      case Request::Operation::GET_BBOS: {
        req.method(http::verb::get);
        auto target = this->getBbosTarget;
        std::string queryString;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->appendParam(queryString, param,
                          {
                              {CCAPI_INSTRUMENT, "currency_pair"},
                          });
        req.target(target + "?" + queryString);
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
          marketDataMessage.tp = UtilTime::makeTimePointMilli(UtilTime::divideMilli(x["create_time_ms"].GetString()));
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["price"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["amount"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["id"].GetString());
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
          elementList.push_back(element);
        }
        message.setElementList(elementList);
        message.setCorrelationIdList({request.getCorrelationId()});
        event.addMessages({message});
      } break;
      case Request::Operation::GET_BBOS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document.GetArray()) {
          Element element;
          element.insert(CCAPI_INSTRUMENT, x["currency_pair"].GetString());
          element.insert(CCAPI_BEST_BID_N_PRICE, x["highest_bid"].GetString());
          {
            const auto& it = x.FindMember("highest_size");
            if (it != x.MemberEnd()) {
              element.insert(CCAPI_BEST_BID_N_SIZE, it->value.GetString());
            }
          }
          element.insert(CCAPI_BEST_ASK_N_PRICE, x["lowest_ask"].GetString());
          {
            const auto& it = x.FindMember("lowest_size");
            if (it != x.MemberEnd()) {
              element.insert(CCAPI_BEST_ASK_N_SIZE, it->value.GetString());
            }
          }
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
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_H_
