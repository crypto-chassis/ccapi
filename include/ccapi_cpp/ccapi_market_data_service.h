#ifndef INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_H_

#ifndef WEBSOCKETPP_USE_BOOST_ASIO
#define ASIO_STANDALONE
#endif
#ifndef RAPIDJSON_ASSERT
#define RAPIDJSON_ASSERT(x) if (!(x)) { throw std::runtime_error("rapidjson internal assertion failure"); }
#endif
#include <string>
#include <map>
#include <vector>
#include <functional>
#include "websocketpp/common/connection_hdl.hpp"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_subscription_list.h"
#include "ccapi_cpp/ccapi_exchange.h"
#include "ccapi_cpp/ccapi_market_data_message.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "websocketpp/config/asio_client.hpp"
#include "websocketpp/client.hpp"
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util.h"
#include "ccapi_cpp/ccapi_decimal.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_market_data_connection.h"
#include "ccapi_cpp/ccapi_service_context.h"
#if defined(ENABLE_HUOBI) || defined(ENABLE_OKEX)
#include <sstream>
#include <iomanip>
#include "ccapi_cpp/websocketpp_decompress_workaround.h"
#endif
namespace wspp = websocketpp;
namespace rj = rapidjson;
namespace ccapi {
class MarketDataService {
 public:
  MarketDataService(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler,
                  SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContext& serviceContext)
      : subscriptionList(subscriptionList),
        wsEventHandler(wsEventHandler),
        sessionOptions(sessionOptions),
        sessionConfigs(sessionConfigs) {
    this->tlsClient = &serviceContext.tlsClient;
    this->tlsClient->set_tls_init_handler(std::bind(&MarketDataService::onTlsInit, std::placeholders::_1));
    CCAPI_LOGGER_DEBUG("endpoint tls init handler set");
    this->pingIntervalMilliSeconds = sessionOptions.pingIntervalMilliSeconds;
    CCAPI_LOGGER_INFO("this->pingIntervalMilliSeconds = "+toString(this->pingIntervalMilliSeconds));
    this->pongTimeoutMilliSeconds = sessionOptions.pongTimeoutMilliSeconds;
    CCAPI_LOGGER_INFO("this->pongTimeoutMilliSeconds = "+toString(this->pongTimeoutMilliSeconds));
  }
  virtual ~MarketDataService() {
  }
  virtual std::map<std::string, SubscriptionList> groupSubscriptionListByUrl(const SubscriptionList& subscriptionList) {
    return {{ this->baseUrl, subscriptionList }};
  }
  void connect() {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrl = "+this->baseUrl);
    for (const auto & x : this->groupSubscriptionListByUrl(this->subscriptionList)) {
      auto wsConnectionMapGivenUrl = this->buildMarketDataConnectionMap(x.first, x.second);
      this->wsConnectionMap.insert(wsConnectionMapGivenUrl.begin(), wsConnectionMapGivenUrl.end());
    }
    std::map<std::string, MarketDataConnection> actualWebsocketwsConnectionMap;
    for (const auto & x : this->wsConnectionMap) {
      auto wsConnectionId = x.first;
      CCAPI_LOGGER_DEBUG("dummy wsConnectionId = "+wsConnectionId);
      auto wsConnection = x.second;
      CCAPI_LOGGER_DEBUG("wsConnection = "+toString(wsConnection));
      this->connect(wsConnection);
      CCAPI_LOGGER_DEBUG("wsConnectionId "+wsConnectionId+" requested");
      actualWebsocketwsConnectionMap.insert(std::pair<std::string, MarketDataConnection>(wsConnection.id, wsConnection));
    }
    this->wsConnectionMap = actualWebsocketwsConnectionMap;
    CCAPI_LOGGER_INFO("actual connection map is "+toString(this->wsConnectionMap));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  const std::string& getBaseUrl() const {
    return baseUrl;
  }
  std::map<std::string, MarketDataConnection> buildMarketDataConnectionMap(
      std::string url, const SubscriptionList& subscriptionList) {
    if (this->sessionOptions.enableOneConnectionPerSubscription) {
      std::map<std::string, MarketDataConnection> wsConnectionMap;
      CCAPI_LOGGER_TRACE("subscriptionList = "+toString(subscriptionList));
      for (const auto & subscription : subscriptionList.getSubscriptionList()) {
        SubscriptionList subSubscriptionList;
        subSubscriptionList.add(subscription);
        MarketDataConnection wsConnection(url, subSubscriptionList);
        wsConnectionMap.insert(std::pair<std::string, MarketDataConnection>(wsConnection.id, wsConnection));
      }
      CCAPI_LOGGER_TRACE("wsConnectionMap = "+toString(wsConnectionMap));
      return wsConnectionMap;
    } else {
      MarketDataConnection wsConnection(url, subscriptionList);
      return { {wsConnection.id, wsConnection}};
    }
  }

 protected:
  typedef wspp::lib::shared_ptr<wspp::lib::asio::ssl::context> SslContext;
//  struct CustomClientConfig : public wspp::config::asio_tls_client {
//    static const wspp::log::level alog_level = wspp::log::alevel::none;
//    static const wspp::log::level elog_level = wspp::log::elevel::none;
//  };
  typedef ServiceContext::TlsClient TlsClient;
  typedef wspp::lib::error_code ErrorCode;
  typedef wspp::lib::shared_ptr<wspp::lib::asio::steady_timer> TimerPtr;
  typedef wspp::lib::function<void(ErrorCode const &)> TimerHandler;
  static SslContext onTlsInit(wspp::connection_hdl hdl) {
    SslContext ctx = std::make_shared<wspp::lib::asio::ssl::context>(wspp::lib::asio::ssl::context::sslv23);
    ctx->set_options(
        wspp::lib::asio::ssl::context::default_workarounds | wspp::lib::asio::ssl::context::no_sslv2 | wspp::lib::asio::ssl::context::no_sslv3
            | wspp::lib::asio::ssl::context::single_dh_use);
    ctx->set_verify_mode(wspp::lib::asio::ssl::verify_none);
    // TODO(cryptochassis): verify ssl certificate to strengthen security
    // https://github.com/boostorg/asio/blob/develop/example/cpp03/ssl/client.cpp
    return ctx;
  }
//  SslContext onTlsInit(wspp::connection_hdl hdl) {
//    return MarketDataService::onTlsInitStatic(hdl);
//  }
  virtual void onOpen(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = std::chrono::system_clock::now();
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(
        this->tlsClient->get_con_from_hdl(hdl));
    wsConnection.status = MarketDataConnection::Status::OPEN;
    CCAPI_LOGGER_INFO("connection " +toString(wsConnection) + " established");
    this->connectNumRetryOnFailByConnectionUrlMap[wsConnection.url] = 0;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    Element element;
    element.insert(CCAPI_EXCHANGE_NAME_CONNECTION, toString(wsConnection));
    message.setElementList({ element });
    event.setMessageList({ message });
    this->wsEventHandler(event);
    if (this->sessionOptions.enableCheckHeartbeat) {
      this->setPingPongTimer(wsConnection, hdl);
    }
    CCAPI_LOGGER_TRACE("this->subscriptionList = "+toString(this->subscriptionList));
//    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(
//        this->tlsClient->get_con_from_hdl(hdl));
    for (const auto & subscription : this->wsConnectionMap.at(wsConnection.id).subscriptionList.getSubscriptionList()) {
      auto instrument = subscription.getInstrument();
      auto productId = this->sessionConfigs.getExchangeInstrumentSymbolMap().at(this->name).at(instrument);
      auto fieldSet = subscription.getFieldSet();
      auto optionMap = subscription.getOptionMap();
      for (auto & field : fieldSet) {
        std::string channelId = this->sessionConfigs.getExchangeFieldWebsocketChannelMap().at(this->name).at(field);
        if (field == CCAPI_EXCHANGE_NAME_MARKET_DEPTH) {
          if (this->name == CCAPI_EXCHANGE_NAME_KRAKEN || this->name == CCAPI_EXCHANGE_NAME_BITFINEX
              || this->name == CCAPI_EXCHANGE_NAME_BINANCE_US || this->name == CCAPI_EXCHANGE_NAME_BINANCE || this->name == CCAPI_EXCHANGE_NAME_BINANCE_FUTURES  || this->name == CCAPI_EXCHANGE_NAME_HUOBI || this->name == CCAPI_EXCHANGE_NAME_OKEX) {
            int marketDepthSubscribedToExchange = 1;
            marketDepthSubscribedToExchange = this->calculateMarketDepthSubscribedToExchange(
                std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX)),
                this->sessionConfigs.getWebsocketAvailableMarketDepth().at(this->name));
            channelId += std::string("?") + CCAPI_EXCHANGE_NAME_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "="
                + std::to_string(marketDepthSubscribedToExchange);
            this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] =
                marketDepthSubscribedToExchange;
          } else if (this->name == CCAPI_EXCHANGE_NAME_GEMINI) {
            if (optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX) == "1") {
              int marketDepthSubscribedToExchange = 1;
              channelId += std::string("?") + CCAPI_EXCHANGE_NAME_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "="
                  + std::to_string(marketDepthSubscribedToExchange);
              this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] =
                  marketDepthSubscribedToExchange;
            }
          } else if (this->name == CCAPI_EXCHANGE_NAME_BITMEX) {
            if (std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX)) == 1) {
              channelId = CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_QUOTE;
            } else if (std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX)) == 10) {
              channelId =
              CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_10;
            } else if (std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX)) == 25) {
              channelId =
              CCAPI_EXCHANGE_NAME_WEBSOCKET_BITMEX_CHANNEL_ORDER_BOOK_L2_25;
            }
          }
        }
        this->subscriptionListByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId].add(
            subscription);
        this->correlationIdListByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId].push_back(
            subscription.getCorrelationId());
        this->fieldSetByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId].insert(
            fieldSet.begin(), fieldSet.end());
        this->optionMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId].insert(
            optionMap.begin(), optionMap.end());
      }
    }
    CCAPI_LOGGER_TRACE("this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap = "+toString(this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap));
    CCAPI_LOGGER_TRACE("this->correlationIdListByConnectionIdChannelProductIdMap = "+toString(this->correlationIdListByConnectionIdChannelIdProductIdMap));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onFail(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(
        this->tlsClient->get_con_from_hdl(hdl));
    wsConnection.status = MarketDataConnection::Status::FAILED;
    CCAPI_LOGGER_ERROR("connection " + toString(wsConnection) + " has failed before opening");
    MarketDataConnection thisWsConnection = wsConnection;
    this->wsConnectionMap.erase(thisWsConnection.id);
    long seconds = std::round(
        UtilAlgorithm::exponentialBackoff(
            1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[thisWsConnection.url], 6)));
    CCAPI_LOGGER_INFO("about to set timer for "+toString(seconds)+" seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(thisWsConnection.id)
        != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(thisWsConnection.id)->cancel();
    }
    this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] =
        this->tlsClient->set_timer(
            seconds * 1000,
            [thisWsConnection, this](ErrorCode const& ec) {
              if (this->wsConnectionMap.find(thisWsConnection.id) == this->wsConnectionMap.end()) {
                if (ec) {
                  CCAPI_LOGGER_ERROR("wsConnection = "+toString(thisWsConnection)+", connect retry on fail timer error: "+ec.message());
                } else {
                  CCAPI_LOGGER_INFO("about to retry");
                  auto thatWsConnection = thisWsConnection;
                  thatWsConnection.assignDummyId();
                  this->connect(thatWsConnection);
                  this->wsConnectionMap.insert(std::pair<std::string, MarketDataConnection>(thatWsConnection.id, thatWsConnection));
                  this->connectNumRetryOnFailByConnectionUrlMap[thatWsConnection.url] += 1;
                }
              }
            });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onClose(wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = std::chrono::system_clock::now();
    TlsClient::connection_ptr con = this->tlsClient->get_con_from_hdl(hdl);
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(con);
    wsConnection.status = MarketDataConnection::Status::CLOSED;
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
    element.insert(CCAPI_EXCHANGE_NAME_CONNECTION, toString(wsConnection));
    element.insert(CCAPI_EXCHANGE_NAME_REASON, reason);
    message.setElementList({ element });
    event.setMessageList({ message });
    this->wsEventHandler(event);
    CCAPI_LOGGER_INFO("connection " +toString(wsConnection) + " is closed");
    CCAPI_LOGGER_INFO("clear states for wsConnection "+toString(wsConnection));
    this->fieldSetByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->optionMapByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->subscriptionListByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->correlationIdListByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap.erase(wsConnection.id);
    this->snapshotBidByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->snapshotAskByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->processedInitialSnapshotByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap.erase(wsConnection.id);
    this->lastPongTpByConnectionIdMap.erase(wsConnection.id);
    this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
    this->conflateTimerMapByConnectionIdChannelIdProductIdMap.erase(wsConnection.id);
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
    this->orderBookChecksumByConnectionIdProductIdMap.erase(wsConnection.id);
    MarketDataConnection thisWsConnection = wsConnection;
    this->wsConnectionMap.erase(thisWsConnection.id);
    thisWsConnection.assignDummyId();
    this->connect(thisWsConnection);
    this->wsConnectionMap.insert(std::pair<std::string, MarketDataConnection>(thisWsConnection.id, thisWsConnection));
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onMessage(wspp::connection_hdl hdl, TlsClient::message_ptr msg) {
    auto now = std::chrono::system_clock::now();
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(
        this->tlsClient->get_con_from_hdl(hdl));
    CCAPI_LOGGER_DEBUG("received a message from connection "+toString(wsConnection));
    if (wsConnection.status == MarketDataConnection::Status::CLOSING
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
        CCAPI_LOGGER_ERROR(e.what());
        CCAPI_LOGGER_ERROR("textMessage = "+textMessage);
      }
    } else if (opcode == websocketpp::frame::opcode::binary) {
#if defined(ENABLE_HUOBI) || defined(ENABLE_OKEX)
      if (this->name == CCAPI_EXCHANGE_NAME_HUOBI || this->name == CCAPI_EXCHANGE_NAME_OKEX) {
        std::string decompressed;
        std::string payload = msg->get_payload();
        try {
          //  why need to init for each message instead of only once, because otherwise it doesn't work
          ErrorCode ec1 = this->deflate.init(false);
          if (ec1) {
            CCAPI_LOGGER_FATAL(ec1.message());
          }
          ErrorCode ec2 = this->deflate.decompress(reinterpret_cast<const uint8_t*>(&payload[0]), payload.size(), decompressed);
          if (ec2) {
            CCAPI_LOGGER_FATAL(ec2.message());
          }
          CCAPI_LOGGER_DEBUG("decompressed = "+decompressed);
          this->onTextMessage(hdl, decompressed, now);
        } catch (const std::exception& e) {
          CCAPI_LOGGER_ERROR(e.what());
          std::stringstream ss;
          ss << std::hex << std::setfill('0');
          for (int i = 0; i < payload.size(); ++i) {
              ss << std::setw(2) << static_cast<unsigned>(reinterpret_cast<const uint8_t*>(&payload[0])[i]);
          }
          CCAPI_LOGGER_ERROR("binaryMessage = "+ss.str());
        }
      }
#endif
    }
  }
  void onPong(wspp::connection_hdl hdl, std::string payload) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = std::chrono::system_clock::now();
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(
        this->tlsClient->get_con_from_hdl(hdl));
    lastPongTpByConnectionIdMap[wsConnection.id] = now;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onTextMessage(wspp::connection_hdl hdl, std::string textMessage, TimePoint timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(
        this->tlsClient->get_con_from_hdl(hdl));
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
              this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_EXCHANGE_NAME_CHANNEL_ID);
          std::string productId =
              this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_EXCHANGE_NAME_PRODUCT_ID);
          auto fieldSet = this->fieldSetByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId);
          CCAPI_LOGGER_TRACE("this->optionMapByConnectionIdChannelIdProductIdMap = "+toString(this->optionMapByConnectionIdChannelIdProductIdMap));
          CCAPI_LOGGER_TRACE("wsConnection = "+toString(wsConnection));
          CCAPI_LOGGER_TRACE("channelId = "+toString(channelId));
          CCAPI_LOGGER_TRACE("productId = "+toString(productId));
          auto optionMap = this->optionMapByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId);
          CCAPI_LOGGER_TRACE("optionMap = "+toString(optionMap));
          auto correlationIdList =
              this->correlationIdListByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId);
          CCAPI_LOGGER_TRACE("correlationIdList = "+toString(correlationIdList));
          if (wsMessage.data.find(MarketDataMessage::DataType::BID) != wsMessage.data.end()
              || wsMessage.data.find(MarketDataMessage::DataType::ASK) != wsMessage.data.end()) {
            std::map<Decimal, std::string>& snapshotBid =
                this->snapshotBidByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
            std::map<Decimal, std::string>& snapshotAsk =
                this->snapshotAskByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
            if (this->processedInitialSnapshotByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId]
                && wsMessage.recapType == MarketDataMessage::RecapType::NONE) {
              this->processUpdateSnapshot(wsConnection, channelId, productId, event, shouldEmitEvent, wsMessage.tp,
                                            timeReceived, wsMessage.data, fieldSet, optionMap, correlationIdList,
                                            snapshotBid, snapshotAsk);
              if (this->sessionOptions.enableCheckOrderBookChecksum) {
                bool shouldProcessRemainingMessage = true;
                std::string receivedOrderBookChecksumStr =
                    this->orderBookChecksumByConnectionIdProductIdMap[wsConnection.id][productId];
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
              this->processInitialSnapshot(wsConnection, channelId, productId, event, shouldEmitEvent, wsMessage.tp,
                                             timeReceived, wsMessage.data, fieldSet, optionMap, correlationIdList,
                                             snapshotBid, snapshotAsk);
            }
            CCAPI_LOGGER_TRACE("snapshotBid.size() = "+toString(snapshotBid.size()));
            CCAPI_LOGGER_TRACE("snapshotAsk.size() = "+toString(snapshotAsk.size()));
          }
          if (wsMessage.data.find(MarketDataMessage::DataType::TRADE) != wsMessage.data.end()) {
            this->processTrade(wsConnection, channelId, productId, event, shouldEmitEvent, wsMessage.tp,
                                                        timeReceived, wsMessage.data, fieldSet, optionMap, correlationIdList);
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
            this->wsEventHandler(event);
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
  void updateElementListWithInitialMarketDepth(const std::set<std::string>& fieldSet,
                                               const std::map<std::string, std::string>& optionMap,
                                               const std::map<Decimal, std::string>& snapshotBid,
                                               const std::map<Decimal, std::string>& snapshotAsk,
                                               std::vector<Element>& elementList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (fieldSet.find(CCAPI_EXCHANGE_NAME_MARKET_DEPTH) != fieldSet.end()) {
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
      int bidIndex = 0;
      for (auto iter = snapshotBid.rbegin(); iter != snapshotBid.rend(); iter++) {
        if (bidIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_PRICE, iter->first.toString());
          element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_SIZE, iter->second);
          elementList.push_back(std::move(element));
        }
        ++bidIndex;
      }
      if (snapshotBid.empty()) {
        Element element;
        element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_PRICE,
        CCAPI_EXCHANGE_NAME_BEST_BID_N_PRICE_EMPTY);
        element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_SIZE,
        CCAPI_EXCHANGE_NAME_BEST_BID_N_SIZE_EMPTY);
        elementList.push_back(std::move(element));
      }
      int askIndex = 0;
      for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); iter++) {
        if (askIndex < maxMarketDepth) {
          Element element;
          element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_PRICE, iter->first.toString());
          element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_SIZE, iter->second);
          elementList.push_back(std::move(element));
        }
        ++askIndex;
      }
      if (snapshotAsk.empty()) {
        Element element;
        element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_PRICE,
        CCAPI_EXCHANGE_NAME_BEST_ASK_N_PRICE_EMPTY);
        element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_SIZE,
        CCAPI_EXCHANGE_NAME_BEST_ASK_N_SIZE_EMPTY);
        elementList.push_back(std::move(element));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void updateElementListWithUpdateMarketDepth(const std::set<std::string>& fieldSet,
                                              const std::map<std::string, std::string>& optionMap,
                                              const std::map<Decimal, std::string>& snapshotBid,
                                              const std::map<Decimal, std::string>& snapshotBidPrevious,
                                              const std::map<Decimal, std::string>& snapshotAsk,
                                              const std::map<Decimal, std::string>& snapshotAskPrevious,
                                              std::vector<Element>& elementList,
                                              bool alwaysUpdate) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (fieldSet.find(CCAPI_EXCHANGE_NAME_MARKET_DEPTH) != fieldSet.end()) {
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
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
          element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_PRICE, iter->first.toString());
          element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_SIZE, iter->second);
          elementList.push_back(std::move(element));
          ++bidIndex;
        }
        if (snapshotBid.empty()) {
          Element element;
          element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_PRICE,
          CCAPI_EXCHANGE_NAME_BEST_BID_N_PRICE_EMPTY);
          element.insert(CCAPI_EXCHANGE_NAME_BEST_BID_N_SIZE,
          CCAPI_EXCHANGE_NAME_BEST_BID_N_SIZE_EMPTY);
          elementList.push_back(std::move(element));
        }
        int askIndex = 0;
        for (auto iter = snapshotAsk.begin(); iter != snapshotAsk.end(); ++iter) {
          if (askIndex >= maxMarketDepth) {
            break;
          }
          Element element;
          element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_PRICE, iter->first.toString());
          element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_SIZE, iter->second);
          elementList.push_back(std::move(element));
          ++askIndex;
        }
        if (snapshotAsk.empty()) {
          Element element;
          element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_PRICE,
          CCAPI_EXCHANGE_NAME_BEST_ASK_N_PRICE_EMPTY);
          element.insert(CCAPI_EXCHANGE_NAME_BEST_ASK_N_SIZE,
          CCAPI_EXCHANGE_NAME_BEST_ASK_N_SIZE_EMPTY);
          elementList.push_back(std::move(element));
        }
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void updateElementListWithTrade(const std::set<std::string>& fieldSet,
                                              const std::map<std::string, std::string>& optionMap,
                                              const MarketDataMessage::TypeForData& input,
                                              std::vector<Element>& elementList) {
    for (const auto & x : input) {
      auto type = x.first;
      auto detail = x.second;
      if (type == MarketDataMessage::DataType::TRADE) {
        for (const auto & y : detail) {
          auto price = y.at(MarketDataMessage::DataFieldType::PRICE);
          auto size = y.at(MarketDataMessage::DataFieldType::SIZE);
          Element element;
          element.insert(CCAPI_EXCHANGE_NAME_LAST_PRICE, y.at(MarketDataMessage::DataFieldType::PRICE));
          element.insert(CCAPI_EXCHANGE_NAME_LAST_SIZE, y.at(MarketDataMessage::DataFieldType::SIZE));
          element.insert(CCAPI_EXCHANGE_NAME_TRADE_ID, y.at(MarketDataMessage::DataFieldType::TRADE_ID));
          element.insert(CCAPI_EXCHANGE_NAME_IS_BUYER_MAKER, y.at(MarketDataMessage::DataFieldType::IS_BUYER_MAKER));
          elementList.push_back(std::move(element));
        }
      } else {
        CCAPI_LOGGER_WARN(
            "extra type " + MarketDataMessage::dataTypeToString(type));
      }
    }
  }
