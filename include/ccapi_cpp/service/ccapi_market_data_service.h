#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) if (!(x)) { throw std::runtime_error("rapidjson internal assertion failure"); }
#endif
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "websocketpp/common/connection_hdl.hpp"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_market_data_message.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/client.hpp"
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util_private.h"
#include "ccapi_cpp/ccapi_decimal.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_ws_connection.h"
#include "ccapi_cpp/service/ccapi_service_context.h"
#include "ccapi_cpp/service/ccapi_service.h"
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)
#include <sstream>
#include <iomanip>
#include "ccapi_cpp/websocketpp_decompress_workaround.h"
#endif
namespace wspp = websocketpp;
namespace rj = rapidjson;
namespace ccapi {
class MarketDataService : public Service, public std::enable_shared_from_this<MarketDataService>  {
 public:
  MarketDataService(std::function<void(Event& event)> eventHandler,
                  SessionOptions sessionOptions, SessionConfigs sessionConfigs, std::shared_ptr<ServiceContext> serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr),
        eventHandler(eventHandler),
        sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs),
        serviceContextPtr(serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->pingIntervalMilliSeconds = sessionOptions.pingIntervalMilliSeconds;
    CCAPI_LOGGER_INFO("this->pingIntervalMilliSeconds = "+toString(this->pingIntervalMilliSeconds));
    this->pongTimeoutMilliSeconds = sessionOptions.pongTimeoutMilliSeconds;
    CCAPI_LOGGER_INFO("this->pongTimeoutMilliSeconds = "+toString(this->pongTimeoutMilliSeconds));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~MarketDataService() {
  }
  void stop() override {
    this->shouldContinue = false;
    for (const auto & x : this->wsConnectionMap) {
      auto wsConnection = x.second;
      ErrorCode ec;
      this->close(wsConnection, wsConnection.hdl, websocketpp::close::status::normal, "stop", ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "shutdown");
      }
      this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
    }
  }
  void subscribe(const std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrl = "+this->baseUrl);
    if (this->shouldContinue.load()) {
      for (const auto & x : this->groupSubscriptionListByInstrumentGroup(subscriptionList)) {
        auto instrumentGroup = x.first;
        auto subscriptionListGivenInstrumentGroup = x.second;
        wspp::lib::asio::post(
          this->serviceContextPtr->tlsClientPtr->get_io_service(),
          [that = shared_from_this(), instrumentGroup, subscriptionListGivenInstrumentGroup](){
            std::map<std::string, std::vector<std::string> > wsConnectionIdListByInstrumentGroupMap = invertMapMulti(that->instrumentGroupByWsConnectionIdMap);
            if (wsConnectionIdListByInstrumentGroupMap.find(instrumentGroup) != wsConnectionIdListByInstrumentGroupMap.end()
                && that->subscriptionStatusByInstrumentGroupInstrumentMap.find(instrumentGroup) != that->subscriptionStatusByInstrumentGroupInstrumentMap.end()
                ) {
              auto wsConnectionId = wsConnectionIdListByInstrumentGroupMap.at(instrumentGroup).at(0);
              auto wsConnection = that->wsConnectionMap.at(wsConnectionId);
              for (const auto & subscription : subscriptionListGivenInstrumentGroup) {
                auto instrument = subscription.getInstrument();
                if (that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup].find(instrument) != that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup].end()) {
                  that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "already subscribed: " + toString(subscription));
                  return;
                }
                wsConnection.subscriptionList.push_back(subscription);
                that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
                that->prepareSubscription(wsConnection, subscription);
              }
              that->subscribeToExchange(wsConnection);
            } else {
              auto url = UtilString::split(instrumentGroup, "|").at(0);
              WsConnection wsConnection(url, subscriptionListGivenInstrumentGroup);
              that->connect(wsConnection);
              that->wsConnectionMap.insert(std::pair<std::string, WsConnection>(wsConnection.id, wsConnection));
              that->instrumentGroupByWsConnectionIdMap.insert(std::pair<std::string, std::string>(wsConnection.id, instrumentGroup));
              wsConnection.instrumentGroup = instrumentGroup;
              for (const auto & subscription : subscriptionListGivenInstrumentGroup) {
                auto instrument = subscription.getInstrument();
                that->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
              }
              wsConnectionIdListByInstrumentGroupMap[instrumentGroup].push_back(wsConnection.id);
            }
        });
      }
      CCAPI_LOGGER_INFO("actual connection map is "+toString(this->wsConnectionMap));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

 protected:
  typedef ServiceContext::SslContextPtr SslContextPtr;
  typedef ServiceContext::TlsClient TlsClient;
  typedef wspp::lib::error_code ErrorCode;
  typedef wspp::lib::shared_ptr<wspp::lib::asio::steady_timer> TimerPtr;
  typedef wspp::lib::function<void(ErrorCode const &)> TimerHandler;
  std::map<std::string, std::vector<Subscription> > groupSubscriptionListByInstrumentGroup(const std::vector<Subscription>& subscriptionList) {
    std::map<std::string, std::vector<Subscription> > groups;
    for (const auto & subscription : subscriptionList) {
      std::string instrumentGroup = this->getInstrumentGroup(subscription);
      groups[instrumentGroup].push_back(subscription);
    }
    return groups;
  }
  virtual std::string getInstrumentGroup(const Subscription& subscription) {
    return this->baseUrl + "|" + subscription.getField() + "|" + subscription.getSerializedOptions();
  }
  SslContextPtr onTlsInit(wspp::connection_hdl hdl) {
    return this->serviceContextPtr->sslContextPtr;
  }
  virtual void onOpen(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = std::chrono::system_clock::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::OPEN;
    wsConnection.hdl = hdl;
    CCAPI_LOGGER_INFO("connection " +toString(wsConnection) + " established");
    this->connectNumRetryOnFailByConnectionUrlMap[wsConnection.url] = 0;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    Element element;
    element.insert(CCAPI_CONNECTION, toString(wsConnection));
    message.setElementList({ element });
    event.setMessageList({ message });
    this->eventHandler(event);
    if (this->sessionOptions.enableCheckPingPong) {
      this->setPingPongTimer(wsConnection, hdl);
    }
    auto instrumentGroup = wsConnection.instrumentGroup;
    for (const auto& subscription : wsConnection.subscriptionList) {
      auto instrument = subscription.getInstrument();
      this->subscriptionStatusByInstrumentGroupInstrumentMap[instrumentGroup][instrument] = Subscription::Status::SUBSCRIBING;
      this->prepareSubscription(wsConnection, subscription);
    }
    this->subscribeToExchange(wsConnection);
  }
  std::string convertInstrumentToWebsocketSymbolId(std::string instrument) {
    std::string symbolId = instrument;
    if (!instrument.empty()) {
      if (this->sessionConfigs.getExchangeInstrumentSymbolMap().find(this->name) != this->sessionConfigs.getExchangeInstrumentSymbolMap().end() &&
          this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).find(instrument) != this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).end()) {
        symbolId = this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).at(instrument);
      }
    }
    return symbolId;
  }
  void prepareSubscription(const WsConnection& wsConnection, const Subscription& subscription) {
    auto instrument = subscription.getInstrument();
    auto symbolId = this->convertInstrumentToWebsocketSymbolId(instrument);
    auto field = subscription.getField();
    auto optionMap = subscription.getOptionMap();
    std::string channelId = this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(this->name).at(field);
    if (field == CCAPI_MARKET_DEPTH) {
      if (this->name == CCAPI_EXCHANGE_NAME_KRAKEN || this->name == CCAPI_EXCHANGE_NAME_BITFINEX
          || this->name == CCAPI_EXCHANGE_NAME_BINANCE_US || this->name == CCAPI_EXCHANGE_NAME_BINANCE || this->name == CCAPI_EXCHANGE_NAME_BINANCE_FUTURES  || this->name == CCAPI_EXCHANGE_NAME_HUOBI || this->name == CCAPI_EXCHANGE_NAME_OKEX) {
        int marketDepthSubscribedToExchange = 1;
        marketDepthSubscribedToExchange = this->calculateMarketDepthSubscribedToExchange(
            std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)),
            this->sessionConfigs.getWebsocketAvailableMarketDepth().at(this->name));
        channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "="
            + std::to_string(marketDepthSubscribedToExchange);
        this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] =
            marketDepthSubscribedToExchange;
      } else if (this->name == CCAPI_EXCHANGE_NAME_GEMINI) {
        if (optionMap.at(CCAPI_MARKET_DEPTH_MAX) == "1") {
          int marketDepthSubscribedToExchange = 1;
          channelId += std::string("?") + CCAPI_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "="
              + std::to_string(marketDepthSubscribedToExchange);
          this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] =
              marketDepthSubscribedToExchange;
        }
      } else if (this->name == CCAPI_EXCHANGE_NAME_BITMEX) {
        if (std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)) == 1) {
          channelId = CCAPI_WEBSOCKET_BITMEX_CHANNEL_QUOTE;
        } else if (std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)) == 10) {
          channelId =
          CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10;
        } else if (std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX)) == 25) {
          channelId =
          CCAPI_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2_25;
        }
      }
    }
    this->correlationIdListByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId].push_back(
        subscription.getCorrelationId());
    this->subscriptionListByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId].push_back(
        subscription);
    this->fieldByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = field;
    this->optionMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId].insert(
        optionMap.begin(), optionMap.end());
    CCAPI_LOGGER_TRACE("this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap = "+toString(this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap));
    CCAPI_LOGGER_TRACE("this->correlationIdListByConnectionIdChannelSymbolIdMap = "+toString(this->correlationIdListByConnectionIdChannelIdSymbolIdMap));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onFail(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    wsConnection.status = WsConnection::Status::FAILED;
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, "connection " + toString(wsConnection) + " has failed before opening");
    WsConnection thisWsConnection = wsConnection;
    this->wsConnectionMap.erase(thisWsConnection.id);
    this->instrumentGroupByWsConnectionIdMap.erase(thisWsConnection.id);
    long seconds = std::round(
        UtilAlgorithm::exponentialBackoff(
            1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[thisWsConnection.url], 6)));
    CCAPI_LOGGER_INFO("about to set timer for "+toString(seconds)+" seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(thisWsConnection.id)
        != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(thisWsConnection.id)->cancel();
    }
    this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] =
        this->serviceContextPtr->tlsClientPtr->set_timer(
            seconds * 1000,
            [thisWsConnection, that = shared_from_this()](ErrorCode const& ec) {
              if (that->wsConnectionMap.find(thisWsConnection.id) == that->wsConnectionMap.end()) {
                if (ec) {
                  CCAPI_LOGGER_ERROR("wsConnection = " + toString(thisWsConnection) + ", connect retry on fail timer error: " + ec.message());
                  that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "timer");
                } else {
                  CCAPI_LOGGER_INFO("about to retry");
                  auto thatWsConnection = thisWsConnection;
                  thatWsConnection.assignDummyId();
                  that->connect(thatWsConnection);
                  that->wsConnectionMap.insert(std::pair<std::string, WsConnection>(thatWsConnection.id, thatWsConnection));
                  that->instrumentGroupByWsConnectionIdMap.insert(std::pair<std::string, std::string>(thatWsConnection.id, thatWsConnection.instrumentGroup));
                  that->connectNumRetryOnFailByConnectionUrlMap[thatWsConnection.url] += 1;
                }
              }
            });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onClose(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = std::chrono::system_clock::now();
    TlsClient::connection_ptr con = this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl);
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(con);
    wsConnection.status = WsConnection::Status::CLOSED;
    CCAPI_LOGGER_INFO("connection " +toString(wsConnection) + " is closed");
    std::stringstream s;
    s << "close code: " << con->get_remote_close_code() << " ("
        << websocketpp::close::status::get_string(con->get_remote_close_code()) << "), close reason: "
        << con->get_remote_close_reason();
    std::string reason = s.str();
    CCAPI_LOGGER_INFO("reason is " +reason);
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_DOWN);
    Element element;
    element.insert(CCAPI_CONNECTION, toString(wsConnection));
    element.insert(CCAPI_REASON, reason);
    message.setElementList({ element });
    event.setMessageList({ message });
    this->eventHandler(event);
    CCAPI_LOGGER_INFO("connection " +toString(wsConnection) + " is closed");
    CCAPI_LOGGER_INFO("clear states for wsConnection "+toString(wsConnection));
    this->fieldByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->optionMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->subscriptionListByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->correlationIdListByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.erase(wsConnection.id);
    this->snapshotBidByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->snapshotAskByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap.erase(wsConnection.id);
    this->lastPongTpByConnectionIdMap.erase(wsConnection.id);
    this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.erase(wsConnection.id);
    if (this->pingTimerByConnectionIdMap.find(wsConnection.id) != this->pingTimerByConnectionIdMap.end()) {
      this->pingTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      this->pingTimerByConnectionIdMap.erase(wsConnection.id);
    }
    if (this->pongTimeOutTimerByConnectionIdMap.find(wsConnection.id)
        != this->pongTimeOutTimerByConnectionIdMap.end()) {
      this->pongTimeOutTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      this->pongTimeOutTimerByConnectionIdMap.erase(wsConnection.id);
    }
    this->connectNumRetryOnFailByConnectionUrlMap.erase(wsConnection.url);
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(wsConnection.id)
        != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      this->connectRetryOnFailTimerByConnectionIdMap.erase(wsConnection.id);
    }
    this->orderBookChecksumByConnectionIdSymbolIdMap.erase(wsConnection.id);
    WsConnection thisWsConnection = wsConnection;
    this->wsConnectionMap.erase(thisWsConnection.id);
    this->instrumentGroupByWsConnectionIdMap.erase(thisWsConnection.id);
    if (this->shouldContinue.load()) {
      thisWsConnection.assignDummyId();
      this->connect(thisWsConnection);
      this->wsConnectionMap.insert(std::pair<std::string, WsConnection>(thisWsConnection.id, thisWsConnection));
      this->instrumentGroupByWsConnectionIdMap.insert(std::pair<std::string, std::string>(thisWsConnection.id, thisWsConnection.instrumentGroup));
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onMessage(wspp::connection_hdl hdl, TlsClient::message_ptr msg) {
    auto now = std::chrono::system_clock::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_DEBUG("received a message from connection "+toString(wsConnection));
    if (wsConnection.status == WsConnection::Status::CLOSING
        && !this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id]) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }
    auto opcode = msg->get_opcode();
    CCAPI_LOGGER_DEBUG("opcode = "+toString(opcode));
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
      std::string textMessage = msg->get_payload();
      CCAPI_LOGGER_DEBUG("received a text message: "+textMessage);
      try {
        this->onTextMessage(hdl, textMessage, now);
      } catch (const std::exception& e) {
        CCAPI_LOGGER_ERROR("textMessage = " + textMessage);
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, e);
      }
    } else if (opcode == websocketpp::frame::opcode::binary) {
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)
      if (this->name == CCAPI_EXCHANGE_NAME_HUOBI || this->name == CCAPI_EXCHANGE_NAME_OKEX) {
        std::string decompressed;
        std::string payload = msg->get_payload();
        try {
          ErrorCode ec = this->inflater.decompress(reinterpret_cast<const uint8_t*>(&payload[0]), payload.size(), decompressed);
          if (ec) {
            CCAPI_LOGGER_FATAL(ec.message());
          }
          CCAPI_LOGGER_DEBUG("decompressed = " + decompressed);
          this->onTextMessage(hdl, decompressed, now);
        } catch (const std::exception& e) {
          std::stringstream ss;
          ss << std::hex << std::setfill('0');
          for (int i = 0; i < payload.size(); ++i) {
              ss << std::setw(2) << static_cast<unsigned>(reinterpret_cast<const uint8_t*>(&payload[0])[i]);
          }
          CCAPI_LOGGER_ERROR("binaryMessage = " + ss.str());
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, e);
        }
        ErrorCode ec = this->inflater.inflate_reset();
        if (ec) {
          this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "decompress");
        }
      }
