#pragma once

#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifdef CCAPI_ENABLE_EXCHANGE_HYPERLIQUID

#include "ccapi_cpp/service/ccapi_market_data_service.h"

namespace ccapi {

class MarketDataServiceHyperliquid : public MarketDataService {
 public:
  MarketDataServiceHyperliquid(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                               ServiceContext* serviceContextPtr)
      : MarketDataService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HYPERLIQUID;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->infoTarget = "/info";
  }

  virtual ~MarketDataServiceHyperliquid() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif

  void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                 const Subscription& subscription, const std::map<std::string, std::string> optionMap) override {
    auto marketDepthRequested = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    if (field == CCAPI_MARKET_DEPTH) {
      if (marketDepthRequested == 1) {
        channelId = CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO;
      } else {
        channelId = CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2_BOOK;
      }
    }
  }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"method":"ping"})", ec);
  }

  std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::vector<std::string> sendStringList;

    for (const auto& subscriptionListByChannelIdSymbolId : this->subscriptionListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
      auto channelId = subscriptionListByChannelIdSymbolId.first;

      for (const auto& subscriptionListBySymbolId : subscriptionListByChannelIdSymbolId.second) {
        std::string symbolId = subscriptionListBySymbolId.first;

        rj::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        document.AddMember("method", rj::Value("subscribe", allocator), allocator);

        rj::Value subscription(rj::kObjectType);

        if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        } else if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2_BOOK) {
          this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
        }

        subscription.AddMember("type", rj::Value(channelId.c_str(), allocator), allocator);
        subscription.AddMember("coin", rj::Value(symbolId.c_str(), allocator), allocator);

        document.AddMember("subscription", subscription, allocator);

        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);

        sendStringList.push_back(stringBuffer.GetString());

        std::string exchangeSubscriptionId = channelId + ":" + symbolId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID] = channelId;
        this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID] = symbolId;
      }
    }

    return sendStringList;
  }

  void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                          std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());

    if (document.IsObject() && document.HasMember("channel")) {
      std::string channelId = document["channel"].GetString();
      const auto& data = document["data"];

      if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_BBO) {
        std::string symbolId = data["coin"].GetString();
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;

        MarketDataMessage marketDataMessage;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(data["time"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        } else {
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
        }

        const auto& bbo = data["bbo"];
        if (!bbo[0].IsNull()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(bbo[0]["px"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(bbo[0]["sz"].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        } else {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, CCAPI_BEST_BID_N_PRICE_EMPTY);
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, CCAPI_BEST_BID_N_SIZE_EMPTY);
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }

        if (!bbo[1].IsNull()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(bbo[1]["px"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(bbo[1]["sz"].GetString()));
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        } else {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, CCAPI_BEST_ASK_N_PRICE_EMPTY);
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, CCAPI_BEST_ASK_N_SIZE_EMPTY);
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));

      } else if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_TRADE) {
        for (const auto& x : document["data"].GetArray()) {
          MarketDataMessage marketDataMessage;
          marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE;
          marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(x["time"].GetString())));
          std::string symbolId = x["coin"].GetString();
          std::string exchangeSubscriptionId = channelId + ":" + symbolId;
          marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["px"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["sz"].GetString()));
          dataPoint.emplace(MarketDataMessage::DataFieldType::TRADE_ID, x["tid"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::IS_BUYER_MAKER, std::string_view(x["side"].GetString()) == "A" ? "1" : "0");
          marketDataMessage.data[MarketDataMessage::DataType::TRADE].emplace_back(std::move(dataPoint));
          marketDataMessageList.emplace_back(std::move(marketDataMessage));
        }
      } else if (channelId == CCAPI_WEBSOCKET_HYPERLIQUID_CHANNEL_L2_BOOK) {
        std::string symbolId = data["coin"].GetString();
        std::string exchangeSubscriptionId = channelId + ":" + symbolId;

        MarketDataMessage marketDataMessage;
        marketDataMessage.tp = TimePoint(std::chrono::milliseconds(std::stoll(data["time"].GetString())));
        marketDataMessage.exchangeSubscriptionId = exchangeSubscriptionId;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
          marketDataMessage.recapType = MarketDataMessage::RecapType::NONE;
        } else {
          marketDataMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
        }

        const auto& levels = data["levels"];
        if (!levels[0].IsNull()) {
          for (const auto& x : levels[0].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["px"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["sz"].GetString()));
            marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
          }
        }

        if (!levels[1].IsNull()) {
          for (const auto& x : levels[1].GetArray()) {
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalStringView(x["px"].GetString()));
            dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, UtilString::normalizeDecimalStringView(x["sz"].GetString()));
            marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
          }
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      }
    }
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    req.method(http::verb::post);
    req.target(this->infoTarget);
    req.set(beast::http::field::content_type, "application/json");
    switch (request.getOperation()) {
      case Request::Operation::GET_MARKET_DEPTH: {
        auto target = this->infoTarget;
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("type", rj::Value("l2Book", allocator), allocator);
        document.AddMember("coin", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        rj::StringBuffer stringBuffer;
        rj::Writer<rj::StringBuffer> writer(stringBuffer);
        document.Accept(writer);
        std::string body = stringBuffer.GetString();
        req.body() = body;
        req.prepare_payload();
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        auto target = this->infoTarget;
        rj::Document document;
        document.SetObject();
        rj::Document::AllocatorType& allocator = document.GetAllocator();
        document.AddMember("type", rj::Value("meta", allocator), allocator);
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

  void extractInstrumentInfo(Element& element, const rj::Value& x) {
    element.insert(CCAPI_INSTRUMENT, x["name"].GetString());
    const auto& szDecimalsStr = x["szDecimals"].GetString();
    int szDecimals = std::stoi(szDecimalsStr);
    int pxDecimals = 6 - szDecimals;
    if (pxDecimals > 0) {
      const auto& priceIncrementStr = "0." + std::string(pxDecimals - 1, '0') + "1";
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, priceIncrementStr);
    } else {
      element.insert(CCAPI_ORDER_PRICE_INCREMENT, "1");
    }
    if (szDecimals > 0) {
      const auto& quantityIncrementStr = "0." + std::string(szDecimals - 1, '0') + "1";
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, quantityIncrementStr);
      element.insert(CCAPI_ORDER_QUANTITY_MIN, quantityIncrementStr);
    } else {
      element.insert(CCAPI_ORDER_QUANTITY_INCREMENT, "1");
      element.insert(CCAPI_ORDER_QUANTITY_MIN, "1");
    }
  }

  void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived, Event& event,
                                             std::vector<MarketDataMessage>& marketDataMessageList) override {
    this->jsonDocumentAllocator.Clear();
    rj::Document document(&this->jsonDocumentAllocator);
    document.Parse<rj::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    switch (request.getOperation()) {
      case Request::Operation::GET_MARKET_DEPTH: {
        MarketDataMessage marketDataMessage;
        marketDataMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
        const rj::Value& levels = document["levels"];
        marketDataMessage.tp = UtilTime::makeTimePointFromMilliseconds(std::stoll(document["time"].GetString()));
        for (const auto& x : levels[0].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x["px"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x["sz"].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::BID].emplace_back(std::move(dataPoint));
        }
        for (const auto& x : levels[1].GetArray()) {
          MarketDataMessage::TypeForDataPoint dataPoint;
          dataPoint.emplace(MarketDataMessage::DataFieldType::PRICE, x["px"].GetString());
          dataPoint.emplace(MarketDataMessage::DataFieldType::SIZE, x["sz"].GetString());
          marketDataMessage.data[MarketDataMessage::DataType::ASK].emplace_back(std::move(dataPoint));
        }
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
      } break;
      case Request::Operation::GET_INSTRUMENTS: {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(this->requestOperationToMessageTypeMap.at(request.getOperation()));
        std::vector<Element> elementList;
        for (const auto& x : document["universe"].GetArray()) {
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

  std::string infoTarget;
};

} /* namespace ccapi */
#endif
#endif