//    std::pair<Decimal, std::string> getBestBid(const std::map<Decimal, std::string>& snapshotBid){
//      if (!snapshotBid.empty()) {
//        return std::make_pair(snapshotBid.rbegin()->first, snapshotBid.rbegin()->second);
//      } else {
//        return std::make_pair(Decimal("0"), "");
//      }
//    }
//    std::pair<Decimal, std::string> getBestAsk(const std::map<Decimal, std::string>& snapshotAsk){
//      if (!snapshotAsk.empty()) {
//        return std::make_pair(snapshotAsk.begin()->first, snapshotAsk.begin()->second);
//      } else {
//        return std::make_pair(Decimal("0"), "");
//      }
//    }
  void connect(MarketDataConnection& wsConnection) {
    wsConnection.status = MarketDataConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("connection initialization on dummy id "+wsConnection.id);
    std::string url = wsConnection.url;
    ErrorCode ec;
    TlsClient::connection_ptr con = this->tlsClient->get_connection(url, ec);
    wsConnection.id = this->connectionAddressToString(con);
    CCAPI_LOGGER_DEBUG("connection initialization on actual id "+wsConnection.id);
    if (ec) {
      CCAPI_LOGGER_FATAL("connection initialization error: " + ec.message());
    }
    con->set_open_handler(std::bind(&MarketDataService::onOpen, this, std::placeholders::_1));
    con->set_fail_handler(std::bind(&MarketDataService::onFail, this, std::placeholders::_1));
    con->set_close_handler(std::bind(&MarketDataService::onClose, this, std::placeholders::_1));
    con->set_message_handler(
        std::bind(&MarketDataService::onMessage, this, std::placeholders::_1, std::placeholders::_2));
    if (this->sessionOptions.enableCheckHeartbeat) {
      con->set_pong_handler(std::bind(&MarketDataService::onPong, this, std::placeholders::_1, std::placeholders::_2));
    }
    this->tlsClient->connect(con);
  }
  void close(MarketDataConnection& wsConnection, wspp::connection_hdl hdl, wspp::close::status::value const code,
             std::string const & reason, ErrorCode & ec) {
    if (wsConnection.status == MarketDataConnection::Status::CLOSING) {
      CCAPI_LOGGER_WARN("websocket connection is already in the state of closing");
      return;
    }
    wsConnection.status = MarketDataConnection::Status::CLOSING;
    this->tlsClient->close(hdl, code, reason, ec);
  }
  void send(wspp::connection_hdl hdl, std::string const & payload, wspp::frame::opcode::value op, ErrorCode & ec) {
    this->tlsClient->send(hdl, payload, op, ec);
  }
  void ping(wspp::connection_hdl hdl, std::string const & payload, ErrorCode & ec) {
    this->tlsClient->ping(hdl, payload, ec);
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
  void processInitialSnapshot(const MarketDataConnection& wsConnection, const std::string& channelId,
                                const std::string& productId, Event& event, bool& shouldEmitEvent, const TimePoint& tp,
                                const TimePoint& timeReceived, const MarketDataMessage::TypeForData & input,
                                const std::set<std::string>& fieldSet,
                                const std::map<std::string, std::string>& optionMap,
                                const std::vector<CorrelationId>& correlationIdList,
                                std::map<Decimal, std::string>& snapshotBid,
                                std::map<Decimal, std::string>& snapshotAsk) {
    snapshotBid.clear();
    snapshotAsk.clear();
    int maxMarketDepth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
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
    this->updateElementListWithInitialMarketDepth(fieldSet, optionMap, snapshotBid, snapshotAsk, elementList);
    if (!elementList.empty()) {
      Message message;
      message.setTimeReceived(timeReceived);
      message.setType(Message::Type::MARKET_DATA_EVENTS);
      message.setRecapType(Message::RecapType::SOLICITED);
      message.setElementList(elementList);
      message.setCorrelationIdList(correlationIdList);
      std::vector<Message> newMessageList = { message };
      event.addMessages(newMessageList);
      CCAPI_LOGGER_TRACE("event.getMessageList() = "+toString(event.getMessageList()));
    }
    this->processedInitialSnapshotByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] = true;
    bool shouldConflate = optionMap.at(
    CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_EXCHANGE_VALUE_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
    if (shouldConflate) {
      this->copySnapshot(
          true, snapshotBid,
          this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId],
          maxMarketDepth);
      this->copySnapshot(
          false, snapshotAsk,
          this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId],
          maxMarketDepth);
      CCAPI_LOGGER_TRACE("this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId) = "+toString(this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId)));
      CCAPI_LOGGER_TRACE("this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId) = "+toString(this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId)));
      TimePoint previousConflateTp = UtilTime::makeTimePointFromMilliseconds(
          std::chrono::duration_cast<std::chrono::milliseconds>(tp - TimePoint(std::chrono::seconds(0))).count()
              / std::stoi(optionMap.at(
              CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS)) * std::stoi(optionMap.at(
          CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS)));
      this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] =
          previousConflateTp;
      if (optionMap.at(
          CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS) != CCAPI_EXCHANGE_VALUE_CONFLATE_GRACE_PERIOD_MILLISECONDS_DEFAULT) {
        auto interval = std::chrono::milliseconds(std::stoi(optionMap.at(
        CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS)));
        auto gracePeriod = std::chrono::milliseconds(std::stoi(optionMap.at(
        CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS)));
        this->setConflateTimer(previousConflateTp, interval, gracePeriod, wsConnection, channelId, productId, fieldSet,
                               optionMap, correlationIdList);
      }
    }
  }
  void processUpdateSnapshot(const MarketDataConnection& wsConnection, const std::string& channelId,
                               const std::string& productId, Event& event, bool& shouldEmitEvent, const TimePoint& tp,
                               const TimePoint& timeReceived, const MarketDataMessage::TypeForData& input,
                               const std::set<std::string>& fieldSet,
                               const std::map<std::string, std::string>& optionMap,
                               const std::vector<CorrelationId>& correlationIdList,
                               std::map<Decimal, std::string>& snapshotBid,
                               std::map<Decimal, std::string>& snapshotAsk) {
    CCAPI_LOGGER_TRACE("input = " + MarketDataMessage::dataToString(input));
    if (this->processedInitialSnapshotByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId]) {
      std::vector<Message> messageList;
      CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
      int maxMarketDepth = std::stoi(optionMap.at(CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX));
      std::map<Decimal, std::string> snapshotBidPrevious;
      this->copySnapshot(true, snapshotBid, snapshotBidPrevious, maxMarketDepth);
      std::map<Decimal, std::string> snapshotAskPrevious;
      this->copySnapshot(false, snapshotAsk, snapshotAskPrevious, maxMarketDepth);
      CCAPI_LOGGER_TRACE("before updating orderbook");
      CCAPI_LOGGER_TRACE(
          "lastNToString(snapshotBid, "+toString(maxMarketDepth)+") = " + lastNToString(snapshotBid, maxMarketDepth));
      CCAPI_LOGGER_TRACE(
          "firstNToString(snapshotAsk, "+toString(maxMarketDepth)+") = " + firstNToString(snapshotAsk, maxMarketDepth));
      if (this->l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId]) {
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
          "this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap = "
          + toString(
              this
              ->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap));
      if (this->shouldAlignSnapshot) {
        int marketDepthSubscribedToExchange = this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap
            .at(wsConnection.id).at(channelId).at(productId);
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
      CCAPI_LOGGER_TRACE("fieldSet = " + toString(fieldSet));
      CCAPI_LOGGER_TRACE("maxMarketDepth = " + toString(maxMarketDepth));
      CCAPI_LOGGER_TRACE("optionMap = " + toString(optionMap));
      bool shouldConflate = optionMap.at(
      CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS) != CCAPI_EXCHANGE_VALUE_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
      CCAPI_LOGGER_TRACE("shouldConflate = " + toString(shouldConflate));
      TimePoint conflateTp =
          shouldConflate ?
              UtilTime::makeTimePointFromMilliseconds(
                  std::chrono::duration_cast<std::chrono::milliseconds>(tp - TimePoint(std::chrono::seconds(0))).count()
                      / std::stoi(optionMap.at(
                      CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS)) * std::stoi(optionMap.at(
                  CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS))) :
              tp;
      CCAPI_LOGGER_TRACE("conflateTp = " + toString(conflateTp));
      bool intervalChanged = shouldConflate
          && conflateTp
              > this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(
                  productId);
      CCAPI_LOGGER_TRACE("intervalChanged = " + toString(intervalChanged));
      if (!shouldConflate || intervalChanged) {
        std::vector<Element> elementList;
        if (shouldConflate && intervalChanged) {
          const std::map<Decimal, std::string>& snapshotBidPreviousPrevious = this
              ->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(
              productId);
          const std::map<Decimal, std::string>& snapshotAskPreviousPrevious = this
              ->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(
              productId);
          this->updateElementListWithUpdateMarketDepth(fieldSet, optionMap, snapshotBidPrevious,
                                                       snapshotBidPreviousPrevious, snapshotAskPrevious,
                                                       snapshotAskPreviousPrevious, elementList, false);
          this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(
              productId) = snapshotBidPrevious;
          this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(
              productId) = snapshotAskPrevious;
          CCAPI_LOGGER_TRACE("this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId) = "+toString(this->previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId)));
          CCAPI_LOGGER_TRACE("this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId) = "+toString(this->previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId)));
        } else {
          this->updateElementListWithUpdateMarketDepth(fieldSet, optionMap, snapshotBid, snapshotBidPrevious,
                                                       snapshotAsk, snapshotAskPrevious, elementList, false);
        }
        CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
        if (!elementList.empty()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setType(Message::Type::MARKET_DATA_EVENTS);
          message.setRecapType(Message::RecapType::NONE);
          TimePoint time = shouldConflate ? this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId) + std::chrono::milliseconds(std::stoll(optionMap.at(
              CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS))) : conflateTp;
          message.setTime(time);