#endif
    }
  }
  void onPong(wspp::connection_hdl hdl, std::string payload) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = std::chrono::system_clock::now();
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    this->lastPongTpByConnectionIdMap[wsConnection.id] = now;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  bool onPing(wspp::connection_hdl hdl, std::string payload) {
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    CCAPI_LOGGER_TRACE("received a ping from " + toString(wsConnection));
    return true;
  }
  virtual void onTextMessage(wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    WsConnection& wsConnection = this->getWsConnectionFromConnectionPtr(
        this->serviceContextPtr->tlsClientPtr->get_con_from_hdl(hdl));
    auto wsMessageList = this->processTextMessage(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_TRACE("websocketMessageList = "+toString(wsMessageList));
    if (!wsMessageList.empty()) {
      for (auto const & wsMessage : wsMessageList) {
        Event event;
        bool shouldEmitEvent = true;
        if (wsMessage.type == MarketDataMessage::Type::MARKET_DATA_EVENTS) {
          if (wsMessage.recapType == MarketDataMessage::RecapType::NONE
              && this->sessionOptions.warnLateEventMaxMilliSeconds > 0
              && std::chrono::duration_cast<std::chrono::milliseconds>(timeReceived - wsMessage.tp).count()
                  > this->sessionOptions.warnLateEventMaxMilliSeconds) {
            CCAPI_LOGGER_WARN("late websocket message: timeReceived = "+toString(timeReceived)+", wsMessage.tp = "+toString(wsMessage.tp)+", wsConnection = "+toString(wsConnection));
          }
          event.setType(Event::Type::SUBSCRIPTION_DATA);
          std::string exchangeSubscriptionId = wsMessage.exchangeSubscriptionId;
          std::string channelId =
              this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_CHANNEL_ID);
          std::string symbolId =
              this->channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_SYMBOL_ID);
          auto field = this->fieldByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          CCAPI_LOGGER_TRACE("this->optionMapByConnectionIdChannelIdSymbolIdMap = "+toString(this->optionMapByConnectionIdChannelIdSymbolIdMap));
          CCAPI_LOGGER_TRACE("wsConnection = "+toString(wsConnection));
          CCAPI_LOGGER_TRACE("channelId = "+toString(channelId));
          CCAPI_LOGGER_TRACE("symbolId = "+toString(symbolId));
          auto optionMap = this->optionMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          CCAPI_LOGGER_TRACE("optionMap = "+toString(optionMap));
          auto correlationIdList =
              this->correlationIdListByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId);
          CCAPI_LOGGER_TRACE("correlationIdList = "+toString(correlationIdList));
          if (wsMessage.data.find(MarketDataMessage::DataType::BID) != wsMessage.data.end()
              || wsMessage.data.find(MarketDataMessage::DataType::ASK) != wsMessage.data.end()) {
            std::map<Decimal, std::string>& snapshotBid =
                this->snapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
            std::map<Decimal, std::string>& snapshotAsk =
                this->snapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
            if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]
                && wsMessage.recapType == MarketDataMessage::RecapType::NONE) {
              this->processUpdateSnapshot(wsConnection, channelId, symbolId, event, shouldEmitEvent, wsMessage.tp,
                                            timeReceived, wsMessage.data, field, optionMap, correlationIdList,
                                            snapshotBid, snapshotAsk);
              if (this->sessionOptions.enableCheckOrderBookChecksum) {
                bool shouldProcessRemainingMessage = true;
                std::string receivedOrderBookChecksumStr =
                    this->orderBookChecksumByConnectionIdSymbolIdMap[wsConnection.id][symbolId];
                if (!this->checkOrderBookChecksum(snapshotBid, snapshotAsk, receivedOrderBookChecksumStr,
                                                  shouldProcessRemainingMessage)) {
                  CCAPI_LOGGER_ERROR("snapshotBid = "+toString(snapshotBid));
                  CCAPI_LOGGER_ERROR("snapshotAsk = "+toString(snapshotAsk));
                  this->onIncorrectStatesFound(wsConnection, hdl, textMessage, timeReceived, exchangeSubscriptionId,
                                               "order book incorrect checksum found");
                }
                if (!shouldProcessRemainingMessage) {
                  return;
                }
              }
              if (this->sessionOptions.enableCheckOrderBookCrossed) {
                bool shouldProcessRemainingMessage = true;
                if (!this->checkOrderBookCrossed(snapshotBid, snapshotAsk, shouldProcessRemainingMessage)) {
                  CCAPI_LOGGER_ERROR("lastNToString(snapshotBid, 1) = "+lastNToString(snapshotBid, 1));
                  CCAPI_LOGGER_ERROR("firstNToString(snapshotAsk, 1) = "+firstNToString(snapshotAsk, 1));
                  this->onIncorrectStatesFound(wsConnection, hdl, textMessage, timeReceived, exchangeSubscriptionId,
                                               "order book crossed market found");
                }
                if (!shouldProcessRemainingMessage) {
                  return;
                }
              }
            } else if (wsMessage.recapType == MarketDataMessage::RecapType::SOLICITED) {
              this->processInitialSnapshot(wsConnection, channelId, symbolId, event, shouldEmitEvent, wsMessage.tp,
                                             timeReceived, wsMessage.data, field, optionMap, correlationIdList,
                                             snapshotBid, snapshotAsk);
            }
            CCAPI_LOGGER_TRACE("snapshotBid.size() = "+toString(snapshotBid.size()));
            CCAPI_LOGGER_TRACE("snapshotAsk.size() = "+toString(snapshotAsk.size()));
          }
          if (wsMessage.data.find(MarketDataMessage::DataType::TRADE) != wsMessage.data.end()) {
            this->processTrade(wsConnection, channelId, symbolId, event, shouldEmitEvent, wsMessage.tp,
                                                        timeReceived, wsMessage.data, field, optionMap, correlationIdList);
          }
        } else {
          CCAPI_LOGGER_WARN("websocket event type is unknown!");
        }
        CCAPI_LOGGER_TRACE("event type is "+event.typeToString(event.getType()));
        if (event.getType() == Event::Type::UNKNOWN) {
          CCAPI_LOGGER_WARN("event type is unknown!");
        } else {
          if (event.getMessageList().empty()) {
            CCAPI_LOGGER_DEBUG("event has no messages!");
            shouldEmitEvent = false;
          }
          if (shouldEmitEvent) {
            this->eventHandler(event);
          }
        }
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void updateOrderBook(std::map<Decimal, std::string>& snapshot, const Decimal& price, const std::string& size) {
    Decimal sizeDecimal(size);
    if (snapshot.find(price) == snapshot.end()) {
      if (sizeDecimal > Decimal("0")) {
        snapshot.insert(std::pair<Decimal, std::string>(price, size));
      }
    } else {
      if (sizeDecimal > Decimal("0")) {
        snapshot[price] = size;
      } else {
        snapshot.erase(price);
      }
    }
  }
  void updateElementListWithInitialMarketDepth(const std::string& field,
                                               const std::map<std::string, std::string>& optionMap,
                                               const std::map<Decimal, std::string>& snapshotBid,
                                               const std::map<Decimal, std::string>& snapshotAsk,
                                               std::vector<Element>& elementList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (field == CCAPI_MARKET_DEPTH) {
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      int bidIndex = 0;
      for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); iter++) {
        if (bidIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_BEST_BID_N_PRICE, iter->first.toString());
          element.insert(CCAPI_BEST_BID_N_SIZE, iter->second);
          elementList.push_back(std::move(element));
        }
        ++bidIndex;
      }
      if (snapshotBid.empty()) {
        Element element;
        element.insert(CCAPI_BEST_BID_N_PRICE,
        CCAPI_BEST_BID_N_PRICE_EMPTY);
        element.insert(CCAPI_BEST_BID_N_SIZE,
        CCAPI_BEST_BID_N_SIZE_EMPTY);
        elementList.push_back(std::move(element));
      }
      int askIndex = 0;
      for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); iter++) {
        if (askIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_BEST_ASK_N_PRICE, iter->first.toString());
          element.insert(CCAPI_BEST_ASK_N_SIZE, iter->second);
          elementList.push_back(std::move(element));
        }
        ++askIndex;
      }
      if (snapshotAsk.empty()) {
        Element element;
        element.insert(CCAPI_BEST_ASK_N_PRICE,
        CCAPI_BEST_ASK_N_PRICE_EMPTY);
        element.insert(CCAPI_BEST_ASK_N_SIZE,
        CCAPI_BEST_ASK_N_SIZE_EMPTY);
        elementList.push_back(std::move(element));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::map<Decimal, std::string> calculateMarketDepthUpdate(bool isBid,
                                                          const std::map<Decimal, std::string>& c1,
                                                          const std::map<Decimal, std::string>& c2,
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
  void updateElementListWithUpdateMarketDepth(const std::string& field,
                                              const std::map<std::string, std::string>& optionMap,
                                              const std::map<Decimal, std::string>& snapshotBid,
                                              const std::map<Decimal, std::string>& snapshotBidPrevious,
                                              const std::map<Decimal, std::string>& snapshotAsk,
                                              const std::map<Decimal, std::string>& snapshotAskPrevious,
                                              std::vector<Element>& elementList,
                                              bool alwaysUpdate) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (field == CCAPI_MARKET_DEPTH) {
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      if (optionMap.at(CCAPI_MARKET_DEPTH_RETURN_UPDATE) == CCAPI_MARKET_DEPTH_RETURN_UPDATE_ENABLE) {
        CCAPI_LOGGER_TRACE("lastNSame = "+toString(lastNSame(snapshotBid, snapshotBidPrevious, maxMarketDepth)));
        CCAPI_LOGGER_TRACE("firstNSame = "+toString(firstNSame(snapshotAsk, snapshotAskPrevious, maxMarketDepth)));
        const std::map<Decimal, std::string>& snapshotBidUpdate = this->calculateMarketDepthUpdate(true, snapshotBid, snapshotBidPrevious, maxMarketDepth);
        for (const auto& x : snapshotBidUpdate) {
          Element element;
          element.insert(CCAPI_BEST_BID_N_PRICE, x.first.toString());
          element.insert(CCAPI_BEST_BID_N_SIZE, x.second);
          elementList.push_back(std::move(element));
        }
        const std::map<Decimal, std::string>& snapshotAskUpdate = this->calculateMarketDepthUpdate(false, snapshotAsk, snapshotAskPrevious, maxMarketDepth);
        for (const auto& x : snapshotAskUpdate) {
          Element element;
          element.insert(CCAPI_BEST_ASK_N_PRICE, x.first.toString());
          element.insert(CCAPI_BEST_ASK_N_SIZE, x.second);
          elementList.push_back(std::move(element));
        }
      } else {
        CCAPI_LOGGER_TRACE("lastNSame = "+toString(lastNSame(snapshotBid, snapshotBidPrevious, maxMarketDepth)));
        CCAPI_LOGGER_TRACE("firstNSame = "+toString(firstNSame(snapshotAsk, snapshotAskPrevious, maxMarketDepth)));
        if (alwaysUpdate || !lastNSame(snapshotBid, snapshotBidPrevious, maxMarketDepth)
            || !firstNSame(snapshotAsk, snapshotAskPrevious, maxMarketDepth)) {
          int bidIndex = 0;
          for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); ++iter) {
            if (bidIndex >= maxMarketDepth) {
              break;
            }
            Element element;
            element.insert(CCAPI_BEST_BID_N_PRICE, iter->first.toString());
            element.insert(CCAPI_BEST_BID_N_SIZE, iter->second);
            elementList.push_back(std::move(element));
            ++bidIndex;
          }
          if (snapshotBid.empty()) {
            Element element;
            element.insert(CCAPI_BEST_BID_N_PRICE,
            CCAPI_BEST_BID_N_PRICE_EMPTY);
            element.insert(CCAPI_BEST_BID_N_SIZE,
            CCAPI_BEST_BID_N_SIZE_EMPTY);
            elementList.push_back(std::move(element));
          }
          int askIndex = 0;
          for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); ++iter) {
            if (askIndex >= maxMarketDepth) {
              break;
            }
            Element element;
            element.insert(CCAPI_BEST_ASK_N_PRICE, iter->first.toString());
            element.insert(CCAPI_BEST_ASK_N_SIZE, iter->second);
            elementList.push_back(std::move(element));
            ++askIndex;
          }
          if (snapshotAsk.empty()) {
            Element element;
            element.insert(CCAPI_BEST_ASK_N_PRICE,
            CCAPI_BEST_ASK_N_PRICE_EMPTY);
            element.insert(CCAPI_BEST_ASK_N_SIZE,
            CCAPI_BEST_ASK_N_SIZE_EMPTY);
            elementList.push_back(std::move(element));
          }
        }
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void updateElementListWithTrade(const std::string& field,
                                              const std::map<std::string, std::string>& optionMap,
                                              const MarketDataMessage::TypeForData& input,
                                              std::vector<Element>& elementList) {
    if (field == CCAPI_TRADE) {
      for (const auto & x : input) {
        auto type = x.first;
        auto detail = x.second;
        if (type == MarketDataMessage::DataType::TRADE) {
          for (const auto & y : detail) {
            auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
            auto size = y.at(MarketDataMessage::DataFieldType::SIZE);
            Element element;
            element.insert(CCAPI_LAST_PRICE, y.at(MarketDataMessage::DataFieldType::PRICE));
            element.insert(CCAPI_LAST_SIZE, y.at(MarketDataMessage::DataFieldType::SIZE));
            element.insert(CCAPI_TRADE_ID, y.at(MarketDataMessage::DataFieldType::TRADE_ID));
            element.insert(CCAPI_IS_BUYER_MAKER, y.at(MarketDataMessage::DataFieldType::IS_BUYER_MAKER));
            elementList.push_back(std::move(element));
          }
        } else {
          CCAPI_LOGGER_WARN(
              "extra type " + MarketDataMessage::dataTypeToString(type));
        }
      }
    }
  }
  void connect(WsConnection& wsConnection) {
    wsConnection.status = WsConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("connection initialization on dummy id "+wsConnection.id);
    std::string url = wsConnection.url;
    this->serviceContextPtr->tlsClientPtr->set_tls_init_handler(std::bind(&MarketDataService::onTlsInit, shared_from_this(), std::placeholders::_1));
    CCAPI_LOGGER_DEBUG("endpoint tls init handler set");
    ErrorCode ec;
    TlsClient::connection_ptr con = this->serviceContextPtr->tlsClientPtr->get_connection(url, ec);
    wsConnection.id = this->connectionAddressToString(con);
    CCAPI_LOGGER_DEBUG("connection initialization on actual id "+wsConnection.id);
    if (ec) {
      CCAPI_LOGGER_FATAL("connection initialization error: " + ec.message());
    }
    con->set_open_handler(std::bind(&MarketDataService::onOpen, shared_from_this(), std::placeholders::_1));
    con->set_fail_handler(std::bind(&MarketDataService::onFail, shared_from_this(), std::placeholders::_1));
    con->set_close_handler(std::bind(&MarketDataService::onClose, shared_from_this(), std::placeholders::_1));
    con->set_message_handler(
        std::bind(&MarketDataService::onMessage, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    if (this->sessionOptions.enableCheckPingPong) {
      con->set_pong_handler(std::bind(&MarketDataService::onPong, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    }
    con->set_ping_handler(std::bind(&MarketDataService::onPing, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    this->serviceContextPtr->tlsClientPtr->connect(con);
  }
  void close(WsConnection& wsConnection, wspp::connection_hdl hdl, wspp::close::status::value const code,
             std::string const & reason, ErrorCode & ec) {
    if (wsConnection.status == WsConnection::Status::CLOSING) {
      CCAPI_LOGGER_WARN("websocket connection is already in the state of closing");
      return;
    }
    wsConnection.status = WsConnection::Status::CLOSING;
    this->serviceContextPtr->tlsClientPtr->close(hdl, code, reason, ec);
  }
  void send(wspp::connection_hdl hdl, std::string const & payload, wspp::frame::opcode::value op, ErrorCode & ec) {
    this->serviceContextPtr->tlsClientPtr->send(hdl, payload, op, ec);
  }
  void ping(wspp::connection_hdl hdl, std::string const & payload, ErrorCode & ec) {
    this->serviceContextPtr->tlsClientPtr->ping(hdl, payload, ec);
  }
  void copySnapshot(bool isBid, const std::map<Decimal, std::string>& original, std::map<Decimal, std::string>& copy,
                    const int maxMarketDepth) {
    size_t nToCopy = std::min(original.size(), static_cast<size_t>(maxMarketDepth));
    if (isBid) {
      std::copy_n(original.rbegin(), nToCopy, std::inserter(copy, copy.end()));
    } else {
      std::copy_n(original.begin(), nToCopy, std::inserter(copy, copy.end()));
    }
  }
  void processInitialSnapshot(const WsConnection& wsConnection, const std::string& channelId,
                                const std::string& symbolId, Event& event, bool& shouldEmitEvent, const TimePoint& tp,
                                const TimePoint& timeReceived, const MarketDataMessage::TypeForData & input,
                                const std::string& field,
                                const std::map<std::string, std::string>& optionMap,
                                const std::vector<std::string>& correlationIdList,
                                std::map<Decimal, std::string>& snapshotBid,
                                std::map<Decimal, std::string>& snapshotAsk) {
    snapshotBid.clear();
    snapshotAsk.clear();
    int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
    for (const auto & x : input) {
      auto type = x.first;
      auto detail = x.second;
      if (type == MarketDataMessage::DataType::BID) {
        for (const auto & y : detail) {
          auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
          auto size = y.at(MarketDataMessage::DataFieldType::SIZE);
          snapshotBid.insert(std::pair<Decimal, std::string>(Decimal(price), size));
        }
        CCAPI_LOGGER_TRACE("lastNToString(snapshotBid, "+toString(maxMarketDepth)+") = "+lastNToString(snapshotBid, maxMarketDepth));
      } else if (type == MarketDataMessage::DataType::ASK) {
        for (const auto & y : detail) {
          auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
          auto size = y.at(MarketDataMessage::DataFieldType::SIZE);
          snapshotAsk.insert(std::pair<Decimal, std::string>(Decimal(price), size));
        }
        CCAPI_LOGGER_TRACE("firstNToString(snapshotAsk, "+toString(maxMarketDepth)+") = "+firstNToString(snapshotAsk, maxMarketDepth));
      } else {
        CCAPI_LOGGER_WARN("extra type "+MarketDataMessage::dataTypeToString(type));
      }
    }
    std::vector<Element> elementList;
    this->updateElementListWithInitialMarketDepth(field, optionMap, snapshotBid, snapshotAsk, elementList);
    if (!elementList.empty()) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::MARKET_DATA_EVENTS);
      message.setRecapType(Message::RecapType::SOLICITED);
      message.setTime(tp);
      message.setElementList(elementList);
      message.setCorrelationIdList(correlationIdList);
      std::vector<Message> newMessageList = { message };
      event.addMessages(newMessageList);
      CCAPI_LOGGER_TRACE("event.getMessageList() = "+toString(event.getMessageList()));
    }
    this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] = true;
    bool shouldConflate = optionMap.at(
    CCAPI_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
    if (shouldConflate) {
      this->copySnapshot(
          true, snapshotBid,
          this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId],
          maxMarketDepth);
      this->copySnapshot(
          false, snapshotAsk,
          this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId],
          maxMarketDepth);
      CCAPI_LOGGER_TRACE("this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId) = "+toString(this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId)));
      CCAPI_LOGGER_TRACE("this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId) = "+toString(this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId)));
      TimePoint previousConflateTp = UtilTime::makeTimePointFromMilliseconds(
          std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()
              / std::stoi(optionMap.at(
              CCAPI_CONFLATE_INTERVAL_MILLISECONDS)) * std::stoi(optionMap.at(
          CCAPI_CONFLATE_INTERVAL_MILLISECONDS)));
      this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] =
          previousConflateTp;
      if (optionMap.at(
          CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS) != CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS_DEFAULT) {
        auto interval = std::chrono::milliseconds(std::stoi(optionMap.at(
        CCAPI_CONFLATE_INTERVAL_MILLISECONDS)));
        auto gracePeriod = std::chrono::milliseconds(std::stoi(optionMap.at(
        CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS)));
        this->setConflateTimer(previousConflateTp, interval, gracePeriod, wsConnection, channelId, symbolId, field,
                               optionMap, correlationIdList);
      }
    }
  }
  void processUpdateSnapshot(const WsConnection& wsConnection, const std::string& channelId,
                               const std::string& symbolId, Event& event, bool& shouldEmitEvent, const TimePoint& tp,
                               const TimePoint& timeReceived, const MarketDataMessage::TypeForData& input,
                               const std::string& field,
                               const std::map<std::string, std::string>& optionMap,
                               const std::vector<std::string>& correlationIdList,
                               std::map<Decimal, std::string>& snapshotBid,
                               std::map<Decimal, std::string>& snapshotAsk) {
    CCAPI_LOGGER_TRACE("input = " + MarketDataMessage::dataToString(input));
    if (this->processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
      std::vector<Message> messageList;
      CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_MARKET_DEPTH_MAX));
      std::map<Decimal, std::string> snapshotBidPrevious;
      this->copySnapshot(true, snapshotBid, snapshotBidPrevious, maxMarketDepth);
      std::map<Decimal, std::string> snapshotAskPrevious;
      this->copySnapshot(false, snapshotAsk, snapshotAskPrevious, maxMarketDepth);
      CCAPI_LOGGER_TRACE("before updating orderbook");
      CCAPI_LOGGER_TRACE(
          "lastNToString(snapshotBid, "+toString(maxMarketDepth)+") = " + lastNToString(snapshotBid, maxMarketDepth));
      CCAPI_LOGGER_TRACE(
          "firstNToString(snapshotAsk, "+toString(maxMarketDepth)+") = " + firstNToString(snapshotAsk, maxMarketDepth));
      if (this->l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]) {
        if (input.find(MarketDataMessage::DataType::BID) != input.end()) {
          snapshotBid.clear();
        }
        if (input.find(MarketDataMessage::DataType::ASK) != input.end()) {
          snapshotAsk.clear();
        }
      }
      for (const auto & x : input) {
        auto type = x.first;
        auto detail = x.second;
        if (type == MarketDataMessage::DataType::BID) {
          for (const auto & y : detail) {
            auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
            auto size = y.at(MarketDataMessage::DataFieldType::SIZE);
            this->updateOrderBook(snapshotBid, Decimal(price), size);
          }
        } else if (type == MarketDataMessage::DataType::ASK) {
          for (const auto & y : detail) {
            auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
            auto size = y.at(MarketDataMessage::DataFieldType::SIZE);
            this->updateOrderBook(snapshotAsk, Decimal(price), size);
          }
        } else {
          CCAPI_LOGGER_WARN(
              "extra type " + MarketDataMessage::dataTypeToString(type));
        }
      }
      CCAPI_LOGGER_TRACE(
          "this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap = "
          + toString(
              this
              ->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap));
      if (this->shouldAlignSnapshot) {
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap
            .at(wsConnection.id).at(channelId).at(symbolId);
        this->alignSnapshot(snapshotBid, snapshotAsk, marketDepthSubscribedToExchange);
      }
      CCAPI_LOGGER_TRACE("afer updating orderbook");
      CCAPI_LOGGER_TRACE(
          "lastNToString(snapshotBid, "+toString(maxMarketDepth)+") = " + lastNToString(snapshotBid, maxMarketDepth));
      CCAPI_LOGGER_TRACE(
          "firstNToString(snapshotAsk, "+toString(maxMarketDepth)+") = " + firstNToString(snapshotAsk, maxMarketDepth));
      CCAPI_LOGGER_TRACE(
          "lastNToString(snapshotBidPrevious, "+toString(maxMarketDepth)+") = "
          + lastNToString(snapshotBidPrevious, maxMarketDepth));
      CCAPI_LOGGER_TRACE(
          "firstNToString(snapshotAskPrevious, "+toString(maxMarketDepth)+") = "
          + firstNToString(snapshotAskPrevious, maxMarketDepth));
      CCAPI_LOGGER_TRACE("field = " + toString(field));
      CCAPI_LOGGER_TRACE("maxMarketDepth = " + toString(maxMarketDepth));
      CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
      bool shouldConflate = optionMap.at(
      CCAPI_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
      CCAPI_LOGGER_TRACE("shouldConflate = " + toString(shouldConflate));
      TimePoint conflateTp =
          shouldConflate ?
              UtilTime::makeTimePointFromMilliseconds(
                  std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()
                      / std::stoi(optionMap.at(
                      CCAPI_CONFLATE_INTERVAL_MILLISECONDS)) * std::stoi(optionMap.at(
                  CCAPI_CONFLATE_INTERVAL_MILLISECONDS))) :
              tp;
      CCAPI_LOGGER_TRACE("conflateTp = " + toString(conflateTp));
      bool intervalChanged = shouldConflate
          && conflateTp
              > this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(
                  symbolId);
      CCAPI_LOGGER_TRACE("intervalChanged = " + toString(intervalChanged));
      if (!shouldConflate || intervalChanged) {
        std::vector<Element> elementList;
        if (shouldConflate && intervalChanged) {
          const std::map<Decimal, std::string>& snapshotBidPreviousPrevious = this
              ->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(
              symbolId);
          const std::map<Decimal, std::string>& snapshotAskPreviousPrevious = this
              ->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(
              symbolId);
          this->updateElementListWithUpdateMarketDepth(field, optionMap, snapshotBidPrevious,
                                                       snapshotBidPreviousPrevious, snapshotAskPrevious,
                                                       snapshotAskPreviousPrevious, elementList, false);
          this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(
              symbolId) = snapshotBidPrevious;
          this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(
              symbolId) = snapshotAskPrevious;
          CCAPI_LOGGER_TRACE("this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId) = "+toString(this->previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId)));
          CCAPI_LOGGER_TRACE("this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId) = "+toString(this->previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId)));
        } else {
          this->updateElementListWithUpdateMarketDepth(field, optionMap, snapshotBid, snapshotBidPrevious,
                                                       snapshotAsk, snapshotAskPrevious, elementList, false);
        }
        CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
        if (!elementList.empty()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::MARKET_DATA_EVENTS);
          message.setRecapType(Message::RecapType::NONE);
          TimePoint time = shouldConflate ? this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId) + std::chrono::milliseconds(std::stoll(optionMap.at(
              CCAPI_CONFLATE_INTERVAL_MILLISECONDS))) : conflateTp;
          message.setTime(time);
          message.setElementList(elementList);
          message.setCorrelationIdList(correlationIdList);
          messageList.push_back(std::move(message));
        }
        if (!messageList.empty()) {
          event.addMessages(messageList);
        }
        if (shouldConflate) {
          this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(
              symbolId) = conflateTp;
        }
      }
    }
  }
  void processTrade(const WsConnection& wsConnection, const std::string& channelId,
                               const std::string& symbolId, Event& event, bool& shouldEmitEvent, const TimePoint& tp,
                               const TimePoint& timeReceived, const MarketDataMessage::TypeForData& input,
                               const std::string& field,
                               const std::map<std::string, std::string>& optionMap,
                               const std::vector<std::string>& correlationIdList) {
    CCAPI_LOGGER_TRACE("input = " + MarketDataMessage::dataToString(input));
    std::vector<Message> messageList;
    std::vector<Element> elementList;
      this->updateElementListWithTrade(field, optionMap, input, elementList);
    CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
    if (!elementList.empty()) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::MARKET_DATA_EVENTS);
      message.setRecapType(Message::RecapType::NONE);
      message.setTime(tp);
      message.setElementList(elementList);
      message.setCorrelationIdList(correlationIdList);
      messageList.push_back(std::move(message));
    }
    if (!messageList.empty()) {
      event.addMessages(messageList);
    }
  }
  virtual void alignSnapshot(std::map<Decimal, std::string>& snapshotBid, std::map<Decimal, std::string>& snapshotAsk,
                             int marketDepthSubscribedToExchange) {
    CCAPI_LOGGER_TRACE("snapshotBid.size() = "+toString(snapshotBid.size()));
    if (snapshotBid.size() > marketDepthSubscribedToExchange) {
      keepLastN(snapshotBid, marketDepthSubscribedToExchange);
    }
    CCAPI_LOGGER_TRACE("snapshotBid.size() = "+toString(snapshotBid.size()));
    CCAPI_LOGGER_TRACE("snapshotAsk.size() = "+toString(snapshotAsk.size()));
    if (snapshotAsk.size() > marketDepthSubscribedToExchange) {
      keepFirstN(snapshotAsk, marketDepthSubscribedToExchange);
    }
    CCAPI_LOGGER_TRACE("snapshotAsk.size() = "+toString(snapshotAsk.size()));
  }
  WsConnection& getWsConnectionFromConnectionPtr(TlsClient::connection_ptr connectionPtr) {
    return this->wsConnectionMap.at(this->connectionAddressToString(connectionPtr));
  }
  std::string connectionAddressToString(const TlsClient::connection_ptr con) {
    const void * address = static_cast<const void*>(con.get());
    std::stringstream ss;
    ss << address;
    return ss.str();
  }
  void setPingPongTimer(WsConnection& wsConnection, wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (wsConnection.status == WsConnection::Status::OPEN) {
      if (this->pingTimerByConnectionIdMap.find(wsConnection.id) != this->pingTimerByConnectionIdMap.end()) {
        this->pingTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      }
      this->pingTimerByConnectionIdMap[wsConnection.id] =
          this->serviceContextPtr->tlsClientPtr->set_timer(
              this->pingIntervalMilliSeconds - this->pongTimeoutMilliSeconds,
              [wsConnection, that = shared_from_this(), hdl](ErrorCode const& ec) {
                if (that->wsConnectionMap.find(wsConnection.id) != that->wsConnectionMap.end()) {
                  if (ec) {
                    CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", connect retry on fail timer error: " + ec.message());
                    that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "timer");
                  } else {
                    if (that->wsConnectionMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                      ErrorCode ec;
                      that->ping(hdl, "", ec);
                      if (ec) {
                        that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "ping");
                      }
                      if (that->pongTimeOutTimerByConnectionIdMap.find(wsConnection.id) != that->pongTimeOutTimerByConnectionIdMap.end()) {
                        that->pongTimeOutTimerByConnectionIdMap.at(wsConnection.id)->cancel();
                      }
                      that->pongTimeOutTimerByConnectionIdMap[wsConnection.id] = that->serviceContextPtr->tlsClientPtr->set_timer(that->pongTimeoutMilliSeconds, [wsConnection, that, hdl](ErrorCode const& ec) {
                            if (that->wsConnectionMap.find(wsConnection.id) != that->wsConnectionMap.end()) {
                              if (ec) {
                                CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", pong time out timer error: " + ec.message());
                                that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "timer");
                              } else {
                                if (that->wsConnectionMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                                  auto now = std::chrono::system_clock::now();
                                  if (that->lastPongTpByConnectionIdMap.find(wsConnection.id) != that->lastPongTpByConnectionIdMap.end() &&
                                      std::chrono::duration_cast<std::chrono::milliseconds>(now - that->lastPongTpByConnectionIdMap.at(wsConnection.id)).count() >= that->pongTimeoutMilliSeconds) {
                                    auto thisWsConnection = wsConnection;
                                    ErrorCode ec;
                                    that->close(thisWsConnection, hdl, websocketpp::close::status::normal, "pong timeout", ec);
                                    if (ec) {
                                      that->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "shutdown");
                                    }
                                    that->shouldProcessRemainingMessageOnClosingByConnectionIdMap[thisWsConnection.id] = true;
                                  } else {
                                    auto thisWsConnection = wsConnection;
                                    that->setPingPongTimer(thisWsConnection, hdl);
                                  }
                                }
                              }
                            }
                          });
                    }
                  }
                }
              });
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::map<std::string, std::map<std::string, std::string> > orderBookChecksumByConnectionIdSymbolIdMap;
  virtual bool checkOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid,
                                      const std::map<Decimal, std::string>& snapshotAsk,
                                      const std::string& receivedOrderBookChecksumStr,
                                      bool& shouldProcessRemainingMessage) {
    return true;
  }
  virtual bool checkOrderBookCrossed(const std::map<Decimal, std::string>& snapshotBid,
                                     const std::map<Decimal, std::string>& snapshotAsk,
                                     bool& shouldProcessRemainingMessage) {
    if (this->sessionOptions.enableCheckOrderBookCrossed) {
      auto i1 = snapshotBid.rbegin();
      auto i2 = snapshotAsk.begin();
      if (i1 != snapshotBid.rend() && i2 != snapshotAsk.end()) {
        auto bid = i1->first;
        auto ask = i2->first;
        if (bid >= ask) {
          CCAPI_LOGGER_ERROR("bid = "+toString(bid));
          CCAPI_LOGGER_ERROR("ask = "+toString(ask));
          shouldProcessRemainingMessage = false;
          return false;
        }
      }
    }
    return true;
  }
  virtual void onIncorrectStatesFound(WsConnection& wsConnection, wspp::connection_hdl hdl,
                                      const std::string& textMessage, const TimePoint& timeReceived,
                                      const std::string& exchangeSubscriptionId, std::string const & reason) {
    std::string errorMessage = "incorrect states found: connection = "+toString(wsConnection)+
        ", textMessage = "+textMessage+
        ", timeReceived = "+UtilTime::getISOTimestamp(timeReceived)+", exchangeSubscriptionId = "+exchangeSubscriptionId+", reason = "+reason;
    CCAPI_LOGGER_ERROR(errorMessage);
    ErrorCode ec;
    this->close(wsConnection, hdl, websocketpp::close::status::normal, "incorrect states found: " + reason, ec);
    if (ec) {
      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, "shutdown");
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
    this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::INCORRECT_STATE_FOUND, errorMessage);
  }
  int calculateMarketDepthSubscribedToExchange(int depthWanted, std::vector<int> availableMarketDepth) {
    int i = ceilSearch(availableMarketDepth, 0, availableMarketDepth.size(), depthWanted);
    if (i < 0) {
      i = availableMarketDepth.size();
    }
    return availableMarketDepth[i];
  }
  void setConflateTimer(const TimePoint& previousConflateTp, const std::chrono::milliseconds& interval,
                        const std::chrono::milliseconds& gracePeriod, const WsConnection& wsConnection,
                        const std::string& channelId, const std::string& symbolId,
                        const std::string& field, const std::map<std::string, std::string>& optionMap,
                        const std::vector<std::string>& correlationIdList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (wsConnection.status == WsConnection::Status::OPEN) {
      if (this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.find(wsConnection.id)
          != this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap.end()
          && this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id].find(channelId)
              != this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id].end()
          && this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId].find(symbolId)
              != this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId].end()) {
        this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId]->cancel();
      }
      long waitMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
          previousConflateTp + interval + gracePeriod - std::chrono::system_clock::now()).count();
      if (waitMilliseconds > 0) {
        this->conflateTimerMapByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId] =
            this->serviceContextPtr->tlsClientPtr->set_timer(
                waitMilliseconds,
                [wsConnection, channelId, symbolId, field, optionMap, correlationIdList, previousConflateTp, interval, gracePeriod, this ](ErrorCode const& ec) {
                  if (this->wsConnectionMap.find(wsConnection.id) != this->wsConnectionMap.end()) {
                    if (ec) {
                      CCAPI_LOGGER_ERROR("wsConnection = " + toString(wsConnection) + ", conflate timer error: " + ec.message());
                      this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::ERROR, ec, "timer");
                    } else {
                      if (this->wsConnectionMap.at(wsConnection.id).status == WsConnection::Status::OPEN) {
                        auto conflateTp = previousConflateTp + interval;
                        if (conflateTp > this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId)) {
                          Event event;
                          event.setType(Event::Type::SUBSCRIPTION_DATA);
                          std::vector<Element> elementList;
                          std::map<Decimal, std::string>& snapshotBid = this->snapshotBidByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
                          std::map<Decimal, std::string>& snapshotAsk = this->snapshotAskByConnectionIdChannelIdSymbolIdMap[wsConnection.id][channelId][symbolId];
                          this->updateElementListWithUpdateMarketDepth(field, optionMap, snapshotBid,
                              std::map<Decimal, std::string>(), snapshotAsk,
                              std::map<Decimal, std::string>(), elementList, true);
                          CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
                          this->previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap.at(wsConnection.id).at(channelId).at(symbolId) = conflateTp;
                          std::vector<Message> messageList;
                          if (!elementList.empty()) {
                            Message message;
                            message.setTimeReceived(conflateTp);
                            message.setType(Message::Type::MARKET_DATA_EVENTS);
                            message.setRecapType(Message::RecapType::NONE);
                            message.setTime(conflateTp);
                            message.setElementList(elementList);
                            message.setCorrelationIdList(correlationIdList);
                            messageList.push_back(std::move(message));
                          }
                          if (!messageList.empty()) {
                            event.addMessages(messageList);
                            this->eventHandler(event);
                          }
                        }
                        auto now = std::chrono::system_clock::now();
                        while (conflateTp + interval + gracePeriod <= now) {
                          conflateTp += interval;
                        }
                        this->setConflateTimer(conflateTp, interval, gracePeriod, wsConnection, channelId, symbolId, field, optionMap, correlationIdList);
                      }
                    }
                  }
                });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void subscribeToExchange(const WsConnection& wsConnection) {
    std::vector<std::string> requestStringList = this->createRequestStringList(wsConnection);
    for (const auto & requestString : requestStringList) {
      CCAPI_LOGGER_INFO("requestString = "+requestString);
      ErrorCode ec;
      this->send(wsConnection.hdl, requestString, wspp::frame::opcode::text, ec);
      if (ec) {
        this->onError(Event::Type::SUBSCRIPTION_STATUS, Message::Type::SUBSCRIPTION_FAILURE, ec, "subscribe");
      }
    }
  }
  virtual std::vector<std::string> createRequestStringList(const WsConnection& wsConnection) = 0;
  virtual std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, const std::string& textMessage,
                                                           const TimePoint& timeReceived) = 0;

  std::shared_ptr<ServiceContext> serviceContextPtr;
  std::string name;
  std::map<std::string, WsConnection> wsConnectionMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::string > > > fieldByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::string> > > > optionMapByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, int> > > marketDepthSubscribedToExchangeByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::vector<Subscription>> > > subscriptionListByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::vector<std::string> > > > correlationIdListByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::string> > > channelIdSymbolIdByConnectionIdExchangeSubscriptionIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > snapshotBidByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > snapshotAskByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > previousConflateSnapshotBidByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > previousConflateSnapshotAskByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool> > > processedInitialSnapshotByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool> > > l2UpdateIsReplaceByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, bool> shouldProcessRemainingMessageOnClosingByConnectionIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, TimePoint> > > previousConflateTimeMapByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, TimerPtr> > > conflateTimerMapByConnectionIdChannelIdSymbolIdMap;
  std::map<std::string, int> connectNumRetryOnFailByConnectionUrlMap;
  std::map<std::string, TimerPtr> connectRetryOnFailTimerByConnectionIdMap;
  std::map<std::string, TimePoint> lastPongTpByConnectionIdMap;
  std::map<std::string, TimerPtr> pingTimerByConnectionIdMap;
  std::map<std::string, TimerPtr> pongTimeOutTimerByConnectionIdMap;
  bool shouldAlignSnapshot{};
  long pingIntervalMilliSeconds{};
  long pongTimeoutMilliSeconds{};
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  std::function<void(Event& event)> eventHandler;
#if defined(CCAPI_ENABLE_EXCHANGE_HUOBI) || defined(CCAPI_ENABLE_EXCHANGE_OKEX)
  struct monostate {};
  websocketpp::extensions_workaround::permessage_deflate::enabled <monostate> inflater;
#endif
  std::atomic<bool> shouldContinue{true};
  std::map<std::string, std::map<std::string, Subscription::Status> > subscriptionStatusByInstrumentGroupInstrumentMap;
  std::map<std::string, std::string> instrumentGroupByWsConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_MARKET_DATA_SERVICE_H_
