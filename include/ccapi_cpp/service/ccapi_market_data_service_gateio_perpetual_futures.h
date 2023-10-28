#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_PERPETUAL_FUTURES_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_PERPETUAL_FUTURES_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
#include "ccapi_cpp/service/ccapi_market_data_service_gateio_base.h"
namespace ccapi {
class MarketDataServiceGateioPerpetualFutures : public MarketDataServiceGateioBase {
 public:
  MarketDataServiceGateioPerpetualFutures(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                          ServiceContext* serviceContextPtr)
      : MarketDataServiceGateioBase(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_GATEIO_PERPETUAL_FUTURES;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/v4/ws/";
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
    this->apiKeyName = CCAPI_GATEIO_PERPETUAL_FUTURES_API_KEY;
    this->setupCredential({this->apiKeyName});
    std::string prefix = "/api/v4";
    this->getRecentTradesTarget = prefix + "/futures/{settle}/trades";
    this->getInstrumentTarget = prefix + "/futures/{settle}/contracts/{contract}";
    this->getInstrumentsTarget = prefix + "/futures/{settle}/contracts";
    this->isDerivatives = true;
    this->websocketChannelTrades = CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_TRADES;
    this->websocketChannelBookTicker = CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_BOOK_TICKER;
    this->websocketChannelOrderBook = CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_ORDER_BOOK;
    this->websocketChannelCandlesticks = CCAPI_WEBSOCKET_GATEIO_PERPETUAL_FUTURES_CHANNEL_CANDLESTICKS;
    this->symbolName = "contract";
  }
  virtual ~MarketDataServiceGateioPerpetualFutures() {}
  std::string getInstrumentGroup(const Subscription& subscription) override {
    auto instrument = subscription.getInstrument();
    std::string url(this->baseUrlWs);
    if (UtilString::endsWith(instrument, "_USD")) {
      url += "btc";
    } else if (UtilString::endsWith(instrument, "_USDT")) {
      url += "usdt";
    }
    return url + "|" + subscription.getField() + "|" + subscription.getSerializedOptions();
  }
  void substituteParamSettle(std::string& target, const std::map<std::string, std::string>& param, const std::string& symbolId) {
    this->substituteParam(target, param,
                          {
                              {"settle", "{settle}"},
                              {CCAPI_MARGIN_ASSET, "{settle}"},
                          });
    std::string settle;
    if (UtilString::endsWith(symbolId, "_USD")) {
      settle = "btc";
    } else if (UtilString::endsWith(symbolId, "_USDT")) {
      settle = "usdt";
    }
    this->substituteParam(target, {
                                      {"{settle}", settle},
                                  });
  }
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
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->substituteParamSettle(target, param, symbolId);
        std::string queryString;
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
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->substituteParamSettle(target, param, symbolId);
        this->substituteParam(target, {
                                          {"{contract}", symbolId},
                                      });
        req.target(target);
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        req.method(http::verb::get);
        auto target = this->getInstrumentsTarget;
        const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
        this->substituteParam(target, param,
                              {
                                  {"settle", "{settle}"},
                                  {CCAPI_MARGIN_ASSET, "{settle}"},
                              });
        req.target(target);
      } break;
      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }
  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["name"].GetString());
    element.insert(CCAPI_ORDER_PRICE_INCREMENT, x["order_price_round"].GetString());
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
          std::string size = x["size"].GetString();
          std::string sizeAbs;
          bool isBuyerMaker;
          if (size.at(0) == '-') {
            sizeAbs = size.substr(1);
            isBuyerMaker = true;
          } else {
            sizeAbs = size;
            isBuyerMaker = false;
          }
          dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(std::string(x["price"].GetString()))});
          dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalString(sizeAbs)});
          dataPoint.insert({MarketDataMessage::DataFieldType::TRADE_ID, std::string(x["id"].GetString())});
          dataPoint.insert({MarketDataMessage::DataFieldType::IS_BUYER_MAKER, isBuyerMaker ? "1" : "0"});
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
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_GATEIO_PERPETUAL_FUTURES_H_
