#ifndef INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BITFINEX_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BITFINEX_H_
#include "ccapi_cpp/ccapi_enable_exchange.h"
#ifdef ENABLE_BITFINEX
#include "ccapi_cpp/ccapi_market_data_service.h"
#include <regex>
namespace ccapi {
class MarketDataServiceBitfinex final : public MarketDataService {
 public:
  MarketDataServiceBitfinex(SubscriptionList subscriptionList, std::function<void(Event& event)> wsEventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs, ServiceContext& serviceContext): MarketDataService(subscriptionList, wsEventHandler, sessionOptions, sessionConfigs, serviceContext) {
    this->name = CCAPI_EXCHANGE_NAME_BITFINEX;
    this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->name);
  }

 private:
  void onOpen(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataService::onOpen(hdl);
//    this->onOpen_2(hdl);
    rj::Document document;
    document.SetObject();
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    document.AddMember("event", rj::Value("conf").Move(), allocator);
    document.AddMember("flags", rj::Value(229376).Move(), allocator);
    rj::StringBuffer stringBuffer;
    rj::Writer<rj::StringBuffer> writer(stringBuffer);
    document.Accept(writer);
    std::string requestString = stringBuffer.GetString();
    CCAPI_LOGGER_INFO("requestString = "+toString(requestString));
    ErrorCode ec;
    this->send(hdl, requestString, wspp::frame::opcode::text, ec);
    if (ec) {
      CCAPI_LOGGER_ERROR(ec.message());
      // TODO(cryptochassis): implement
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onClose(wspp::connection_hdl hdl) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(this->tlsClient->get_con_from_hdl(hdl));
    this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap.erase(wsConnection.id);
    this->sequenceByConnectionIdMap.erase(wsConnection.id);
    MarketDataService::onClose(hdl);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void onTextMessage(wspp::connection_hdl hdl, std::string textMessage, TimePoint timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataService::onTextMessage(hdl, textMessage, timeReceived);
//    this->onTextMessage_2(hdl, textMessage, timeReceived);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  std::vector<MarketDataMessage> processTextMessage(wspp::connection_hdl hdl, std::string& textMessage, TimePoint& timeReceived) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    MarketDataConnection& wsConnection = this->getMarketDataConnectionFromConnectionPtr(this->tlsClient->get_con_from_hdl(hdl));
    CCAPI_LOGGER_TRACE("wsConnection = "+toString(wsConnection));
    rj::Document document;
    rj::Document::AllocatorType& allocator = document.GetAllocator();
    std::string quotedTextMessage = std::regex_replace(textMessage, std::regex("([,\\[:])(-?\\d+\\.?\\d*e?-?\\d*)"), "$1\"$2\"");
    CCAPI_LOGGER_TRACE("quotedTextMessage = "+quotedTextMessage);
    document.Parse(quotedTextMessage.c_str());
    std::vector<MarketDataMessage> wsMessageList;
    CCAPI_LOGGER_TRACE("document.IsObject() = "+toString(document.IsObject()));
    if (document.IsArray() && document.Size() >= 1) {
      auto exchangeSubscriptionId = std::string(document[0].GetString());
      CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
      auto channelId = this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_EXCHANGE_NAME_CHANNEL_ID);
      CCAPI_LOGGER_TRACE("channelId = "+channelId);
      auto productId = this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap.at(wsConnection.id).at(exchangeSubscriptionId).at(CCAPI_EXCHANGE_NAME_PRODUCT_ID);
      CCAPI_LOGGER_TRACE("productId = "+productId);
      if (channelId.rfind(CCAPI_EXCHANGE_NAME_WEBSOCKET_BITFINEX_CHANNEL_BOOK, 0) == 0) {
        rj::Value data(rj::kArrayType);
        if (document[1].IsArray()) {
          if (this->sessionOptions.enableCheckSequence) {
            int sequence = std::stoi(document[2].GetString());
            if (!this->checkSequence(wsConnection, sequence)) {
              this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, exchangeSubscriptionId);
              return wsMessageList;
            }
          }
          const rj::Value& content = document[1].GetArray();
          if (content[0].IsArray()) {
            CCAPI_LOGGER_TRACE("SOLICITED");
            if (this->sessionOptions.enableCheckSequence) {
              int sequence = std::stoi(document[2].GetString());
              CCAPI_LOGGER_INFO("snapshot sequence is "+toString(sequence));
            }
            MarketDataMessage wsMessage;
            wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
            wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            wsMessage.recapType = MarketDataMessage::RecapType::SOLICITED;
            for (const auto& x : content.GetArray()) {
              MarketDataMessage::TypeForDataPoint dataPoint;
              dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
              auto count = std::string(x[1].GetString());
              auto amount = UtilString::normalizeDecimalString(x[2].GetString());
              if (count != "0") {
                if (amount.at(0) == '-') {
                  dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, amount.substr(1)});
                  wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
                } else {
                  dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, amount});
                  wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
                }
              } else {
                if (amount.at(0) == '-') {
                  dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, "0"});
                  wsMessage.data[MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
                } else {
                  dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, "0"});
                  wsMessage.data[MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
                }
              }
            }
            wsMessageList.push_back(std::move(wsMessage));
          } else {
            const rj::Value& x = content;
            MarketDataMessage::TypeForDataPoint dataPoint;
            dataPoint.insert({MarketDataMessage::DataFieldType::PRICE, UtilString::normalizeDecimalString(x[0].GetString())});
            auto count = std::string(x[1].GetString());
            auto amount = UtilString::normalizeDecimalString(x[2].GetString());
            if (count != "0") {
              if (amount.at(0) == '-') {
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, amount.substr(1)});
                this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
              } else {
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, amount});
                this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
              }
            } else {
              if (amount.at(0) == '-') {
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, "0"});
                this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][MarketDataMessage::DataType::ASK].push_back(std::move(dataPoint));
              } else {
                dataPoint.insert({MarketDataMessage::DataFieldType::SIZE, "0"});
                this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][MarketDataMessage::DataType::BID].push_back(std::move(dataPoint));
              }
            }
          }
        } else if (document[1].IsString()) {
          std::string str = document[1].GetString();
          if (str == "cs") {
            if (this->sessionOptions.enableCheckSequence) {
              int sequence = std::stoi(document[3].GetString());
              if (!this->checkSequence(wsConnection, sequence)) {
                this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, exchangeSubscriptionId);
                return wsMessageList;
              }
            }
            if (this->sessionOptions.enableCheckOrderBookChecksum) {
              this->orderBookChecksumByConnectionIdProductIdMap[wsConnection.id][productId] =
              intToHex(static_cast<uint_fast32_t>(static_cast<uint32_t>(std::stoi(document[2].GetString()))));
            }
            MarketDataMessage wsMessage;
            wsMessage.type = MarketDataMessage::Type::MARKET_DATA_EVENTS;
            std::string timestampInMilliseconds = document[4].GetString();
            timestampInMilliseconds.insert(timestampInMilliseconds.size() - 3, ".");
            wsMessage.tp = UtilTime::makeTimePoint(UtilTime::divide(timestampInMilliseconds));
            wsMessage.exchangeSubscriptionId = exchangeSubscriptionId;
            CCAPI_LOGGER_TRACE("NONE");
            wsMessage.recapType = MarketDataMessage::RecapType::NONE;
            std::swap(wsMessage.data, this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId]);
            wsMessageList.push_back(std::move(wsMessage));
          } else if (str == "hb") {
            CCAPI_LOGGER_DEBUG("heartbeat: "+toString(wsConnection));
            if (this->sessionOptions.enableCheckSequence) {
              int sequence = std::stoi(document[2].GetString());
              if (!this->checkSequence(wsConnection, sequence)) {
                this->onOutOfSequence(wsConnection, sequence, hdl, textMessage, timeReceived, exchangeSubscriptionId);
                return wsMessageList;
              }
            }
          }
        }
      }
    } else if (document.IsObject() && document.HasMember("event")) {
      if (std::string(document["event"].GetString()) == "conf") {
        std::vector<std::string> requestStringList;
        for (const auto & subscriptionListByChannelIdProductId : this->subscriptionListByConnectionIdChannelIdProductIdMap.at(wsConnection.id)) {
          auto channelId = subscriptionListByChannelIdProductId.first;
          if (channelId.rfind(CCAPI_EXCHANGE_NAME_WEBSOCKET_BITFINEX_CHANNEL_BOOK, 0) == 0) {
            for (auto & subscriptionListByProductId : subscriptionListByChannelIdProductId.second) {
              rj::Document document;
              document.SetObject();
              rj::Document::AllocatorType& allocator = document.GetAllocator();
              document.AddMember("event", rj::Value("subscribe").Move(), allocator);
              document.AddMember("channel", rj::Value(std::string(CCAPI_EXCHANGE_NAME_WEBSOCKET_BITFINEX_CHANNEL_BOOK).c_str(), allocator).Move(), allocator);
              auto productId = subscriptionListByProductId.first;
              document.AddMember("symbol", rj::Value(productId.c_str(), allocator).Move(), allocator);
              document.AddMember("prec", rj::Value("P0").Move(), allocator);
              document.AddMember("freq", rj::Value("F0").Move(), allocator);
              document.AddMember("len", rj::Value(std::to_string(this->marketDepthSubscribedToExchangeByConnectionIdChannelIdProductIdMap.at(wsConnection.id).at(channelId).at(productId)).c_str(), allocator).Move(), allocator);
              rj::StringBuffer stringBuffer;
              rj::Writer<rj::StringBuffer> writer(stringBuffer);
              document.Accept(writer);
              std::string requestString = stringBuffer.GetString();
              requestStringList.push_back(std::move(requestString));
              CCAPI_LOGGER_TRACE("this->subscriptionListByConnectionIdChannelProductIdMap = "+toString(this->subscriptionListByConnectionIdChannelIdProductIdMap));
            }
          }
        }
        for (const auto & requestString : requestStringList) {
          CCAPI_LOGGER_INFO("requestString = "+requestString);
          ErrorCode ec;
          this->send(hdl, requestString, wspp::frame::opcode::text, ec);
          if (ec) {
            CCAPI_LOGGER_ERROR(ec.message());
            // TODO(cryptochassis): implement
          }
        }
      } else if (std::string(document["event"].GetString()) == "subscribed") {
        auto channelId = std::string(document["channel"].GetString()) + "?" + CCAPI_EXCHANGE_NAME_MARKET_DEPTH_SUBSCRIBED_TO_EXCHANGE + "=" + std::string(document["len"].GetString());
        CCAPI_LOGGER_TRACE("channelId = "+channelId);
        auto productId = std::string(document["symbol"].GetString());
        CCAPI_LOGGER_TRACE("productId = "+productId);
        auto exchangeSubscriptionId = std::string(document["chanId"].GetString());
        CCAPI_LOGGER_TRACE("exchangeSubscriptionId = "+exchangeSubscriptionId);
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_CHANNEL_ID] = channelId;
        this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId][CCAPI_EXCHANGE_NAME_PRODUCT_ID] = productId;
        CCAPI_LOGGER_TRACE("this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap = "+toString(this->channelIdProductIdByConnectionIdExchangeSubscriptionIdMap));
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
    return wsMessageList;
  }
  bool checkSequence(const MarketDataConnection& wsConnection, int sequence) {
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) == this->sequenceByConnectionIdMap.end()) {
      if (sequence != this->sessionConfigs.getInitialSequenceByExchangeMap().at(this->name)) {
        CCAPI_LOGGER_WARN("incorrect initial sequence, wsConnection = "+toString(wsConnection));
        return false;
      }
      this->sequenceByConnectionIdMap.insert(std::pair<std::string, int>(wsConnection.id, sequence));
      return true;
    } else {
      CCAPI_LOGGER_DEBUG("sequence: previous = "+toString(this->sequenceByConnectionIdMap[wsConnection.id])+
          ", current = "+toString(sequence)+"wsConnection = "+toString(wsConnection));
      if (sequence-this->sequenceByConnectionIdMap[wsConnection.id] == 1) {
        this->sequenceByConnectionIdMap[wsConnection.id] = sequence;
        return true;
      } else {
        return false;
      }
    }
  }
  void onOutOfSequence(MarketDataConnection& wsConnection, int sequence, wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived,
      const std::string& exchangeSubscriptionId) {
    int previous = 0;
    if (this->sequenceByConnectionIdMap.find(wsConnection.id) != this->sequenceByConnectionIdMap.end()) {
      previous = this->sequenceByConnectionIdMap[wsConnection.id];
    }
    CCAPI_LOGGER_ERROR("out of sequence: previous = "+toString(previous)+
        ", current = "+toString(sequence)+
        ", connection = "+toString(wsConnection)+
        ", textMessage = "+textMessage+
        ", timeReceived = "+UtilTime::getISOTimestamp(timeReceived));
    this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId].clear();
    ErrorCode ec;
    this->close(wsConnection, hdl, websocketpp::close::status::normal, "out of sequence", ec);
    if (ec) {
      CCAPI_LOGGER_ERROR(ec.message());
    }
    this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id] = false;
  }
  std::string calculateOrderBookChecksum(
      const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk) {
    auto i = 0;
    auto i1 = snapshotBid.rbegin();
    auto i2 = snapshotAsk.begin();
    std::vector<std::string> csData;
    while (i < 25 && (i1 != snapshotBid.rend() || i2 != snapshotAsk.end())) {
      if (i1 != snapshotBid.rend()) {
        csData.push_back(toString(i1->first));
        csData.push_back(i1->second);
        ++i1;
      }
      if (i2 != snapshotAsk.end()) {
        csData.push_back(toString(i2->first));
        csData.push_back("-"+i2->second);
        ++i2;
      }
      ++i;
    }
    std::string csStr = UtilString::join(csData, ":");
    CCAPI_LOGGER_DEBUG("csStr = "+csStr);
    uint_fast32_t csCalc = UtilAlgorithm::crc(csStr.begin(), csStr.end());
    return intToHex(csCalc);
  }
  bool checkOrderBookChecksum(const std::map<Decimal, std::string>& snapshotBid, const std::map<Decimal, std::string>& snapshotAsk, const std::string& receivedOrderBookChecksumStr,
      bool& shouldProcessRemainingMessage) override {
    if (this->sessionOptions.enableCheckOrderBookChecksum) {
      std::string calculatedOrderBookChecksumStr = this->calculateOrderBookChecksum(
          snapshotBid, snapshotAsk);
      if (calculatedOrderBookChecksumStr != receivedOrderBookChecksumStr) {
        shouldProcessRemainingMessage = false;
        CCAPI_LOGGER_ERROR("calculatedOrderBookChecksumStr = "+toString(calculatedOrderBookChecksumStr));
        CCAPI_LOGGER_ERROR("receivedOrderBookChecksumStr = "+receivedOrderBookChecksumStr);
        CCAPI_LOGGER_ERROR("snapshotBid = "+toString(snapshotBid));
        CCAPI_LOGGER_ERROR("snapshotAsk = "+toString(snapshotAsk));
        return false;
      }
    }
    return true;
  }
  void onIncorrectStatesFound(MarketDataConnection& wsConnection,
      wspp::connection_hdl hdl, const std::string& textMessage, const TimePoint& timeReceived, const std::string& exchangeSubscriptionId, std::string const & reason) override {
    CCAPI_LOGGER_ERROR("incorrect states found: connection = "+toString(wsConnection)+
        ", textMessage = "+textMessage+
        ", timeReceived = "+UtilTime::getISOTimestamp(timeReceived) +", reason = "+reason);
    this->wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap[wsConnection.id][exchangeSubscriptionId].clear();
    MarketDataService::onIncorrectStatesFound(wsConnection,
        hdl, textMessage, timeReceived, exchangeSubscriptionId, reason);
  }
  std::map<std::string, std::map<std::string, MarketDataMessage::TypeForData > > wsMessageDataBufferByConnectionIdExchangeSubscriptionIdMap;
  std::map<std::string, int> sequenceByConnectionIdMap;
};
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_SERVICE_BITFINEX_H_