//          message.setTime(conflateTp);
          message.setElementList(elementList);
          message.setCorrelationIdList(correlationIdList);
          messageList.push_back(std::move(message));
        }
        if (!messageList.empty()) {
          event.addMessages(messageList);
        }
        if (shouldConflate) {
          this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(
              productId) = conflateTp;
        }
      }
    }
  }
  void processTrade(const MarketDataConnection& wsConnection, const std::string& channelId,
                               const std::string& productId, Event& event, bool& shouldEmitEvent, const TimePoint& tp,
                               const TimePoint& timeReceived, const MarketDataMessage::TypeForData& input,
                               const std::set<std::string>& fieldSet,
                               const std::map<std::string, std::string>& optionMap,
                               const std::vector<CorrelationId>& correlationIdList) {
    CCAPI_LOGGER_TRACE("input = " + MarketDataMessage::dataToString(input));
    std::vector<Message> messageList;
    std::vector<Element> elementList;
      this->updateElementListWithTrade(fieldSet, optionMap, input, elementList);
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
  virtual std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage,
                                                           TimePoint& timeReceived) {
    std::vector<MarketDataMessage> x;
    return x;
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
  MarketDataConnection& getMarketDataConnectionFromConnectionPtr(TlsClient::connection_ptr connectionPtr) {
    return this->wsConnectionMap.at(this->connectionAddressToString(connectionPtr));
  }
  std::string connectionAddressToString(const TlsClient::connection_ptr con) {
    const void * address = static_cast<const void*>(con.get());
    std::stringstream ss;
    ss << address;
    return ss.str();
  }
  void setPingPongTimer(MarketDataConnection& wsConnection, wspp::connection_hdl hdl) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (wsConnection.status == MarketDataConnection::Status::OPEN) {
      if (this->pingTimerByConnectionIdMap.find(wsConnection.id) != this->pingTimerByConnectionIdMap.end()) {
        this->pingTimerByConnectionIdMap.at(wsConnection.id)->cancel();
      }
      this->pingTimerByConnectionIdMap[wsConnection.id] =
          this->tlsClient->set_timer(
              this->pingIntervalMilliSeconds - this->pongTimeoutMilliSeconds,
              [wsConnection, this, hdl](ErrorCode const& ec) {
                if (this->wsConnectionMap.find(wsConnection.id) != this->wsConnectionMap.end()) {
                  if (ec) {
                    CCAPI_LOGGER_ERROR("wsConnection = "+toString(wsConnection)+", ping timer error: "+ec.message());
                  } else {
                    if (this->wsConnectionMap.at(wsConnection.id).status == MarketDataConnection::Status::OPEN) {
                      ErrorCode ec;
                      this->ping(hdl, "", ec);
                      if (ec) {
                        CCAPI_LOGGER_ERROR(ec.message());
                      }
                      if (this->pongTimeOutTimerByConnectionIdMap.find(wsConnection.id) != this->pongTimeOutTimerByConnectionIdMap.end()) {
                        this->pongTimeOutTimerByConnectionIdMap.at(wsConnection.id)->cancel();
                      }
                      this->pongTimeOutTimerByConnectionIdMap[wsConnection.id] = this->tlsClient->set_timer(this->pongTimeoutMilliSeconds, [wsConnection, this, hdl](ErrorCode const& ec) {
                            if (this->wsConnectionMap.find(wsConnection.id) != this->wsConnectionMap.end()) {
                              if (ec) {
                                CCAPI_LOGGER_ERROR("wsConnection = "+toString(wsConnection)+", pong time out timer error: "+ec.message());
                              } else {
                                if (this->wsConnectionMap.at(wsConnection.id).status == MarketDataConnection::Status::OPEN) {
                                  auto now = std::chrono::system_clock::now();
                                  if (this->lastPongTpByConnectionIdMap.find(wsConnection.id) != this->lastPongTpByConnectionIdMap.end() &&
                                      std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastPongTpByConnectionIdMap.at(wsConnection.id)).count() >= this->pongTimeoutMilliSeconds) {
                                    auto thisWsConnection = wsConnection;
                                    ErrorCode ec;
                                    this->close(thisWsConnection, hdl, websocketpp::close::status::normal, "pong timeout", ec);
                                    if (ec) {
                                      CCAPI_LOGGER_ERROR(ec.message());
                                    }
                                    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[thisWsConnection.id] = true;
                                  } else {
                                    auto thisWsConnection = wsConnection;
                                    this->setPingPongTimer(thisWsConnection, hdl);
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
  std::map<std::string, std::map<std::string, std::string> > orderBookChecksumByConnectionIdProductIdMap;
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
  virtual void onIncorrectStatesFound(MarketDataConnection& wsConnection, wspp::connection_hdl hdl,
                                      const std::string& textMessage, const TimePoint& timeReceived,
                                      const std::string& exchangeSubscriptionId, std::string const & reason) {
    std::string errorMessage = "incorrect states found: connection = "+toString(wsConnection)+
        ", textMessage = "+textMessage+
        ", timeReceived = "+UtilTime::getISOTimestamp(timeReceived)+", exchangeSubscriptionId = "+exchangeSubscriptionId+", reason = "+reason;
    CCAPI_LOGGER_ERROR(errorMessage);
    ErrorCode ec;
    this->close(wsConnection, hdl, websocketpp::close::status::normal, "incorrect states found: " + reason, ec);
    if (ec) {
      CCAPI_LOGGER_ERROR(ec.message());
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTime(timeReceived);
    message.setType(Message::Type::SESSION_INCORRECT_STATES_FOUND);
    Element element;
    element.insert(CCAPI_EXCHANGE_NAME_ERROR_MESSAGE, errorMessage);
    message.setElementList({ element });
    event.setMessageList({ message });
    this->wsEventHandler(event);
  }
  int calculateMarketDepthSubscribedToExchange(int depthWanted, std::vector<int> availableMarketDepth) {
    int i = ceilSearch(availableMarketDepth, 0, availableMarketDepth.size(), depthWanted);
    if (i < 0) {
      i = availableMarketDepth.size();
    }
    return availableMarketDepth[i];
  }
  void setConflateTimer(const TimePoint& previousConflateTp, const std::chrono::milliseconds& interval,
                        const std::chrono::milliseconds& gracePeriod, const MarketDataConnection& wsConnection,
                        const std::string& channelId, const std::string& productId,
                        const std::set<std::string>& fieldSet, const std::map<std::string, std::string>& optionMap,
                        const std::vector<CorrelationId>& correlationIdList) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (wsConnection.status == MarketDataConnection::Status::OPEN) {
      if (this->conflateTimerMapByConnectionIdChannelIdProductIdMap.find(wsConnection.id)
          != this->conflateTimerMapByConnectionIdChannelIdProductIdMap.end()
          && this->conflateTimerMapByConnectionIdChannelIdProductIdMap[wsConnection.id].find(channelId)
              != this->conflateTimerMapByConnectionIdChannelIdProductIdMap[wsConnection.id].end()
          && this->conflateTimerMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId].find(productId)
              != this->conflateTimerMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId].end()) {
        this->conflateTimerMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId]->cancel();
      }
      long waitMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
          previousConflateTp + interval + gracePeriod - std::chrono::system_clock::now()).count();
      if (waitMilliseconds > 0) {
        this->conflateTimerMapByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId] =
            this->tlsClient->set_timer(
                waitMilliseconds,
                [wsConnection, channelId, productId, fieldSet, optionMap, correlationIdList, previousConflateTp, interval, gracePeriod, this ](ErrorCode const& ec) {
                  if (this->wsConnectionMap.find(wsConnection.id) != this->wsConnectionMap.end()) {
                    if (ec) {
                      CCAPI_LOGGER_ERROR("wsConnection = "+toString(wsConnection)+", conflate timer error: "+ec.message());
                      //      }
                    } else {
                      if (this->wsConnectionMap.at(wsConnection.id).status == MarketDataConnection::Status::OPEN) {
                        auto conflateTp = previousConflateTp + interval;
                        if (conflateTp > this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId)) {
                          Event event;
                          event.setType(Event::Type::SUBSCRIPTION_DATA);
                          std::vector<Element> elementList;
                          std::map<Decimal, std::string>& snapshotBid = this->snapshotBidByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
                          std::map<Decimal, std::string>& snapshotAsk = this->snapshotAskByConnectionIdChannelIdProductIdMap[wsConnection.id][channelId][productId];
                          this->updateElementListWithUpdateMarketDepth(fieldSet, optionMap, snapshotBid,
                              std::map<Decimal, std::string>(), snapshotAsk,
                              std::map<Decimal, std::string>(), elementList, true);
                          CCAPI_LOGGER_TRACE("elementList = " + toString(elementList));
                          this->previousConflateTimeMapByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId) = conflateTp;
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
                            this->wsEventHandler(event);
                          }
                        }
                        auto now = std::chrono::system_clock::now();
                        while (conflateTp + interval + gracePeriod <= now) {
                          conflateTp += interval;
                        }
                        this->setConflateTimer(conflateTp, interval, gracePeriod, wsConnection, channelId, productId, fieldSet, optionMap, correlationIdList);
                      }
                    }
                  }
                });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  TlsClient* tlsClient;
  std::string baseUrl;
  std::string name;
  std::map<std::string, MarketDataConnection> wsConnectionMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string> > > > fieldSetByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::string> > > > optionMapByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, int> > > marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, SubscriptionList> > > subscriptionListByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::vector<CorrelationId> > > > correlationIdListByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::string> > > channelIdProductIdByConnectionIdExchangeSubscriptionIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > snapshotBidByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > snapshotAskByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > previousConflateSnapshotBidByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, std::map<Decimal, std::string> > > > previousConflateSnapshotAskByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool> > > processedInitialSnapshotByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, bool> > > l2UpdateIsReplaceByConnectionIdChannelIdProductIdMap;
  std::map<std::string, bool> shouldProcessRemainingMessageOnClosingByConnectionIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, TimePoint> > > previousConflateTimeMapByConnectionIdChannelIdProductIdMap;
  std::map<std::string, std::map<std::string, std::map<std::string, TimerPtr> > > conflateTimerMapByConnectionIdChannelIdProductIdMap;
  std::map<std::string, int> connectNumRetryOnFailByConnectionUrlMap;
  std::map<std::string, TimerPtr> connectRetryOnFailTimerByConnectionIdMap;
  std::map<std::string, TimePoint> lastPongTpByConnectionIdMap;
  std::map<std::string, TimerPtr> pingTimerByConnectionIdMap;
  std::map<std::string, TimerPtr> pongTimeOutTimerByConnectionIdMap;
  bool shouldAlignSnapshot{};
  long pingIntervalMilliSeconds{};
  long pongTimeoutMilliSeconds{};
  SubscriptionList subscriptionList;
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
//  ServiceContext& serviceContext;
  std::function<void(Event& event)> wsEventHandler;
#if defined(ENABLE_HUOBI) || defined(ENABLE_OKEX)
  struct monostate {};
  websocketpp::extensions_workaround::permessage_deflate::enabled <monostate> deflate;
#endif
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_H_
