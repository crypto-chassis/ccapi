#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util_private.h"
#include "ccapi_cpp/service/ccapi_service.h"

namespace ccapi {

/**
 * The MarketDataService class inherits from the Service class and provides implemenations more specific to market data such as order book, trades, etc..
 */
class MarketDataService : public Service {
 public:
  MarketDataService(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                    ServiceContext* serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->requestOperationToMessageTypeMap = {
        {Request::Operation::GET_RECENT_TRADES, Message::Type::GET_RECENT_TRADES},
        {Request::Operation::GET_HISTORICAL_TRADES, Message::Type::GET_HISTORICAL_TRADES},
        {Request::Operation::GET_RECENT_AGG_TRADES, Message::Type::GET_RECENT_AGG_TRADES},
        {Request::Operation::GET_HISTORICAL_AGG_TRADES, Message::Type::GET_HISTORICAL_AGG_TRADES},
        {Request::Operation::GET_RECENT_CANDLESTICKS, Message::Type::GET_RECENT_CANDLESTICKS},
        {Request::Operation::GET_HISTORICAL_CANDLESTICKS, Message::Type::GET_HISTORICAL_CANDLESTICKS},
        {Request::Operation::GET_MARKET_DEPTH, Message::Type::GET_MARKET_DEPTH},
        {Request::Operation::GET_SERVER_TIME, Message::Type::GET_SERVER_TIME},
        {Request::Operation::GET_INSTRUMENT, Message::Type::GET_INSTRUMENT},
        {Request::Operation::GET_INSTRUMENTS, Message::Type::GET_INSTRUMENTS},
        {Request::Operation::GET_BBOS, Message::Type::GET_BBOS},
        {Request::Operation::GET_TICKERS, Message::Type::GET_TICKERS},
    };
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual ~MarketDataService() {
    for (const auto& x : this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap) {
      for (const auto& y : x.second) {
        for (const auto& z : y.second) {
          z.second->cancel();
        }
      }
    }
    for (const auto& x : this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap) {
      for (const auto& y : x.second) {
        y.second->cancel();
      }
    }
  }

  // subscriptions are grouped and each group creates a unique websocket connection
  void subscribe(std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->shouldContinue.load()) {
      for (auto& x : this->groupSubscriptionListByInstrumentGroup(subscriptionList)) {
        const auto& instrumentGroup = x.first;
        auto& subscriptionListGivenInstrumentGroup = x.second;
        boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<MarketDataService>(), instrumentGroup,
                                                                   subscriptionListGivenInstrumentGroup]() mutable {
          const auto& now = UtilTime::now();
          for (auto& subscription : subscriptionListGivenInstrumentGroup) {
            subscription.setTimeSent(now);
          }
          std::map<std::string, std::vector<std::string>> wsConnectionIdListByInstrumentGroupMap = invertMapMulti(that->instrumentGroupByWsConnectionIdMap);
          if (wsConnectionIdListByInstrumentGroupMap.find(instrumentGroup) != wsConnectionIdListByInstrumentGroupMap.end() &&
              that->subscriptionStatusByInstrumentGroupInstrumentMap.find(instrumentGroup) != that->subscriptionStatusByInstrumentGroupInstrumentMap.end()) {
            const auto& wsConnectionId = wsConnectionIdListByInstrumentGroupMap.at(instrumentGroup).at(0);
            const auto& wsConnectionPtr = that->wsConnectionPtrByIdMap.at(wsConnectionId);
            for (const auto& subscription : subscriptionListGivenInstrumentGroup) {
              const auto& instrument = subscription.getInstrument();
              if (that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup].find(instrument) !=
                  that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup].end()) {
                that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "already subscribed: " + toString(subscription));
                return;
              }
              wsConnectionPtr->subscriptionList.push_back(subscription);
              that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
              that->prepareSubscription(wsConnectionPtr, subscription);
            }
            CCAPI_LOGGER_INFO("about to subscribe to exchange");
            that->subscribeToExchange(wsConnectionPtr);
          } else {
            const auto& splittedInstrumentGroup = UtilString::split(instrumentGroup, "|");
            const auto& url = splittedInstrumentGroup.at(0);
            auto credential = subscriptionListGivenInstrumentGroup.at(0).getCredential();
            if (credential.empty()) {
              credential = that->credentialDefault;
            }
            const auto& proxyUrl = splittedInstrumentGroup.at(splittedInstrumentGroup.size() - 1);

            auto wsConnectionPtr = std::make_shared<WsConnection>(url, instrumentGroup, subscriptionListGivenInstrumentGroup, credential, proxyUrl);
            that->setWsConnectionStream(wsConnectionPtr);
            CCAPI_LOGGER_WARN("about to subscribe with new wsConnectionPtr " + toString(*wsConnectionPtr));
            that->prepareConnect(wsConnectionPtr);
          }
        });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif

  typedef boost::system::error_code ErrorCode;

  std::map<std::string, std::vector<Subscription>> groupSubscriptionListByInstrumentGroup(const std::vector<Subscription>& subscriptionList) {
    std::map<std::string, std::vector<Subscription>> groups;
    for (const auto& subscription : subscriptionList) {
      std::string instrumentGroup = this->getInstrumentGroup(subscription);
      groups[instrumentGroup].push_back(subscription);
    }
    return groups;
  }

  virtual std::string getInstrumentGroup(const Subscription& subscription) {
    const auto& field = subscription.getField();
    if (field == CCAPI_GENERIC_PUBLIC_SUBSCRIPTION) {
      return this->baseUrlWs + "|" + subscription.getField() + "|" + subscription.getCorrelationId() + "|" + subscription.getSerializedCredential() + "|" +
             subscription.getProxyUrl();
    } else {
      return this->baseUrlWs + "|" + subscription.getField() + "|" + subscription.getSerializedOptions() + "|" + subscription.getSerializedCredential() + "|" +
             subscription.getProxyUrl();
    }
  }

  void prepareSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription) {
    auto instrument = subscription.getInstrument();
    CCAPI_LOGGER_TRACE("instrument = " + instrument);
    std::string symbolId = instrument;
    CCAPI_LOGGER_TRACE("symbolId = " + symbolId);
    auto field = subscription.getField();
    CCAPI_LOGGER_TRACE("field = " + field);
    auto optionMap = subscription.getOptionMap();
    CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
    std::string channelId = this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(this->exchangeName).at(field);
    CCAPI_LOGGER_TRACE("channelId = " + channelId);
    CCAPI_LOGGER_TRACE("this->exchangeName = " + this->exchangeName);
    this->prepareSubscriptionDetail(channelId, symbolId, field, wsConnectionPtr, subscription, optionMap);
    CCAPI_LOGGER_TRACE("channelId = " + channelId);
    this->correlationIdListByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId].push_back(subscription.getCorrelationId());
    this->subscriptionListByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId].push_back(subscription);
    this->fieldByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = field;
    this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId].insert(optionMap.begin(), optionMap.end());
    CCAPI_LOGGER_TRACE("this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap = " +
                       toString(this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap));
    CCAPI_LOGGER_TRACE("this->correlationIdListByConnectionIdChannelSymbolIdMap = " + toString(this->correlationIdListByConnectionIdChannelIdSymbolIdMap));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void processMarketDataMessageList(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessage, const TimePoint& timeReceived,
                                    Event& event, std::vector<MarketDataMessage>& marketDataMessageList) {
    CCAPI_LOGGER_TRACE("marketDataMessageList = " + toString(marketDataMessageList));
    event.setType(Event::Type::SUBSCRIPTION_DATA);
    for (auto& marketDataMessage : marketDataMessageList) {
      if (marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH ||
          marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE ||
          marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_AGG_TRADE ||
          marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK) {
        // if (this->sessionOptions.warnLateEventMaxMilliseconds > 0 &&
        //     std::chrono::duration_cast<std::chrono::milliseconds>(timeReceived - marketDataMessage.tp).count() >
        //         this->sessionOptions.warnLateEventMaxMilliseconds &&
        //     marketDataMessage.recapType == MarketDataMessage::RecapType::NONE) {
        //   CCAPI_LOGGER_WARN("late websocket message: timeReceived = " + toString(timeReceived) + ", marketDataMessage.tp = " + toString(marketDataMessage.tp)
        //   +
        //                     ", wsConnection = " + toString(*wsConnectionPtr));
        // }

        std::string& exchangeSubscriptionId = marketDataMessage.exchangeSubscriptionId;
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap = " +
                           toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap));
        CCAPI_LOGGER_TRACE("this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id) = " +
                           toString(this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id)));
        CCAPI_LOGGER_TRACE("exchangeSubscriptionId = " + exchangeSubscriptionId);
        std::string& channelId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
        CCAPI_LOGGER_TRACE("channelId = " + toString(channelId));
        std::string& symbolId =
            this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
        CCAPI_LOGGER_TRACE("symbolId = " + toString(symbolId));
        CCAPI_LOGGER_TRACE("this->fieldByConnectionIdChannelIdSymbolIdMap = " + toString(this->fieldByConnectionIdChannelIdSymbolIdMap));
        CCAPI_LOGGER_TRACE("this->fieldByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id) = " +
                           toString(this->fieldByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)));
        auto& field = this->fieldByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
        CCAPI_LOGGER_TRACE("this->optionMapByConnectionIdChannelIdSymbolIdMap = " + toString(this->optionMapByConnectionIdChannelIdSymbolIdMap));
        CCAPI_LOGGER_TRACE("wsConnection = " + toString(*wsConnectionPtr));
        auto& optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
        CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
        auto& correlationIdList = this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
        CCAPI_LOGGER_TRACE("correlationIdList = " + toString(correlationIdList));
        if (marketDataMessage.data.find(MarketDataMessage::DataType::BID) != marketDataMessage.data.end() ||
            marketDataMessage.data.find(MarketDataMessage::DataType::ASK) != marketDataMessage.data.end()) {
          std::map<Decimal, std::string>& snapshotBid = this->snapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
          std::map<Decimal, std::string>& snapshotAsk = this->snapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
          if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] &&
              marketDataMessage.recapType == MarketDataMessage::RecapType::NONE) {
            this->processOrderBookUpdate(wsConnectionPtr, channelId, symbolId, event, marketDataMessage.tp, timeReceived, marketDataMessage.data, field,
                                         optionMap, correlationIdList, snapshotBid, snapshotAsk);
            if (this->sessionOptions.enableCheckOrderBookChecksum &&
                this->orderBookChecksumByConnectionIdSymbolIdMap.find(wsConnectionPtr->id) != this->orderBookChecksumByConnectionIdSymbolIdMap.end() &&
                this->orderBookChecksumByConnectionIdSymbolIdMap.at(wsConnectionPtr->id).find(symbolId) !=
                    this->orderBookChecksumByConnectionIdSymbolIdMap.at(wsConnectionPtr->id).end()) {
              bool shouldProcessRemainingMessage = true;
              std::string receivedOrderBookChecksumStr = this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnectionPtr->id][symbolId];
              if (!this->checkOrderBookChecksum(snapshotBid, snapshotAsk, receivedOrderBookChecksumStr, shouldProcessRemainingMessage)) {
                CCAPI_LOGGER_ERROR("snapshotBid = " + toString(snapshotBid));
                CCAPI_LOGGER_ERROR("snapshotAsk = " + toString(snapshotAsk));
                this->onIncorrectStatesFound(wsConnectionPtr, textMessage, timeReceived, exchangeSubscriptionId, "order book incorrect checksum found");
              }
              if (!shouldProcessRemainingMessage) {
                return;
              }
            }
            if (this->sessionOptions.enableCheckOrderBookCrossed) {
              bool shouldProcessRemainingMessage = true;
              if (!this->checkOrderBookCrossed(snapshotBid, snapshotAsk, shouldProcessRemainingMessage)) {
                CCAPI_LOGGER_ERROR("lastNToString(snapshotBid, 1) = " + lastNToString(snapshotBid, 1));
                CCAPI_LOGGER_ERROR("firstNToString(snapshotAsk, 1) = " + firstNToString(snapshotAsk, 1));
                this->onIncorrectStatesFound(wsConnectionPtr, textMessage, timeReceived, exchangeSubscriptionId, "order book crossed market found");
              }
              if (!shouldProcessRemainingMessage) {
                return;
              }
            }
          } else if (marketDataMessage.recapType == MarketDataMessage::RecapType::SOLICITED) {
            this->processOrderBookInitial(wsConnectionPtr, channelId, symbolId, event, marketDataMessage.tp, timeReceived, marketDataMessage.data, field,
                                          optionMap, correlationIdList, snapshotBid, snapshotAsk);
          }
          CCAPI_LOGGER_TRACE("snapshotBid.size() = " + toString(snapshotBid.size()));
          CCAPI_LOGGER_TRACE("snapshotAsk.size() = " + toString(snapshotAsk.size()));
        }
        if (marketDataMessage.data.find(MarketDataMessage::DataType::TRADE) != marketDataMessage.data.end() ||
            marketDataMessage.data.find(MarketDataMessage::DataType::AGG_TRADE) != marketDataMessage.data.end()) {
          bool isSolicited = marketDataMessage.recapType == MarketDataMessage::RecapType::SOLICITED;
          this->processTrade(wsConnectionPtr, channelId, symbolId, event, marketDataMessage.tp, timeReceived, marketDataMessage.data, field, optionMap,
                             correlationIdList, isSolicited);
        }
        if (marketDataMessage.data.find(MarketDataMessage::DataType::CANDLESTICK) != marketDataMessage.data.end()) {
          bool isSolicited = marketDataMessage.recapType == MarketDataMessage::RecapType::SOLICITED;
          this->processExchangeProvidedCandlestick(wsConnectionPtr, channelId, symbolId, event, marketDataMessage.tp, timeReceived, marketDataMessage.data,
                                                   field, optionMap, correlationIdList, isSolicited);
        }
      } else {
        CCAPI_LOGGER_WARN("websocket event type is unknown for " + toString(marketDataMessage));
        CCAPI_LOGGER_WARN("textMessage is " + std::string(textMessage));
      }
    }
  }

  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessage, const TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->correlationIdByConnectionIdMap.find(wsConnectionPtr->id) == this->correlationIdByConnectionIdMap.end()) {
      Event event;
      std::vector<MarketDataMessage> marketDataMessageList;
      this->processTextMessage(wsConnectionPtr, textMessage, timeReceived, event, marketDataMessageList);
      if (!marketDataMessageList.empty()) {
        this->processMarketDataMessageList(wsConnectionPtr, textMessage, timeReceived, event, marketDataMessageList);
      }
      if (!event.getMessageList().empty()) {
        this->eventHandler(event, nullptr);
      }
    } else {
      Event event;
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      Message message;
      message.setType(Message::Type::GENERIC_PUBLIC_SUBSCRIPTION);
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({this->correlationIdByConnectionIdMap.at(wsConnectionPtr->id)});
      Element element;
      element.insert(CCAPI_WEBSOCKET_MESSAGE_PAYLOAD, textMessage);
      message.setElementList({element});
      event.setMessageList({message});
      this->eventHandler(event, nullptr);
    }
    this->onPongByMethod(PingPongMethod::WEBSOCKET_APPLICATION_LEVEL, wsConnectionPtr, timeReceived, false);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void onIncorrectStatesFound(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived,
                                      const std::string& exchangeSubscriptionId, std::string const& reason) {
    std::string errorMessage = "incorrect states found: connection = " + toString(*wsConnectionPtr) + ", textMessage = " + std::string(textMessageView) +
                               ", timeReceived = " + UtilTime::getISOTimestamp(timeReceived) + ", exchangeSubscriptionId = " + exchangeSubscriptionId +
                               ", reason = " + reason;
    CCAPI_LOGGER_ERROR(errorMessage);
    ErrorCode ec;
    this->close(wsConnectionPtr, beast::websocket::close_code::normal,
                beast::websocket::close_reason(beast::websocket::close_code::normal, "incorrect states found: " + reason), ec);
    if (ec) {
      std::string& channelId =
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
      std::string& symbolId =
          this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
      CCAPI_LOGGER_TRACE("channelId = " + toString(channelId));
      CCAPI_LOGGER_TRACE("symbolId = " + toString(symbolId));
      auto& correlationIdList = this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
      CCAPI_LOGGER_TRACE("correlationIdList = " + toString(correlationIdList));
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::INCORRECT_STATE_FOUND, "shutdown", correlationIdList);
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnectionPtr->id] = false;
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::INCORRECT_STATE_FOUND, errorMessage);
  }

  void connect(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    Service::connect(wsConnectionPtr);
    this->instrumentGroupByWsConnectionIdMap.insert(std::pair<std::string, std::string>(wsConnectionPtr->id, wsConnectionPtr->group));
    CCAPI_LOGGER_DEBUG("this->instrumentGroupByWsConnectionIdMap = " + toString(this->instrumentGroupByWsConnectionIdMap));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void onOpen(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    Service::onOpen(wsConnectionPtr);
    auto credential = wsConnectionPtr->credential;
    if (!credential.empty()) {
      this->logonToExchange(wsConnectionPtr, now, credential);
    } else {
      this->startSubscribe(wsConnectionPtr);
    }
  }

  void onFail_(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    std::string wsConnectionId = wsConnectionPtr->id;
    Service::onFail_(wsConnectionPtr);
    this->instrumentGroupByWsConnectionIdMap.erase(wsConnectionId);
  }

  void clearStates(std::shared_ptr<WsConnection> wsConnectionPtr) override {
    Service::clearStates(wsConnectionPtr);
    this->fieldByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->optionMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->subscriptionListByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->correlationIdListByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.erase(wsConnectionPtr->id);
    this->snapshotBidByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->snapshotAskByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->processedInitialTradeByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    if (this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) != this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.end()) {
      for (const auto& x : this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id)) {
        for (const auto& y : x.second) {
          y.second->cancel();
        }
      }
      this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    }
    this->openByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->highByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->lowByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->closeByConnectionIdChannelIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->orderBookChecksumByConnectionIdSymbolIdMap.erase(wsConnectionPtr->id);
    this->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap.erase(wsConnectionPtr->id);
    if (this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap.find(wsConnectionPtr->id) !=
        this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap.end()) {
      for (const auto& x : this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id)) {
        x.second->cancel();
      }
      this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap.erase(wsConnectionPtr->id);
    }
    this->orderbookVersionIdByConnectionIdExchangeSubscriptionIdMap.erase(wsConnectionPtr->id);
  }

  virtual void onClose(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode ec) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap.erase(wsConnectionPtr->id);
    this->exchangeJsonPayloadIdByConnectionIdMap.erase(wsConnectionPtr->id);
    this->instrumentGroupByWsConnectionIdMap.erase(wsConnectionPtr->id);
    this->correlationIdByConnectionIdMap.erase(wsConnectionPtr->id);
    Service::onClose(wsConnectionPtr, ec);
  }

  virtual void subscribeToExchange(std::shared_ptr<WsConnection> wsConnectionPtr) {
    CCAPI_LOGGER_INFO("exchange is " + this->exchangeName);
    std::vector<std::string> sendStringList;
    if (this->correlationIdByConnectionIdMap.find(wsConnectionPtr->id) == this->correlationIdByConnectionIdMap.end()) {
      sendStringList = this->createSendStringList(wsConnectionPtr);
    } else {
      auto subscription = wsConnectionPtr->subscriptionList.at(0);
      sendStringList = std::vector<std::string>(1, subscription.getRawOptions());
    }
    for (const auto& sendString : sendStringList) {
      CCAPI_LOGGER_FINE("sendString = " + sendString);
      ErrorCode ec;
      this->send(wsConnectionPtr, sendString, ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    }
  }

  void startSubscribe(std::shared_ptr<WsConnection> wsConnectionPtr) {
    auto instrumentGroup = wsConnectionPtr->group;
    for (const auto& subscription : wsConnectionPtr->subscriptionList) {
      auto instrument = subscription.getInstrument();
      this->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
      if (subscription.getField() == CCAPI_GENERIC_PUBLIC_SUBSCRIPTION) {
        this->correlationIdByConnectionIdMap.insert({wsConnectionPtr->id, subscription.getCorrelationId()});
      } else {
        this->prepareSubscription(wsConnectionPtr, subscription);
      }
    }
    CCAPI_LOGGER_INFO("about to subscribe to exchange");
    this->subscribeToExchange(wsConnectionPtr);
  }

  virtual void logonToExchange(std::shared_ptr<WsConnection> wsConnectionPtr, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    CCAPI_LOGGER_INFO("about to logon to exchange");
    CCAPI_LOGGER_INFO("exchange is " + this->exchangeName);
    auto subscriptionList = wsConnectionPtr->subscriptionList;
    std::vector<std::string> sendStringList = this->createSendStringListFromSubscriptionList(wsConnectionPtr, subscriptionList, now, credential);
    for (const auto& sendString : sendStringList) {
      CCAPI_LOGGER_FINE("sendString = " + sendString);
      ErrorCode ec;
      this->send(wsConnectionPtr, sendString, ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    }
  }

  void updateOrderBook(std::map<Decimal, std::string>& snapshot, const Decimal& price, std::string_view size, bool sizeMayHaveTrailingZero = false) {
    auto it = snapshot.find(price);
    if (it == snapshot.end()) {
      if ((!sizeMayHaveTrailingZero && size != "0") ||
          (sizeMayHaveTrailingZero && ((size.find('.') != std::string::npos && UtilString::rtrim(UtilString::rtrim(size, '0'), '.') != "0") ||
                                       (size.find('.') == std::string::npos && size != "0")))) {
        snapshot.emplace(price, std::string(size));
      }
    } else {
      if ((!sizeMayHaveTrailingZero && size != "0") ||
          (sizeMayHaveTrailingZero && ((size.find('.') != std::string::npos && UtilString::rtrim(UtilString::rtrim(size, '0'), '.') != "0") ||
                                       (size.find('.') == std::string::npos && size != "0")))) {
        it->second = std::string(size);
      } else {
        snapshot.erase(price);
      }
    }
  }

  void updateElementListWithInitialMarketDepth(const std::string& field, const std::map<std::string, std::string>& optionMap,
                                               const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk,
                                               std::vector<Element>& elementList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (field == CCAPI_MARKET_DEPTH) {
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      int bidIndex = 0;
      for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); iter++) {
        if (bidIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_BEST_BID_N_PRICE, ConvertDecimalToString(iter->first));
          element.insert(CCAPI_BEST_BID_N_SIZE, iter->second);
          elementList.emplace_back(std::move(element));
        }
        ++bidIndex;
      }
      if (snapshotBid.empty()) {
        Element element;
        element.insert(CCAPI_BEST_BID_N_PRICE, CCAPI_BEST_BID_N_PRICE_EMPTY);
        element.insert(CCAPI_BEST_BID_N_SIZE, CCAPI_BEST_BID_N_SIZE_EMPTY);
        elementList.emplace_back(std::move(element));
      }
      int askIndex = 0;
      for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); iter++) {
        if (askIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_BEST_ASK_N_PRICE, ConvertDecimalToString(iter->first));
          element.insert(CCAPI_BEST_ASK_N_SIZE, iter->second);
          elementList.emplace_back(std::move(element));
        }
        ++askIndex;
      }
      if (snapshotAsk.empty()) {
        Element element;
        element.insert(CCAPI_BEST_ASK_N_PRICE, CCAPI_BEST_ASK_N_PRICE_EMPTY);
        element.insert(CCAPI_BEST_ASK_N_SIZE, CCAPI_BEST_ASK_N_SIZE_EMPTY);
        elementList.emplace_back(std::move(element));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void updateElementListWithOrderBookSnapshot(const std::string& field, int maxMarketDepth, const std::map<Decimal, std::string>& snapshotBid,
                                              const std::map<Decimal, std::string>& snapshotAsk, std::vector<Element>& elementList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (field == CCAPI_MARKET_DEPTH) {
      int bidIndex = 0;
      for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); iter++) {
        if (bidIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_BEST_BID_N_PRICE, ConvertDecimalToString(iter->first));
          element.insert(CCAPI_BEST_BID_N_SIZE, iter->second);
          elementList.emplace_back(std::move(element));
        }
        ++bidIndex;
      }
      if (snapshotBid.empty()) {
        Element element;
        element.insert(CCAPI_BEST_BID_N_PRICE, CCAPI_BEST_BID_N_PRICE_EMPTY);
        element.insert(CCAPI_BEST_BID_N_SIZE, CCAPI_BEST_BID_N_SIZE_EMPTY);
        elementList.emplace_back(std::move(element));
      }
      int askIndex = 0;
      for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); iter++) {
        if (askIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_BEST_ASK_N_PRICE, ConvertDecimalToString(iter->first));
          element.insert(CCAPI_BEST_ASK_N_SIZE, iter->second);
          elementList.emplace_back(std::move(element));
        }
        ++askIndex;
      }
      if (snapshotAsk.empty()) {
        Element element;
        element.insert(CCAPI_BEST_ASK_N_PRICE, CCAPI_BEST_ASK_N_PRICE_EMPTY);
        element.insert(CCAPI_BEST_ASK_N_SIZE, CCAPI_BEST_ASK_N_SIZE_EMPTY);
        elementList.emplace_back(std::move(element));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  std::map<Decimal, std::string> calculateMarketDepthUpdate(bool isBid, const std::map<Decimal, std::string>& c1, const std::map<Decimal, std::string>& c2,
                                                            int maxMarketDepth) {
    if (c1.empty()) {
      std::map<Decimal, std::string> output;
      for (const auto& x : c2) {
        output.insert(std::make_pair(x.first, "0"));
      }
      return output;
    } else if (c2.empty()) {
      return c1;
    }
    if (isBid) {
      auto it1 = c1.rbegin();
      int i1 = 0;
      auto it2 = c2.rbegin();
      int i2 = 0;
      std::map<Decimal, std::string> output;
      while (i1 < maxMarketDepth && i2 < maxMarketDepth && it1 != c1.rend() && it2 != c2.rend()) {
        if (it1->first > it2->first) {
          output.insert(std::make_pair(it1->first, it1->second));
          ++it1;
          ++i1;
        } else if (it1->first < it2->first) {
          output.insert(std::make_pair(it2->first, "0"));
          ++it2;
          ++i2;
        } else {
          if (it1->second != it2->second) {
            output.insert(std::make_pair(it1->first, it1->second));
          }
          ++it1;
          ++i1;
          ++it2;
          ++i2;
        }
      }
      while (i1 < maxMarketDepth && it1 != c1.rend()) {
        output.insert(std::make_pair(it1->first, it1->second));
        ++it1;
        ++i1;
      }
      while (i2 < maxMarketDepth && it2 != c2.rend()) {
        output.insert(std::make_pair(it2->first, "0"));
        ++it2;
        ++i2;
      }
      return output;
    } else {
      auto it1 = c1.begin();
      int i1 = 0;
      auto it2 = c2.begin();
      int i2 = 0;
      std::map<Decimal, std::string> output;
      while (i1 < maxMarketDepth && i2 < maxMarketDepth && it1 != c1.end() && it2 != c2.end()) {
        if (it1->first < it2->first) {
          output.insert(std::make_pair(it1->first, it1->second));
          ++it1;
          ++i1;
        } else if (it1->first > it2->first) {
          output.insert(std::make_pair(it2->first, "0"));
          ++it2;
          ++i2;
        } else {
          if (it1->second != it2->second) {
            output.insert(std::make_pair(it1->first, it1->second));
          }
          ++it1;
          ++i1;
          ++it2;
          ++i2;
        }
      }
      while (i1 < maxMarketDepth && it1 != c1.end()) {
        output.insert(std::make_pair(it1->first, it1->second));
        ++it1;
        ++i1;
      }
      while (i2 < maxMarketDepth && it2 != c2.end()) {
        output.insert(std::make_pair(it2->first, "0"));
        ++it2;
        ++i2;
      }
      return output;
    }
  }

  void updateElementListWithUpdateMarketDepth(const std::string& field, const std::map<std::string, std::string>& optionMap,
                                              const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotBidPrevious,
                                              const std::map<Decimal, std::string>& snapshotAsk, const std::map<Decimal, std::string>& snapshotAskPrevious,
                                              std::vector<Element>& elementList, bool alwaysUpdate) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (field == CCAPI_MARKET_DEPTH) {
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      if (optionMap.at(CCAPI_MARKET_DEPTH_RETURN_UPDATE) == CCAPI_MARKET_DEPTH_RETURN_UPDATE_ENABLE) {
        CCAPI_LOGGER_TRACE("lastNSame = " + toString(lastNSame(snapshotBid, snapshotBidPrevious, maxMarketDepth)));
        CCAPI_LOGGER_TRACE("firstNSame = " + toString(firstNSame(snapshotAsk, snapshotAskPrevious, maxMarketDepth)));
        std::map<Decimal, std::string> snapshotBidUpdate = this->calculateMarketDepthUpdate(true, snapshotBid, snapshotBidPrevious, maxMarketDepth);
        for (auto& x : snapshotBidUpdate) {
          Element element;
          std::string v1 = ConvertDecimalToString(x.first);
          element.insert(CCAPI_BEST_BID_N_PRICE, v1);
          element.insert(CCAPI_BEST_BID_N_SIZE, x.second);
          elementList.emplace_back(std::move(element));
        }
        std::map<Decimal, std::string> snapshotAskUpdate = this->calculateMarketDepthUpdate(false, snapshotAsk, snapshotAskPrevious, maxMarketDepth);
        for (auto& x : snapshotAskUpdate) {
          Element element;
          std::string v1 = ConvertDecimalToString(x.first);
          element.insert(CCAPI_BEST_ASK_N_PRICE, v1);
          element.insert(CCAPI_BEST_ASK_N_SIZE, x.second);
          elementList.emplace_back(std::move(element));
        }
      } else {
        CCAPI_LOGGER_TRACE("lastNSame = " + toString(lastNSame(snapshotBid, snapshotBidPrevious, maxMarketDepth)));
        CCAPI_LOGGER_TRACE("firstNSame = " + toString(firstNSame(snapshotAsk, snapshotAskPrevious, maxMarketDepth)));
        if (alwaysUpdate || !lastNSame(snapshotBid, snapshotBidPrevious, maxMarketDepth) || !firstNSame(snapshotAsk, snapshotAskPrevious, maxMarketDepth)) {
          int bidIndex = 0;
          for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); ++iter) {
            if (bidIndex >= maxMarketDepth) {
              break;
            }
            Element element;
            element.insert(CCAPI_BEST_BID_N_PRICE, ConvertDecimalToString(iter->first));
            element.insert(CCAPI_BEST_BID_N_SIZE, iter->second);
            elementList.emplace_back(std::move(element));
            ++bidIndex;
          }
          if (snapshotBid.empty()) {
            Element element;
            element.insert(CCAPI_BEST_BID_N_PRICE, CCAPI_BEST_BID_N_PRICE_EMPTY);
            element.insert(CCAPI_BEST_BID_N_SIZE, CCAPI_BEST_BID_N_SIZE_EMPTY);
            elementList.emplace_back(std::move(element));
          }
          int askIndex = 0;
          for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); ++iter) {
            if (askIndex >= maxMarketDepth) {
              break;
            }
            Element element;
            element.insert(CCAPI_BEST_ASK_N_PRICE, ConvertDecimalToString(iter->first));
            element.insert(CCAPI_BEST_ASK_N_SIZE, iter->second);
            elementList.emplace_back(std::move(element));
            ++askIndex;
          }
          if (snapshotAsk.empty()) {
            Element element;
            element.insert(CCAPI_BEST_ASK_N_PRICE, CCAPI_BEST_ASK_N_PRICE_EMPTY);
            element.insert(CCAPI_BEST_ASK_N_SIZE, CCAPI_BEST_ASK_N_SIZE_EMPTY);
            elementList.emplace_back(std::move(element));
          }
        }
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void updateElementListWithTrade(const std::string& field, MarketDataMessage::TypeForData& input, std::vector<Element>& elementList) {
    if (field == CCAPI_TRADE || field == CCAPI_AGG_TRADE) {
      for (auto& x : input) {
        auto& type = x.first;
        auto& detail = x.second;
        if (type == MarketDataMessage::DataType::TRADE || type == MarketDataMessage::DataType::AGG_TRADE) {
          for (auto& y : detail) {
            const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
            const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
            Element element;
            element.insert(CCAPI_LAST_PRICE, price);
            element.insert(CCAPI_LAST_SIZE, size);
            {
              auto it = y.find(MarketDataMessage::DataFieldType::TRADE_ID);
              if (it != y.end()) {
                element.insert(CCAPI_TRADE_ID, it->second);
              }
            }
            {
              auto it = y.find(MarketDataMessage::DataFieldType::AGG_TRADE_ID);
              if (it != y.end()) {
                element.insert(CCAPI_AGG_TRADE_ID, it->second);
              }
            }
            element.insert(CCAPI_IS_BUYER_MAKER, y.at(MarketDataMessage::DataFieldType::IS_BUYER_MAKER));
            {
              auto it = y.find(MarketDataMessage::DataFieldType::SEQUENCE_NUMBER);
              if (it != y.end()) {
                element.insert(CCAPI_SEQUENCE_NUMBER, it->second);
              }
            }
            elementList.emplace_back(std::move(element));
          }
        } else {
          CCAPI_LOGGER_WARN("extra type " + MarketDataMessage::dataTypeToString(type));
        }
      }
    }
  }

  void updateElementListWithExchangeProvidedCandlestick(const std::string& field, MarketDataMessage::TypeForData& input, std::vector<Element>& elementList) {
    if (field == CCAPI_CANDLESTICK) {
      for (auto& x : input) {
        auto& type = x.first;
        auto& detail = x.second;
        if (type == MarketDataMessage::DataType::CANDLESTICK) {
          for (auto& y : detail) {
            Element element;
            element.insert(CCAPI_OPEN_PRICE, y.at(MarketDataMessage::DataFieldType::OPEN_PRICE));
            element.insert(CCAPI_HIGH_PRICE, y.at(MarketDataMessage::DataFieldType::HIGH_PRICE));
            element.insert(CCAPI_LOW_PRICE, y.at(MarketDataMessage::DataFieldType::LOW_PRICE));
            element.insert(CCAPI_CLOSE_PRICE, y.at(MarketDataMessage::DataFieldType::CLOSE_PRICE));
            {
              auto it = y.find(MarketDataMessage::DataFieldType::VOLUME);
              if (it != y.end()) {
                element.insert(CCAPI_VOLUME, it->second);
              }
            }
            {
              auto it = y.find(MarketDataMessage::DataFieldType::QUOTE_VOLUME);
              if (it != y.end()) {
                element.insert(CCAPI_QUOTE_VOLUME, it->second);
              }
            }
            elementList.emplace_back(std::move(element));
          }
        } else {
          CCAPI_LOGGER_WARN("extra type " + MarketDataMessage::dataTypeToString(type));
        }
      }
    }
  }

  void updateElementListWithCalculatedCandlestick(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId,
                                                  const std::string& field, std::vector<Element>& elementList) {
    if (field == CCAPI_TRADE || field == CCAPI_AGG_TRADE) {
      Element element;
      if (this->openByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId].empty()) {
        element.insert(CCAPI_OPEN_PRICE, CCAPI_CANDLESTICK_EMPTY);
        element.insert(CCAPI_HIGH_PRICE, CCAPI_CANDLESTICK_EMPTY);
        element.insert(CCAPI_LOW_PRICE, CCAPI_CANDLESTICK_EMPTY);
        element.insert(CCAPI_CLOSE_PRICE, CCAPI_CANDLESTICK_EMPTY);
      } else {
        element.insert(CCAPI_OPEN_PRICE, this->openByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]);
        element.insert(CCAPI_HIGH_PRICE, ConvertDecimalToString(this->highByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]));
        element.insert(CCAPI_LOW_PRICE, ConvertDecimalToString(this->lowByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]));
        element.insert(CCAPI_CLOSE_PRICE, this->closeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]);
      }
      elementList.emplace_back(std::move(element));
      this->openByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = "";
      this->highByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = Decimal();
      this->lowByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = Decimal();
      this->closeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = "";
    }
  }

  void copySnapshot(bool isBid, const std::map<Decimal, std::string>& original, std::map<Decimal, std::string>& copy, const int maxMarketDepth) {
    size_t nToCopy = std::min(original.size(), static_cast<size_t>(maxMarketDepth));
    if (isBid) {
      std::copy_n(original.rbegin(), nToCopy, std::inserter(copy, copy.end()));
    } else {
      std::copy_n(original.begin(), nToCopy, std::inserter(copy, copy.end()));
    }
  }

  void processOrderBookInitial(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId, Event& event,
                               const TimePoint& tp, const TimePoint& timeReceived, MarketDataMessage::TypeForData& input, const std::string& field,
                               const std::map<std::string, std::string>& optionMap, const std::vector<std::string>& correlationIdList,
                               std::map<Decimal, std::string>& snapshotBid, std::map<Decimal, std::string>& snapshotAsk) {
    snapshotBid.clear();
    snapshotAsk.clear();
    int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    for (auto& x : input) {
      auto& type = x.first;
      auto& detail = x.second;
      if (type == MarketDataMessage::DataType::BID) {
        for (auto& y : detail) {
          const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
          const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
          Decimal decimalPrice(price);
          snapshotBid.emplace(decimalPrice, std::string(size));
        }
        CCAPI_LOGGER_TRACE("lastNToString(snapshotBid, " + toString(maxMarketDepth) + ") = " + lastNToString(snapshotBid, maxMarketDepth));
      } else if (type == MarketDataMessage::DataType::ASK) {
        for (auto& y : detail) {
          const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
          const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
          Decimal decimalPrice(price);
          snapshotAsk.emplace(decimalPrice, std::string(size));
        }
        CCAPI_LOGGER_TRACE("firstNToString(snapshotAsk, " + toString(maxMarketDepth) + ") = " + firstNToString(snapshotAsk, maxMarketDepth));
      } else {
        CCAPI_LOGGER_WARN("extra type " + MarketDataMessage::dataTypeToString(type));
      }
    }
    std::vector<Element> elementList;
    this->updateElementListWithInitialMarketDepth(field, optionMap, snapshotBid, snapshotAsk, elementList);
    if (!elementList.empty()) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH);
      message.setRecapType(Message::RecapType::SOLICITED);
      message.setTime(tp);
      message.setElementList(elementList);
      message.setCorrelationIdList(correlationIdList);
      std::vector<Message> newMessageList = {message};
      event.addMessages(newMessageList);
      CCAPI_LOGGER_TRACE("event.getMessageList() = " + toString(event.getMessageList()));
    }
    this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
    bool shouldConflate = optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
    if (shouldConflate) {
      this->copySnapshot(true, snapshotBid, this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId],
                         maxMarketDepth);
      this->copySnapshot(false, snapshotAsk, this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId],
                         maxMarketDepth);
      CCAPI_LOGGER_TRACE(
          "this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at("
          "symbolId) = " +
          toString(this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId)));
      CCAPI_LOGGER_TRACE(
          "this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at("
          "symbolId) = " +
          toString(this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId)));
      TimePoint previousConflateTp = UtilTime::makeTimePointFromMilliseconds(
          std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() / std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)) *
          std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)));
      this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = previousConflateTp;
      if (optionMap.at(CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS) != CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS_DEFAULT) {
        auto interval = std::chrono::milliseconds(std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)));
        auto gracePeriod = std::chrono::milliseconds(std::stoi(optionMap.at(CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS)));
        CCAPI_LOGGER_TRACE("about to set conflate timer");
        this->setConflateTimer(previousConflateTp, interval, gracePeriod, wsConnectionPtr, channelId, symbolId, field, optionMap, correlationIdList);
      }
    }
  }

  void processOrderBookUpdate(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId, Event& event,
                              const TimePoint& tp, const TimePoint& timeReceived, MarketDataMessage::TypeForData& input, const std::string& field,
                              const std::map<std::string, std::string>& optionMap, const std::vector<std::string>& correlationIdList,
                              std::map<Decimal, std::string>& snapshotBid, std::map<Decimal, std::string>& snapshotAsk) {
    CCAPI_LOGGER_TRACE("input = " + MarketDataMessage::dataToString(input));
    if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
      std::vector<Message> messageList;
      CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      std::map<Decimal, std::string> snapshotBidPrevious;
      this->copySnapshot(true, snapshotBid, snapshotBidPrevious, maxMarketDepth);
      std::map<Decimal, std::string> snapshotAskPrevious;
      this->copySnapshot(false, snapshotAsk, snapshotAskPrevious, maxMarketDepth);
      CCAPI_LOGGER_TRACE("before updating orderbook");
      CCAPI_LOGGER_TRACE("lastNToString(snapshotBid, " + toString(maxMarketDepth) + ") = " + lastNToString(snapshotBid, maxMarketDepth));
      CCAPI_LOGGER_TRACE("firstNToString(snapshotAsk, " + toString(maxMarketDepth) + ") = " + firstNToString(snapshotAsk, maxMarketDepth));
      if (this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
        CCAPI_LOGGER_TRACE("l2Update is replace");
        if (input.find(MarketDataMessage::DataType::BID) != input.end()) {
          snapshotBid.clear();
        }
        if (input.find(MarketDataMessage::DataType::ASK) != input.end()) {
          snapshotAsk.clear();
        }
      }
      for (auto& x : input) {
        auto& type = x.first;
        auto& detail = x.second;
        if (type == MarketDataMessage::DataType::BID) {
          for (auto& y : detail) {
            const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
            const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
            Decimal decimalPrice(price);
            this->updateOrderBook(snapshotBid, decimalPrice, size, this->sessionOptions.enableCheckOrderBookChecksum);
          }
        } else if (type == MarketDataMessage::DataType::ASK) {
          for (auto& y : detail) {
            const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
            const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
            Decimal decimalPrice(price);
            this->updateOrderBook(snapshotAsk, decimalPrice, size, this->sessionOptions.enableCheckOrderBookChecksum);
          }
        } else {
          CCAPI_LOGGER_WARN("extra type " + MarketDataMessage::dataTypeToString(type));
        }
      }
      CCAPI_LOGGER_TRACE("this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap = " +
                         toString(this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap));
      if (this->shouldAlignSnapshot) {
        int marketDepthSubscribedToExchange =
            this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
        this->alignSnapshot(snapshotBid, snapshotAsk, marketDepthSubscribedToExchange);
      }
      CCAPI_LOGGER_TRACE("after updating orderbook");
      CCAPI_LOGGER_TRACE("lastNToString(snapshotBid, " + toString(maxMarketDepth) + ") = " + lastNToString(snapshotBid, maxMarketDepth));
      CCAPI_LOGGER_TRACE("firstNToString(snapshotAsk, " + toString(maxMarketDepth) + ") = " + firstNToString(snapshotAsk, maxMarketDepth));
      CCAPI_LOGGER_TRACE("lastNToString(snapshotBidPrevious, " + toString(maxMarketDepth) + ") = " + lastNToString(snapshotBidPrevious, maxMarketDepth));
      CCAPI_LOGGER_TRACE("firstNToString(snapshotAskPrevious, " + toString(maxMarketDepth) + ") = " + firstNToString(snapshotAskPrevious, maxMarketDepth));
      CCAPI_LOGGER_TRACE("field = " + toString(field));
      CCAPI_LOGGER_TRACE("maxMarketDepth = " + toString(maxMarketDepth));
      CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
      bool shouldConflate = optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
      CCAPI_LOGGER_TRACE("shouldConflate = " + toString(shouldConflate));
      TimePoint conflateTp =
          shouldConflate ? UtilTime::makeTimePointFromMilliseconds(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() /
                                                                   std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)) *
                                                                   std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)))
                         : tp;
      CCAPI_LOGGER_TRACE("conflateTp = " + toString(conflateTp));
      bool intervalChanged =
          shouldConflate && conflateTp > this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
      CCAPI_LOGGER_TRACE("intervalChanged = " + toString(intervalChanged));
      if (!shouldConflate || intervalChanged) {
        std::vector<Element> elementList;
        if (shouldConflate && intervalChanged) {
          const std::map<Decimal, std::string>& snapshotBidPreviousPrevious =
              this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          const std::map<Decimal, std::string>& snapshotAskPreviousPrevious =
              this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
          this->updateElementListWithUpdateMarketDepth(field, optionMap, snapshotBidPrevious, snapshotBidPreviousPrevious, snapshotAskPrevious,
                                                       snapshotAskPreviousPrevious, elementList, false);
          this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) = snapshotBidPrevious;
          this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) = snapshotAskPrevious;
          CCAPI_LOGGER_TRACE(
              "this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at("
              "symbolId) = " +
              toString(this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId)));
          CCAPI_LOGGER_TRACE(
              "this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at("
              "symbolId) = " +
              toString(this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId)));
        } else {
          this->updateElementListWithUpdateMarketDepth(field, optionMap, snapshotBid, snapshotBidPrevious, snapshotAsk, snapshotAskPrevious, elementList,
                                                       false);
        }
        CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
        if (!elementList.empty()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH);
          message.setRecapType(Message::RecapType::NONE);
          TimePoint time = shouldConflate ? this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) +
                                                std::chrono::milliseconds(std::stoll(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)))
                                          : conflateTp;
          message.setTime(time);
          message.setElementList(elementList);
          message.setCorrelationIdList(correlationIdList);
          messageList.emplace_back(std::move(message));
        }
        if (!messageList.empty()) {
          event.addMessages(messageList);
        }
        if (shouldConflate) {
          this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) = conflateTp;
        }
      }
    }
  }

  void processTrade(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId, Event& event, const TimePoint& tp,
                    const TimePoint& timeReceived, MarketDataMessage::TypeForData& input, const std::string& field,
                    const std::map<std::string, std::string>& optionMap, const std::vector<std::string>& correlationIdList, bool isSolicited) {
    CCAPI_LOGGER_TRACE("input = " + MarketDataMessage::dataToString(input));
    CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
    bool shouldConflate = optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
    CCAPI_LOGGER_TRACE("shouldConflate = " + toString(shouldConflate));
    TimePoint conflateTp = shouldConflate
                               ? UtilTime::makeTimePointFromMilliseconds(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() /
                                                                         std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)) *
                                                                         std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)))
                               : tp;
    CCAPI_LOGGER_TRACE("conflateTp = " + toString(conflateTp));
    if (!this->processedInitialTradeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
      if (shouldConflate) {
        TimePoint previousConflateTp = conflateTp;
        this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = previousConflateTp;
        if (optionMap.at(CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS) != CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS_DEFAULT) {
          auto interval = std::chrono::milliseconds(std::stoi(optionMap.at(CCAPI_CONFLATE_INTERVAL_MILLISECONDS)));
          auto gracePeriod = std::chrono::milliseconds(std::stoi(optionMap.at(CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS)));
          CCAPI_LOGGER_TRACE("about to set conflate timer");
          this->setConflateTimer(previousConflateTp, interval, gracePeriod, wsConnectionPtr, channelId, symbolId, field, optionMap, correlationIdList);
        }
      }
      this->processedInitialTradeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
    }
    bool intervalChanged =
        shouldConflate && conflateTp > this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
    CCAPI_LOGGER_TRACE("intervalChanged = " + toString(intervalChanged));
    if (!shouldConflate || intervalChanged) {
      std::vector<Message> messageList;
      std::vector<Element> elementList;
      if (shouldConflate && intervalChanged) {
        this->updateElementListWithCalculatedCandlestick(wsConnectionPtr, channelId, symbolId, field, elementList);
      } else {
        this->updateElementListWithTrade(field, input, elementList);
      }
      CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
      if (!elementList.empty()) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(field == CCAPI_TRADE ? Message::Type::MARKET_DATA_EVENTS_TRADE : Message::Type::MARKET_DATA_EVENTS_AGG_TRADE);
        message.setRecapType(isSolicited ? Message::RecapType::SOLICITED : Message::RecapType::NONE);
        TimePoint time =
            shouldConflate ? this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) : conflateTp;
        message.setTime(time);
        message.setElementList(elementList);
        message.setCorrelationIdList(correlationIdList);
        messageList.emplace_back(std::move(message));
      }
      if (!messageList.empty()) {
        event.addMessages(messageList);
      }
      if (shouldConflate) {
        this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) = conflateTp;
        this->updateCalculatedCandlestick(wsConnectionPtr, channelId, symbolId, field, input);
      }
    } else {
      this->updateCalculatedCandlestick(wsConnectionPtr, channelId, symbolId, field, input);
    }
  }

  void processExchangeProvidedCandlestick(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId,
                                          Event& event, const TimePoint& tp, const TimePoint& timeReceived, MarketDataMessage::TypeForData& input,
                                          const std::string& field, const std::map<std::string, std::string>& optionMap,
                                          const std::vector<std::string>& correlationIdList, bool isSolicited) {
    std::vector<Message> messageList;
    std::vector<Element> elementList;
    this->updateElementListWithExchangeProvidedCandlestick(field, input, elementList);
    if (!elementList.empty()) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::MARKET_DATA_EVENTS_CANDLESTICK);
      message.setRecapType(isSolicited ? Message::RecapType::SOLICITED : Message::RecapType::NONE);
      message.setTime(tp);
      message.setElementList(elementList);
      message.setCorrelationIdList(correlationIdList);
      messageList.emplace_back(std::move(message));
    }
    if (!messageList.empty()) {
      event.addMessages(messageList);
    }
  }

  void processExchangeProvidedCandlestick(Event& event, const TimePoint& tp, const TimePoint& timeReceived, MarketDataMessage::TypeForData& input,
                                          const std::vector<std::string>& correlationIdList, Message::Type messageType) {
    std::vector<Message> messageList;
    std::vector<Element> elementList;
    this->updateElementListWithExchangeProvidedCandlestick(CCAPI_CANDLESTICK, input, elementList);
    if (!elementList.empty()) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::MARKET_DATA_EVENTS_CANDLESTICK);
      message.setTime(tp);
      message.setElementList(elementList);
      message.setCorrelationIdList(correlationIdList);
      messageList.emplace_back(std::move(message));
    }
    if (!messageList.empty()) {
      event.addMessages(messageList);
    }
  }

  void updateCalculatedCandlestick(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId,
                                   const std::string& field, const MarketDataMessage::TypeForData& input) {
    if (field == CCAPI_TRADE || field == CCAPI_AGG_TRADE) {
      for (const auto& x : input) {
        auto type = x.first;
        auto detail = x.second;
        if (type == MarketDataMessage::DataType::TRADE || type == MarketDataMessage::DataType::AGG_TRADE) {
          for (const auto& y : detail) {
            auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
            if (this->openByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId].empty()) {
              this->openByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = price;
              this->highByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = Decimal(price);
              this->lowByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = Decimal(price);
            } else {
              Decimal decimalPrice(price);
              if (decimalPrice > this->highByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
                this->highByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = decimalPrice;
              }
              if (decimalPrice < this->lowByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
                this->lowByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = decimalPrice;
              }
            }
            this->closeByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = price;
          }
        } else {
          CCAPI_LOGGER_WARN("extra type " + MarketDataMessage::dataTypeToString(type));
        }
      }
    }
  }

  virtual void alignSnapshot(std::map<Decimal, std::string>& snapshotBid, std::map<Decimal, std::string>& snapshotAsk, int marketDepthSubscribedToExchange) {
    CCAPI_LOGGER_TRACE("snapshotBid.size() = " + toString(snapshotBid.size()));
    if (snapshotBid.size() > marketDepthSubscribedToExchange) {
      keepLastN(snapshotBid, marketDepthSubscribedToExchange);
    }
    CCAPI_LOGGER_TRACE("snapshotBid.size() = " + toString(snapshotBid.size()));
    CCAPI_LOGGER_TRACE("snapshotAsk.size() = " + toString(snapshotAsk.size()));
    if (snapshotAsk.size() > marketDepthSubscribedToExchange) {
      keepFirstN(snapshotAsk, marketDepthSubscribedToExchange);
    }
    CCAPI_LOGGER_TRACE("snapshotAsk.size() = " + toString(snapshotAsk.size()));
  }

  virtual bool checkOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk,
                                      const std::string& receivedOrderBookChecksumStr, bool& shouldProcessRemainingMessage) {
    if (this->sessionOptions.enableCheckOrderBookChecksum) {
      std::string calculatedOrderBookChecksumStr = this->calculateOrderBookChecksum(snapshotBid, snapshotAsk);
      if (!calculatedOrderBookChecksumStr.empty() && calculatedOrderBookChecksumStr != receivedOrderBookChecksumStr) {
        shouldProcessRemainingMessage = false;
        CCAPI_LOGGER_ERROR("calculatedOrderBookChecksumStr = " + calculatedOrderBookChecksumStr);
        CCAPI_LOGGER_ERROR("receivedOrderBookChecksumStr = " + receivedOrderBookChecksumStr);
        CCAPI_LOGGER_ERROR("snapshotBid = " + toString(snapshotBid));
        CCAPI_LOGGER_ERROR("snapshotAsk = " + toString(snapshotAsk));
        return false;
      } else {
        CCAPI_LOGGER_DEBUG("calculatedOrderBookChecksumStr = " + calculatedOrderBookChecksumStr);
        CCAPI_LOGGER_DEBUG("receivedOrderBookChecksumStr = " + receivedOrderBookChecksumStr);
      }
    }
    return true;
  }

  virtual bool checkOrderBookCrossed(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk,
                                     bool& shouldProcessRemainingMessage) {
    if (this->sessionOptions.enableCheckOrderBookCrossed) {
      auto i1 = snapshotBid.rbegin();
      auto i2 = snapshotAsk.begin();
      if (i1 != snapshotBid.rend() && i2 != snapshotAsk.end()) {
        auto& bid = i1->first;
        auto& ask = i2->first;
        if (bid >= ask) {
          CCAPI_LOGGER_ERROR("bid = " + ConvertDecimalToString(bid));
          CCAPI_LOGGER_ERROR("ask = " + ConvertDecimalToString(ask));
          shouldProcessRemainingMessage = false;
          return false;
        }
      }
    }
    return true;
  }

  int calculateMarketDepthAllowedByExchange(int depthWanted, std::vector<int> availableMarketDepth) {
    int i = ceilSearch(availableMarketDepth, 0, availableMarketDepth.size(), depthWanted);
    if (i < 0) {
      i = availableMarketDepth.size() - 1;
    }
    return availableMarketDepth[i];
  }

  Message::Type convertFieldToMessageType(std::string field) {
    if (field == CCAPI_MARKET_DEPTH) {
      return Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH;
    } else if (field == CCAPI_TRADE) {
      return Message::Type::MARKET_DATA_EVENTS_TRADE;
    } else if (field == CCAPI_AGG_TRADE) {
      return Message::Type::MARKET_DATA_EVENTS_AGG_TRADE;
    } else {
      return Message::Type::UNKNOWN;
    }
  }

  void setConflateTimer(const TimePoint& previousConflateTp, const std::chrono::milliseconds& interval, const std::chrono::milliseconds& gracePeriod,
                        std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId, const std::string& symbolId, const std::string& field,
                        const std::map<std::string, std::string>& optionMap, const std::vector<std::string>& correlationIdList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (wsConnectionPtr->status == WsConnection::Status::OPEN) {
      if (this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.find(wsConnectionPtr->id) !=
              this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.end() &&
          this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id].find(channelId) !=
              this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id].end() &&
          this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId].find(symbolId) !=
              this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId].end()) {
        this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]->cancel();
      }
      long waitMilliseconds =
          std::chrono::duration_cast<std::chrono::milliseconds>(previousConflateTp + interval + gracePeriod - std::chrono::system_clock::now()).count();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(interval + gracePeriod).count() > 0) {
        while (waitMilliseconds <= 0) {
          waitMilliseconds += std::chrono::duration_cast<std::chrono::milliseconds>(interval + gracePeriod).count();
        }
      }
      if (waitMilliseconds > 0) {
        TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(waitMilliseconds)));
        timerPtr->async_wait([wsConnectionPtr, channelId, symbolId, field, optionMap, correlationIdList, previousConflateTp, interval, gracePeriod,
                              this](ErrorCode const& ec) {
          if (this->wsConnectionPtrByIdMap.find(wsConnectionPtr->id) != this->wsConnectionPtrByIdMap.end()) {
            if (ec) {
              if (ec != boost::asio::error::operation_aborted) {
                CCAPI_LOGGER_ERROR("wsConnection = " + toString(*wsConnectionPtr) + ", conflate timer error: " + ec.message());
                this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
              }
            } else {
              if (

                  this->wsConnectionPtrByIdMap.at(wsConnectionPtr->id)->status == WsConnection::Status::OPEN

              ) {
                auto conflateTp = previousConflateTp + interval;
                if (conflateTp > this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId)) {
                  Event event;
                  event.setType(Event::Type::SUBSCRIPTION_DATA);
                  std::vector<Element> elementList;
                  if (field == CCAPI_MARKET_DEPTH) {
                    std::map<Decimal, std::string>& snapshotBid = this->snapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
                    std::map<Decimal, std::string>& snapshotAsk = this->snapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
                    this->updateElementListWithUpdateMarketDepth(field, optionMap, snapshotBid, std::map<Decimal, std::string>(), snapshotAsk,
                                                                 std::map<Decimal, std::string>(), elementList, true);
                  } else if (field == CCAPI_TRADE || field == CCAPI_AGG_TRADE) {
                    this->updateElementListWithCalculatedCandlestick(wsConnectionPtr, channelId, symbolId, field, elementList);
                  }
                  CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
                  this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId) = conflateTp;
                  std::vector<Message> messageList;
                  if (!elementList.empty()) {
                    Message message;
                    message.setTimeReceived(conflateTp);
                    message.setType(this->convertFieldToMessageType(field));
                    message.setRecapType(Message::RecapType::NONE);
                    message.setTime(field == CCAPI_MARKET_DEPTH ? conflateTp : previousConflateTp);
                    message.setElementList(elementList);
                    message.setCorrelationIdList(correlationIdList);
                    messageList.emplace_back(std::move(message));
                  }
                  if (!messageList.empty()) {
                    event.addMessages(messageList);
                    this->eventHandler(event, nullptr);
                  }
                }
                auto now = UtilTime::now();
                while (conflateTp + interval + gracePeriod <= now) {
                  conflateTp += interval;
                }
                CCAPI_LOGGER_TRACE("about to set conflate timer");
                this->setConflateTimer(conflateTp, interval, gracePeriod, wsConnectionPtr, channelId, symbolId, field, optionMap, correlationIdList);
              }
            }
          }
        });
        this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = timerPtr;
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void processSuccessfulTextMessageRest(int statusCode, const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived,
                                        Queue<Event>* eventQueuePtr) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    Event event;
    if (this->doesHttpBodyContainError(textMessageView)) {
      event.setType(Event::Type::RESPONSE);
      Message message;
      message.setType(Message::Type::RESPONSE_ERROR);
      message.setTimeReceived(timeReceived);
      message.setCorrelationIdList({request.getCorrelationId()});
      Element element;
      element.insert(CCAPI_HTTP_STATUS_CODE, "200");
      element.insert(CCAPI_ERROR_MESSAGE, UtilString::trim(std::string(textMessageView)));
      message.setElementList({element});
      event.setMessageList({message});
    } else {
      event.setType(Event::Type::RESPONSE);
      if (request.getOperation() == Request::Operation::GENERIC_PUBLIC_REQUEST) {
        Message message;
        message.setTimeReceived(timeReceived);
        message.setType(Message::Type::GENERIC_PUBLIC_REQUEST);
        Element element;
        element.insert(CCAPI_HTTP_STATUS_CODE, std::to_string(statusCode));
        element.insert(CCAPI_HTTP_BODY, textMessageView);
        message.setElementList({element});
        const std::vector<std::string>& correlationIdList = {request.getCorrelationId()};
        CCAPI_LOGGER_TRACE("correlationIdList = " + toString(correlationIdList));
        message.setCorrelationIdList(correlationIdList);
        event.addMessages({message});
      } else {
        std::vector<MarketDataMessage> marketDataMessageList;
        this->convertTextMessageToMarketDataMessage(request, textMessageView, timeReceived, event, marketDataMessageList);
        if (!marketDataMessageList.empty()) {
          this->processMarketDataMessageList(request, textMessageView, timeReceived, event, marketDataMessageList);
        }
      }
    }
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, eventQueuePtr);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void processMarketDataMessageList(const Request& request, const std::string& textMessage, const TimePoint& timeReceived, Event& event,
                                    std::vector<MarketDataMessage>& marketDataMessageList) {
    CCAPI_LOGGER_TRACE("marketDataMessageList = " + toString(marketDataMessageList));
    for (auto& marketDataMessage : marketDataMessageList) {
      if (marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_MARKET_DEPTH ||
          marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_TRADE ||
          marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_AGG_TRADE ||
          marketDataMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS_CANDLESTICK) {
        const std::vector<std::string>& correlationIdList = {request.getCorrelationId()};
        CCAPI_LOGGER_TRACE("correlationIdList = " + toString(correlationIdList));
        if (marketDataMessage.data.find(MarketDataMessage::DataType::BID) != marketDataMessage.data.end() ||
            marketDataMessage.data.find(MarketDataMessage::DataType::ASK) != marketDataMessage.data.end()) {
          auto messageType = this->requestOperationToMessageTypeMap.at(request.getOperation());
          const auto& param = request.getFirstParamWithDefault();
          const auto& maxMarketDepthStr = mapGetWithDefault(param, std::string(CCAPI_MARKET_DEPTH_MAX));
          int maxMarketDepth = maxMarketDepthStr.empty() ? INT_MAX : std::stoi(maxMarketDepthStr);
          this->processOrderBookSnapshot(event, marketDataMessage.tp, timeReceived, marketDataMessage.data, correlationIdList, messageType, maxMarketDepth);
        } else if (marketDataMessage.data.find(MarketDataMessage::DataType::TRADE) != marketDataMessage.data.end() ||
                   marketDataMessage.data.find(MarketDataMessage::DataType::AGG_TRADE) != marketDataMessage.data.end()) {
          auto messageType = this->requestOperationToMessageTypeMap.at(request.getOperation());
          this->processTrade(event, marketDataMessage.tp, timeReceived, marketDataMessage.data, correlationIdList, messageType);
        } else if (marketDataMessage.data.find(MarketDataMessage::DataType::CANDLESTICK) != marketDataMessage.data.end()) {
          auto messageType = this->requestOperationToMessageTypeMap.at(request.getOperation());
          this->processExchangeProvidedCandlestick(event, marketDataMessage.tp, timeReceived, marketDataMessage.data, correlationIdList, messageType);
        }
      } else {
        CCAPI_LOGGER_WARN("market data event type is unknown!");
      }
    }
    CCAPI_LOGGER_TRACE("event type is " + event.typeToString(event.getType()));
  }

  void processTrade(Event& event, const TimePoint& tp, const TimePoint& timeReceived, MarketDataMessage::TypeForData& input,
                    const std::vector<std::string>& correlationIdList, Message::Type messageType) {
    std::vector<Message> messageList;
    std::vector<Element> elementList;
    this->updateElementListWithTrade(CCAPI_TRADE, input, elementList);
    CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
    Message message;
    message.setTimeReceived(timeReceived);
    message.setType(messageType);
    message.setTime(tp);
    message.setElementList(elementList);
    message.setCorrelationIdList(correlationIdList);
    messageList.emplace_back(std::move(message));
    event.addMessages(messageList);
  }

  void processOrderBookSnapshot(Event& event, const TimePoint& tp, const TimePoint& timeReceived, MarketDataMessage::TypeForData& input,
                                const std::vector<std::string>& correlationIdList, Message::Type messageType, int maxMarketDepth) {
    std::vector<Message> messageList;
    std::vector<Element> elementList;
    std::map<Decimal, std::string> snapshotBid, snapshotAsk;
    for (auto& x : input) {
      auto& type = x.first;
      auto& detail = x.second;
      if (type == MarketDataMessage::DataType::BID) {
        for (auto& y : detail) {
          const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
          const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
          Decimal decimalPrice(price);
          snapshotBid.emplace(decimalPrice, std::string(size));
        }
        CCAPI_LOGGER_TRACE("lastNToString(snapshotBid, " + toString(maxMarketDepth) + ") = " + lastNToString(snapshotBid, maxMarketDepth));
      } else if (type == MarketDataMessage::DataType::ASK) {
        for (auto& y : detail) {
          const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
          const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
          Decimal decimalPrice(price);
          snapshotAsk.emplace(decimalPrice, std::string(size));
        }
        CCAPI_LOGGER_TRACE("firstNToString(snapshotAsk, " + toString(maxMarketDepth) + ") = " + firstNToString(snapshotAsk, maxMarketDepth));
      } else {
        CCAPI_LOGGER_WARN("extra type " + MarketDataMessage::dataTypeToString(type));
      }
    }
    this->updateElementListWithOrderBookSnapshot(CCAPI_MARKET_DEPTH, maxMarketDepth, snapshotBid, snapshotAsk, elementList);
    CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
    Message message;
    message.setTimeReceived(timeReceived);
    message.setType(messageType);
    message.setTime(tp);
    message.setElementList(elementList);
    message.setCorrelationIdList(correlationIdList);
    messageList.emplace_back(std::move(message));
    event.addMessages(messageList);
  }

  void convertRequestForRestGenericPublicRequest(http::request<http::string_body>& req, const Request& request, const TimePoint& now,
                                                 const std::string& symbolId, const std::map<std::string, std::string>& credential) {
    const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
    auto methodString = mapGetWithDefault(param, std::string(CCAPI_HTTP_METHOD));
    CCAPI_LOGGER_TRACE("methodString = " + methodString);
    req.method(this->convertHttpMethodStringToMethod(methodString));
    auto path = mapGetWithDefault(param, std::string(CCAPI_HTTP_PATH));
    CCAPI_LOGGER_TRACE("path = " + path);
    auto queryString = mapGetWithDefault(param, std::string(CCAPI_HTTP_QUERY_STRING));
    CCAPI_LOGGER_TRACE("queryString = " + queryString);
    auto target = path;
    if (!queryString.empty()) {
      target += "?" + queryString;
    }
    req.target(target);
    auto body = mapGetWithDefault(param, std::string(CCAPI_HTTP_BODY));
    CCAPI_LOGGER_TRACE("body = " + body);
    if (!body.empty()) {
      req.body() = body;
      req.prepare_payload();
    }
  }

  void processOrderBookWithVersionId(int64_t versionId, std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& channelId,
                                     const std::string& symbolId, const std::string& exchangeSubscriptionId,
                                     const std::map<std::string, std::string>& optionMap, std::vector<MarketDataMessage>& marketDataMessageList,
                                     const MarketDataMessage& marketDataMessage) {
    if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId]) {
      if (versionId > this->orderbookVersionIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnectionPtr->id).at(exchangeSubscriptionId)) {
        marketDataMessageList.emplace_back(std::move(marketDataMessage));
        this->orderbookVersionIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId] = versionId;
      }
    } else {
      if (this->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap[wsConnectionPtr->id][exchangeSubscriptionId].empty()) {
        int delayMilliseconds = std::stoi(optionMap.at(CCAPI_FETCH_MARKET_DEPTH_INITIAL_SNAPSHOT_DELAY_MILLISECONDS));
        if (delayMilliseconds > 0) {
          TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(delayMilliseconds)));
          timerPtr->async_wait([wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds, that = this](ErrorCode const& ec) {
            if (ec) {
              that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
            } else {
              that->buildOrderBookInitial(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
            }
          });
          this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId] = timerPtr;
        } else {
          this->buildOrderBookInitial(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
        }
      }
      this->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][versionId] =
          MarketDataMessage::ConvertDataToOwingData(marketDataMessage.data);
    }
  }

  void buildOrderBookInitialOnFail(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& exchangeSubscriptionId, long delayMilliseconds) {
    auto thisDelayMilliseconds = delayMilliseconds * 2;
    if (thisDelayMilliseconds > 0) {
      TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(thisDelayMilliseconds)));
      timerPtr->async_wait([wsConnectionPtr, exchangeSubscriptionId, thisDelayMilliseconds, that = this](ErrorCode const& ec) {
        if (ec) {
          that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
        } else {
          that->buildOrderBookInitial(wsConnectionPtr, exchangeSubscriptionId, thisDelayMilliseconds);
        }
      });
      this->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId] = timerPtr;
    } else {
      this->buildOrderBookInitial(wsConnectionPtr, exchangeSubscriptionId, thisDelayMilliseconds);
    }
  }

  void buildOrderBookInitial(std::shared_ptr<WsConnection> wsConnectionPtr, const std::string& exchangeSubscriptionId, long delayMilliseconds) {
    auto now = UtilTime::now();
    http::request<http::string_body> req;
    std::string symbolId = this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
    auto credential = wsConnectionPtr->credential;
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->createFetchOrderBookInitialReq(req, symbolId, now, credential);
    this->sendRequest(
        req,
        [wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds, that = shared_from_base<MarketDataService>()](const beast::error_code& ec) {
          that->buildOrderBookInitialOnFail(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
        },
        [wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds,
         that = shared_from_base<MarketDataService>()](const http::response<http::string_body>& res) {
          auto timeReceived = UtilTime::now();
          int statusCode = res.result_int();
          boost::beast::string_view bodyView(res.body());
          if (statusCode / 100 == 2 && !that->doesHttpBodyContainError(bodyView)) {
            try {
              that->jsonDocumentAllocator.Clear();
              rj::Document document(&that->jsonDocumentAllocator);
              document.Parse<rj::kParseNumbersAsStringsFlag>(bodyView.data(), bodyView.size());
              int64_t versionId;
              that->extractOrderBookInitialVersionId(versionId, document);
              if (versionId >= that->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap[wsConnectionPtr->id][exchangeSubscriptionId]
                                   .begin()
                                   ->first) {
                const auto& channelId =
                    that->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_CHANNEL_ID];
                const auto& symbolId =
                    that->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId][CCAPI_SYMBOL_ID];
                const auto& optionMap = that->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
                that->orderbookVersionIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId] = versionId;
                const auto& correlationIdList = that->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnectionPtr->id).at(channelId).at(symbolId);
                std::map<Decimal, std::string>& snapshotBid = that->snapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
                std::map<Decimal, std::string>& snapshotAsk = that->snapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId];
                snapshotBid.clear();
                snapshotAsk.clear();
                MarketDataMessage::TypeForData input;
                that->extractOrderBookInitialData(input, document);
                for (const auto& x : input) {
                  const auto& type = x.first;
                  const auto& detail = x.second;
                  if (type == MarketDataMessage::DataType::BID) {
                    for (const auto& y : detail) {
                      const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
                      const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
                      Decimal decimalPrice(price);
                      snapshotBid.emplace(decimalPrice, std::string(size));
                    }
                  } else if (type == MarketDataMessage::DataType::ASK) {
                    for (const auto& y : detail) {
                      const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
                      const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
                      Decimal decimalPrice(price);
                      snapshotAsk.emplace(decimalPrice, std::string(size));
                    }
                  }
                }
                if (that->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap.at(wsConnectionPtr->id).find(exchangeSubscriptionId) !=
                    that->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap.at(wsConnectionPtr->id).end()) {
                  auto it = that->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap.at(wsConnectionPtr->id)
                                .at(exchangeSubscriptionId)
                                .upper_bound(versionId);
                  while (it != that->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap.at(wsConnectionPtr->id)
                                   .at(exchangeSubscriptionId)
                                   .end()) {
                    const auto& input = it->second;
                    for (const auto& x : input) {
                      const auto& type = x.first;
                      const auto& detail = x.second;
                      if (type == MarketDataMessage::DataType::BID) {
                        for (const auto& y : detail) {
                          const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
                          const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
                          Decimal decimalPrice(price);
                          that->updateOrderBook(snapshotBid, decimalPrice, size, that->sessionOptions.enableCheckOrderBookChecksum);
                        }
                      } else if (type == MarketDataMessage::DataType::ASK) {
                        for (const auto& y : detail) {
                          const auto& price = y.at(MarketDataMessage::DataFieldType::PRICE);
                          const auto& size = y.at(MarketDataMessage::DataFieldType::SIZE);
                          Decimal decimalPrice(price);
                          that->updateOrderBook(snapshotAsk, decimalPrice, size, that->sessionOptions.enableCheckOrderBookChecksum);
                        }
                      }
                    }
                    that->orderbookVersionIdByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId] = it->first;
                    it++;
                  }
                  that->marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap.at(wsConnectionPtr->id).erase(exchangeSubscriptionId);
                }
                Event event;
                event.setType(Event::Type::SUBSCRIPTION_DATA);
                std::vector<Element> elementList;
                int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
                int bidIndex = 0;
                for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); iter++) {
                  if (bidIndex < maxMarketDepth) {
                    Element element;
                    element.insert(CCAPI_BEST_BID_N_PRICE, ConvertDecimalToString(iter->first));
                    element.insert(CCAPI_BEST_BID_N_SIZE, iter->second);
                    elementList.emplace_back(std::move(element));
                  }
                  ++bidIndex;
                }
                if (snapshotBid.empty()) {
                  Element element;
                  element.insert(CCAPI_BEST_BID_N_PRICE, CCAPI_BEST_BID_N_PRICE_EMPTY);
                  element.insert(CCAPI_BEST_BID_N_SIZE, CCAPI_BEST_BID_N_SIZE_EMPTY);
                  elementList.emplace_back(std::move(element));
                }
                int askIndex = 0;
                for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); iter++) {
                  if (askIndex < maxMarketDepth) {
                    Element element;
                    element.insert(CCAPI_BEST_ASK_N_PRICE, ConvertDecimalToString(iter->first));
                    element.insert(CCAPI_BEST_ASK_N_SIZE, iter->second);
                    elementList.emplace_back(std::move(element));
                  }
                  ++askIndex;
                }
                if (snapshotAsk.empty()) {
                  Element element;
                  element.insert(CCAPI_BEST_ASK_N_PRICE, CCAPI_BEST_ASK_N_PRICE_EMPTY);
                  element.insert(CCAPI_BEST_ASK_N_SIZE, CCAPI_BEST_ASK_N_SIZE_EMPTY);
                  elementList.emplace_back(std::move(element));
                }
                std::vector<Message> messageList;
                Message message;
                message.setTimeReceived(timeReceived);
                message.setType(Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH);
                message.setRecapType(Message::RecapType::SOLICITED);
                message.setTime(timeReceived);
                message.setElementList(elementList);
                message.setCorrelationIdList(correlationIdList);
                messageList.emplace_back(std::move(message));
                event.addMessages(messageList);
                that->eventHandler(event, nullptr);
                that->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnectionPtr->id][channelId][symbolId] = true;
              } else {
                that->buildOrderBookInitialOnFail(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
                // if (delayMilliseconds > 0) {
                //   that->fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap[wsConnectionPtr->id][exchangeSubscriptionId] =
                //       that->serviceContextPtr->tlsClientPtr->set_timer(
                //           delayMilliseconds, [wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds, that](ErrorCode const& ec) {
                //             if (ec) {
                //               that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                //             } else {
                //               that->buildOrderBookInitial(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
                //             }
                //           });
                // } else {
                //   that->buildOrderBookInitial(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
                // }
              }
              return;
            } catch (const std::runtime_error& e) {
              CCAPI_LOGGER_ERROR(std::string("e.what() = ") + e.what());
            }
          }
          that->buildOrderBookInitialOnFail(wsConnectionPtr, exchangeSubscriptionId, delayMilliseconds);
          // WsConnection thisWsConnection = wsConnection;
          // that->onFail_(thisWsConnection);
        },
        this->sessionOptions.httpRequestTimeoutMilliseconds);
  }

  std::string convertCandlestickIntervalSecondsToInterval(int intervalSeconds, const std::string& secondStr, const std::string& minuteStr,
                                                          const std::string& hourStr, const std::string& dayStr, const std::string& weekStr) {
    std::string interval;
    if (intervalSeconds < 60) {
      interval = std::to_string(intervalSeconds) + secondStr;
    } else if (intervalSeconds < 3600) {
      interval = std::to_string(intervalSeconds / 60) + minuteStr;
    } else if (intervalSeconds < 86400) {
      interval = std::to_string(intervalSeconds / 3600) + hourStr;
    } else if (intervalSeconds < 604800) {
      interval = std::to_string(intervalSeconds / 86400) + dayStr;
    } else {
      interval = std::to_string(intervalSeconds / 604800) + weekStr;
    }
    return interval;
  }

  virtual std::vector<std::string> createSendStringListFromSubscriptionList(std::shared_ptr<WsConnection> wsConnectionPtr,
                                                                            const std::vector<Subscription>& subscriptionList, const TimePoint& now,
                                                                            const std::map<std::string, std::string>& credential) {
    return {};
  }

  virtual void convertTextMessageToMarketDataMessage(const Request& request, boost::beast::string_view textMessageView, const TimePoint& timeReceived,
                                                     Event& event, std::vector<MarketDataMessage>& marketDataMessageList) {}

  virtual void processTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, boost::beast::string_view textMessageView, const TimePoint& timeReceived,
                                  Event& event, std::vector<MarketDataMessage>& marketDataMessageList) {}

  virtual std::string calculateOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk) {
    return {};
  }

  virtual std::vector<std::string> createSendStringList(std::shared_ptr<WsConnection> wsConnectionPtr) { return {}; }

  virtual void prepareSubscriptionDetail(std::string& channelId, std::string& symbolId, const std::string& field, std::shared_ptr<WsConnection> wsConnectionPtr,
                                         const Subscription& subscription, const std::map<std::string, std::string> optionMap) {}

  virtual void createFetchOrderBookInitialReq(http::request<http::string_body>& req, const std::string& symbolId, const TimePoint& now,
                                              const std::map<std::string, std::string>& credential) {}

  virtual void extractOrderBookInitialVersionId(int64_t& versionId, const rj::Document& document) {}

  virtual void extractOrderBookInitialData(MarketDataMessage::TypeForData& input, const rj::Document& document) {}

  std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> fieldByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>> optionMapByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, int>>> marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::vector<Subscription>>>> subscriptionListByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::vector<std::string>>>> correlationIdListByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string>>>> snapshotBidByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string>>>> snapshotAskByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string>>>>
      previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string>>>>
      previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool>>> processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool>>> processedInitialTradeByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool>>> l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, TimePoint>>> previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, TimerPtr>>> conflateTimerMapByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::string>> orderBookChecksumByConnectionIdSymbolIdMap;
  bool shouldAlignSnapshot{};
  std::map<std::string, std::map<std::string, Subscription::Status>> subscriptionStatusByInstrumentGroupInstrumentMap;
  std::map<std::string, std::string> instrumentGroupByWsConnectionIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> openByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, Decimal>>> highByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, Decimal>>> lowByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> closeByConnectionIdChannelIdSymbolIdMap;
  std::string getRecentTradesTarget;
  std::string getHistoricalTradesTarget;
  std::string getRecentCandlesticksTarget;
  std::string getHistoricalCandlesticksTarget;
  std::string getMarketDepthTarget;
  std::string getServerTimeTarget;
  std::string getInstrumentTarget;
  std::string getInstrumentsTarget;
  std::string getBbosTarget;
  std::string getTickersTarget;
  std::map<std::string, int> exchangeJsonPayloadIdByConnectionIdMap;
  std::map<std::string, std::map<int, std::vector<std::string>>> exchangeSubscriptionIdListByConnectionIdExchangeJsonPayloadIdMap;
  // only needed for generic public subscription
  std::map<std::string, std::string> correlationIdByConnectionIdMap;
  std::map<std::string, std::map<std::string, std::map<int64_t, MarketDataMessage::TypeForOwingData>>>
      marketDataMessageDataBufferByConnectionIdExchangeSubscriptionIdVersionIdMap;
  std::map<std::string, std::map<std::string, int64_t>> orderbookVersionIdByConnectionIdExchangeSubscriptionIdMap;
  std::map<std::string, std::map<std::string, TimerPtr>> fetchMarketDepthInitialSnapshotTimerByConnectionIdExchangeSubscriptionIdMap;
};

} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_H_
