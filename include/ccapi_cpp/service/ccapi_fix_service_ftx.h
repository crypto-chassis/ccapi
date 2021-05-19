#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/ccapi_hmac.h"
#include "ccapi_cpp/service/ccapi_service.h"
#include "hffix.hpp"
namespace hff = hffix;
namespace ccapi {
class FixServiceFtx : public Service {
 public:
  FixServiceFtx(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                ServiceContextPtr serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_FTX;
    this->baseUrlFix = this->sessionConfigs.getUrlFixBase().at(this->exchangeName);
    this->setHostFixFromUrlFix(this->baseUrlFix);
    this->apiKeyName = CCAPI_FTX_API_KEY;
    this->apiSecretName = CCAPI_FTX_API_SECRET;
    this->apiSubaccountName = CCAPI_FTX_API_SUBACCOUNT;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiSubaccountName});
    try {
      this->tcpResolverResults = this->resolver.resolve(this->hostFix, this->portFix);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    this->protocolVersion = CCAPI_FIX_PROTOCOL_VERSION_FTX;
    this->targetCompID = "FTX";
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~FixServiceFtx() {}

 protected:
  static std::string printableString(const char* s, size_t n) {
    std::string output(s, n);
    std::replace(output.begin(), output.end(), '\x01', '^');
    return output;
  }
  static std::string printableString(const std::string& s) {
    std::string output(s);
    std::replace(output.begin(), output.end(), '\x01', '^');
    return output;
  }
  void setHostFixFromUrlFix(std::string baseUrlFix) {
    auto hostPort = this->extractHostFromUrl(baseUrlFix);
    this->hostFix = hostPort.first;
    this->portFix = hostPort.second;
  }
  void subscribeByFix(const Subscription& subscription) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrlFix = " + this->baseUrlFix);
    if (this->shouldContinue.load()) {
      wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [that = shared_from_base<FixServiceFtx>(), subscription]() {
        auto thatSubscription = subscription;
        that->connect(thatSubscription);
      });
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onFail(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& errorMessage) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->clearStates(fixConnectionPtr);
    this->onFail_(fixConnectionPtr, errorMessage);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void clearStates(std::shared_ptr<FixConnection> fixConnectionPtr) {
    CCAPI_LOGGER_INFO("clear states for fixConnectionPtr " + toString(*fixConnectionPtr));
    auto& connectionId = fixConnectionPtr->id;
    this->readMessageBufferByConnectionIdMap.erase(connectionId);
    this->readMessageBufferReadLengthByConnectionIdMap.erase(connectionId);
    this->writeMessageBufferByConnectionIdMap.erase(connectionId);
    this->writeMessageBufferWrittenLengthByConnectionIdMap.erase(connectionId);
    this->fixConnectionPtrByConnectionIdMap.erase(connectionId);
    this->sequenceSentByConnectionIdMap.erase(connectionId);
    this->credentialByConnectionIdMap.erase(connectionId);
    auto urlBase = fixConnectionPtr->url;
    this->connectNumRetryOnFailByConnectionUrlMap.erase(urlBase);
  }
  void onFail_(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& errorMessage) {
    fixConnectionPtr->status = FixConnection::Status::FAILED;
    this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, errorMessage, {fixConnectionPtr->subscription.getCorrelationId()});
    auto urlBase = fixConnectionPtr->url;
    CCAPI_LOGGER_TRACE("urlBase = " + urlBase);
    CCAPI_LOGGER_TRACE("this->connectNumRetryOnFailByConnectionUrlMap = " + toString(this->connectNumRetryOnFailByConnectionUrlMap));
    long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[urlBase], 6)));
    CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(fixConnectionPtr->id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(fixConnectionPtr->id)->cancel();
    }
    this->connectRetryOnFailTimerByConnectionIdMap[fixConnectionPtr->id] = this->serviceContextPtr->tlsClientPtr->set_timer(
        seconds * 1000, [fixConnectionPtr, that = shared_from_base<FixServiceFtx>(), urlBase](ErrorCode const& ec) {
          if (that->fixConnectionPtrByConnectionIdMap.find(fixConnectionPtr->id) == that->fixConnectionPtrByConnectionIdMap.end()) {
            if (ec) {
              CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", connect retry on fail timer error: " + ec.message());
              that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
            } else {
              CCAPI_LOGGER_INFO("about to retry");
              that->connect(fixConnectionPtr->subscription);
              that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
            }
          }
        });
  }
  void connect(Subscription& subscription) {
    std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(nullptr);
    try {
      streamPtr = this->createStream(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, this->hostFix);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, ec, "create stream", {subscription.getCorrelationId()});
      return;
    }
    std::shared_ptr<FixConnection> fixConnectionPtr(new FixConnection(this->hostFix, this->portFix, subscription, streamPtr));
    fixConnectionPtr->status = FixConnection::Status::CONNECTING;
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::ssl_stream<beast::tcp_stream>& stream = *streamPtr;
    beast::get_lowest_layer(stream).async_connect(this->tcpResolverResults,
                                                  beast::bind_front_handler(&FixServiceFtx::onConnect_3, shared_from_base<FixServiceFtx>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_connect");
  }
  void onConnect_3(std::shared_ptr<FixConnection> fixConnectionPtr, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    auto now = UtilTime::now();
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(fixConnectionPtr, "connect");
      return;
    }
    CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
    CCAPI_LOGGER_TRACE("connected");
    beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&FixServiceFtx::onHandshake_3, shared_from_base<FixServiceFtx>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake_3(std::shared_ptr<FixConnection> fixConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    auto now = UtilTime::now();
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(fixConnectionPtr, "handshake");
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    auto& connectionId = fixConnectionPtr->id;
    fixConnectionPtr->status = FixConnection::Status::OPEN;
    CCAPI_LOGGER_INFO("connection " + toString(*fixConnectionPtr) + " established");
    this->connectNumRetryOnFailByConnectionUrlMap[fixConnectionPtr->url] = 0;
    {
      Event event;
      event.setType(Event::Type::SESSION_STATUS);
      Message message;
      message.setTimeReceived(now);
      message.setType(Message::Type::SESSION_CONNECTION_UP);
      message.setCorrelationIdList({fixConnectionPtr->subscription.getCorrelationId()});
      Element element(true);
      element.insert(CCAPI_CONNECTION_ID, connectionId);
      message.setElementList({element});
      event.setMessageList({message});
      this->eventHandler(event);
    }
    this->fixConnectionPtrByConnectionIdMap.insert({connectionId, fixConnectionPtr});
    auto credential = fixConnectionPtr->subscription.getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->credentialByConnectionIdMap[connectionId] = credential;
    auto& readMessageBuffer = this->readMessageBufferByConnectionIdMap[connectionId];
    auto& readMessageBufferReadLength = this->readMessageBufferReadLengthByConnectionIdMap[connectionId];
    CCAPI_LOGGER_TRACE("about to start read");
    CCAPI_LOGGER_TRACE("readMessageBufferReadLength = " + toString(readMessageBufferReadLength));
    this->startRead_3(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                      std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize));
    std::map<int, std::string> logonOptionMap;
    for (const auto& x : fixConnectionPtr->subscription.getOptionMap()) {
      if (UtilString::isNumber(x.first)) {
        logonOptionMap.insert({std::stoi(x.first), x.second});
      }
    }
    auto param = this->createLogonParam(connectionId, nowFixTimeStr, logonOptionMap);
    this->writeMessage(fixConnectionPtr, nowFixTimeStr, {param});
  }
  void startRead_3(std::shared_ptr<FixConnection> fixConnectionPtr, void* data, size_t requestedNumBytesToRead) {
    beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    CCAPI_LOGGER_TRACE("requestedNumBytesToRead = " + toString(requestedNumBytesToRead));
    stream.async_read_some(boost::asio::buffer(data, requestedNumBytesToRead),
                           beast::bind_front_handler(&FixServiceFtx::onRead_3, shared_from_base<FixServiceFtx>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead_3(std::shared_ptr<FixConnection> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("n = " + toString(n));
    auto now = UtilTime::now();
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(fixConnectionPtr, "read");
      return;
    }
    if (fixConnectionPtr->status != FixConnection::Status::OPEN) {
      CCAPI_LOGGER_WARN("should not process remaining message on closing");
      return;
    }
    auto& connectionId = fixConnectionPtr->id;
    auto& readMessageBuffer = this->readMessageBufferByConnectionIdMap[connectionId];
    auto& readMessageBufferReadLength = this->readMessageBufferReadLengthByConnectionIdMap[connectionId];
    readMessageBufferReadLength += n;
    CCAPI_LOGGER_TRACE("readMessageBufferReadLength = " + toString(readMessageBufferReadLength));
    hff::message_reader reader(readMessageBuffer.data(), readMessageBuffer.data() + readMessageBufferReadLength);
    std::vector<std::string> correlationIdList{fixConnectionPtr->subscription.getCorrelationId()};
    for (; reader.is_complete(); reader = reader.next_message_reader()) {
      Event event;
      bool shouldEmitEvent = true;
      Message message;
      Element element(true);
      message.setTimeReceived(now);
      message.setCorrelationIdList(correlationIdList);
      if (reader.is_valid()) {
        try {
          CCAPI_LOGGER_TRACE("recevied " + printableString(reader.message_begin(), reader.message_end() - reader.message_begin()));
          auto it = reader.message_type();
          auto messageType = it->value().as_string();
          CCAPI_LOGGER_DEBUG("received a " + messageType + " message");
          if (messageType == "0") {
            shouldEmitEvent = false;
            CCAPI_LOGGER_DEBUG("heartbeat: " + toString(*fixConnectionPtr));
          } else if (messageType == "1") {
            shouldEmitEvent = false;
            if (reader.find_with_hint(hff::tag::TestReqID, it)) {
              this->writeMessage(fixConnectionPtr, nowFixTimeStr,
                                 {{
                                     {hff::tag::MsgType, "0"},
                                     {hff::tag::TestReqID, it->value().as_string()},
                                 }});
            }
          } else {
            it = it + 5;
            while (it->tag() != hffix::tag::CheckSum) {
              element.insert(it->tag(), it->value().as_string());
              ++it;
            }
            if (reader.find_with_hint(hff::tag::MsgSeqNum, it) && it->value().as_string() == "1") {
              if (messageType == "A") {
                event.setType(Event::Type::AUTHORIZATION_STATUS);
                message.setType(Message::Type::AUTHORIZATION_SUCCESS);
                this->setPingPongTimer(
                    PingPongMethod::FIX_PROTOCOL_LEVEL, fixConnectionPtr,
                    [that = shared_from_base<FixServiceFtx>()](std::shared_ptr<FixConnection> fixConnectionPtr) {
                      auto now = UtilTime::now();
                      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
                      that->writeMessage(
                          fixConnectionPtr, nowFixTimeStr,
                          {
                              {
                                  {hff::tag::MsgType, "0"},
                              },
                              {
                                  {hff::tag::MsgType, "1"},
                                  {hff::tag::TestReqID, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())},
                              },
                          });
                    });
              } else {
                event.setType(Event::Type::AUTHORIZATION_STATUS);
                message.setType(Message::Type::AUTHORIZATION_FAILURE);
              }
            } else {
              event.setType(Event::Type::FIX);
              message.setType(Message::Type::FIX);
            }
          }
        } catch (const std::exception& e) {
          std::string errorMessage(std::string("Error reading fields: ") + e.what());
          element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
          event.setType(Event::Type::FIX_STATUS);
          message.setType(Message::Type::GENERIC_ERROR);
        }
      } else {
        std::string errorMessage("Error Invalid FIX message: ");
        errorMessage.append(reader.message_begin(), std::min(ssize_t(64), readMessageBuffer.data() + readMessageBufferReadLength - reader.message_begin()));
        errorMessage.append("...");
        element.insert(CCAPI_ERROR_MESSAGE, errorMessage);
        event.setType(Event::Type::FIX_STATUS);
        message.setType(Message::Type::GENERIC_ERROR);
      }
      if (shouldEmitEvent) {
        message.setElementList({element});
        event.setMessageList({message});
        this->eventHandler(event);
      }
    }
    readMessageBufferReadLength = reader.buffer_end() - reader.buffer_begin();
    if (readMessageBufferReadLength >= this->readMessageChunkSize) {
      std::memmove(readMessageBuffer.data(), reader.buffer_begin(), readMessageBufferReadLength);
      CCAPI_LOGGER_TRACE("about to start read");
      this->startRead_3(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                        std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize));
    } else {
      CCAPI_LOGGER_TRACE("about to start read");
      this->startRead_3(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                        std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize - readMessageBufferReadLength));
    }
    this->onPongByMethod(PingPongMethod::FIX_PROTOCOL_LEVEL, fixConnectionPtr, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void startWrite_3(std::shared_ptr<FixConnection> fixConnectionPtr, void* data, size_t numBytesToWrite) {
    beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_write");
    CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
    boost::asio::async_write(stream, boost::asio::buffer(data, numBytesToWrite),
                             beast::bind_front_handler(&FixServiceFtx::onWrite_3, shared_from_base<FixServiceFtx>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite_3(std::shared_ptr<FixConnection> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto& connectionId = fixConnectionPtr->id;
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    writeMessageBufferWrittenLength -= n;
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    if (writeMessageBufferWrittenLength > 0) {
      std::memmove(writeMessageBuffer.data(), writeMessageBuffer.data() + n, writeMessageBufferWrittenLength);
      CCAPI_LOGGER_TRACE("about to start write");
      this->startWrite_3(fixConnectionPtr, writeMessageBuffer.data(), writeMessageBufferWrittenLength);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void writeMessage(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& nowFixTimeStr,
                    const std::vector<std::vector<std::pair<int, std::string>>>& paramList) {
    if (fixConnectionPtr->status != FixConnection::Status::OPEN) {
      CCAPI_LOGGER_WARN("should write no more messages");
      return;
    }
    auto& connectionId = fixConnectionPtr->id;
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    size_t n = 0;
    for (const auto& param : paramList) {
      auto commonParam = this->createCommonParam(connectionId, nowFixTimeStr);
      hff::message_writer messageWriter(writeMessageBuffer.data() + n, writeMessageBuffer.data() + writeMessageBuffer.size());
      messageWriter.push_back_header(this->protocolVersion.c_str());
      auto it = param.begin();
      if (it != param.end()) {
        messageWriter.push_back_string(it->first, it->second);
        for (const auto& x : commonParam) {
          messageWriter.push_back_string(x.first, x.second);
        }
        ++it;
        while (it != param.end()) {
          messageWriter.push_back_string(it->first, it->second);
          ++it;
        }
      }
      messageWriter.push_back_trailer();
      n += messageWriter.message_end() - messageWriter.message_begin();
    }
    CCAPI_LOGGER_TRACE("about to send " + printableString(writeMessageBuffer.data(), n));
    if (writeMessageBufferWrittenLength == 0) {
      CCAPI_LOGGER_TRACE("about to start write");
      this->startWrite_3(fixConnectionPtr, writeMessageBuffer.data(), n);
    }
    writeMessageBufferWrittenLength += n;
  }
  virtual std::vector<std::pair<int, std::string>> createCommonParam(const std::string& connectionId, const std::string& nowFixTimeStr) {
    return {
        {hff::tag::SenderCompID, mapGetWithDefault(this->credentialByConnectionIdMap[connectionId], this->apiKeyName)},
        {hff::tag::TargetCompID, this->targetCompID},
        {hff::tag::MsgSeqNum, std::to_string(++this->sequenceSentByConnectionIdMap[connectionId])},
        {hff::tag::SendingTime, nowFixTimeStr},
    };
  }
  virtual std::vector<std::pair<int, std::string>> createLogonParam(const std::string& connectionId, const std::string& nowFixTimeStr,
                                                                    const std::map<int, std::string> logonOptionMap = {}) {
    std::vector<std::pair<int, std::string>> param;
    auto msgType = "A";
    param.push_back({hff::tag::MsgType, msgType});
    param.push_back({hff::tag::EncryptMethod, "0"});
    param.push_back({hff::tag::HeartBtInt, std::to_string(this->sessionOptions.heartbeatFixIntervalMilliSeconds / 1000)});
    auto msgSeqNum = std::to_string(this->sequenceSentByConnectionIdMap[connectionId] + 1);
    auto credential = this->credentialByConnectionIdMap[connectionId];
    auto senderCompID = mapGetWithDefault(credential, this->apiKeyName);
    auto targetCompID = this->targetCompID;
    std::vector<std::string> prehashFieldList{nowFixTimeStr, msgType, msgSeqNum, senderCompID, targetCompID};
    auto prehashStr = UtilString::join(prehashFieldList, "\x01");
    CCAPI_LOGGER_TRACE("prehashStr = " + printableString(prehashStr));
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    std::string rawData = UtilString::toLower(UtilAlgorithm::stringToHex(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, prehashStr)));
    CCAPI_LOGGER_TRACE("rawData = " + rawData);
    param.push_back({hff::tag::RawData, rawData});
    for (const auto& x : logonOptionMap) {
      param.push_back({x.first, x.second});
    }
    auto apiSubaccountName = mapGetWithDefault(credential, this->apiSubaccountName);
    if (!apiSubaccountName.empty()) {
      param.push_back({hff::tag::Account, apiSubaccountName});
    }
    return param;
  }
  void onPongByMethod(PingPongMethod method, std::shared_ptr<FixConnection> fixConnectionPtr, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->lastPongTpByMethodByConnectionIdMap[fixConnectionPtr->id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void setupCredential(std::vector<std::string> nameList) {
    for (const auto& x : nameList) {
      if (this->sessionConfigs.getCredential().find(x) != this->sessionConfigs.getCredential().end()) {
        this->credentialDefault.insert(std::make_pair(x, this->sessionConfigs.getCredential().at(x)));
      } else if (!UtilSystem::getEnvAsString(x).empty()) {
        this->credentialDefault.insert(std::make_pair(x, UtilSystem::getEnvAsString(x)));
      }
    }
  }
  void sendRequestByFix(const Request& request, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("now = " + toString(now));
    wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [that = shared_from_base<FixServiceFtx>(), request]() {
      auto now = UtilTime::now();
      CCAPI_LOGGER_DEBUG("request = " + toString(request));
      CCAPI_LOGGER_TRACE("now = " + toString(now));
      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
      auto& connectionId = request.getCorrelationId();
      auto it = that->fixConnectionPtrByConnectionIdMap.find(connectionId);
      if (it == that->fixConnectionPtrByConnectionIdMap.end()) {
        Event event;
        event.setType(Event::Type::FIX_STATUS);
        Message message;
        message.setTimeReceived(now);
        message.setType(Message::Type::FIX_FAILURE);
        message.setCorrelationIdList({request.getCorrelationId()});
        Element element(true);
        element.insert(CCAPI_ERROR_MESSAGE, "FIX connection was not found");
        message.setElementList({element});
        event.setMessageList({message});
        that->eventHandler(event);
        return;
      }
      auto& fixConnectionPtr = it->second;
      CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
      that->writeMessage(fixConnectionPtr, nowFixTimeStr, request.getParamListFix());
    });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void setPingPongTimer(PingPongMethod method, std::shared_ptr<FixConnection> fixConnectionPtr,
                        std::function<void(std::shared_ptr<FixConnection>)> pingMethod) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliSeconds = this->pingIntervalMilliSecondsByMethodMap[method];
    auto pongTimeoutMilliSeconds = this->pongTimeoutMilliSecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliSeconds = " + toString(pingIntervalMilliSeconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliSeconds = " + toString(pongTimeoutMilliSeconds));
    if (pingIntervalMilliSeconds <= pongTimeoutMilliSeconds) {
      return;
    }
    if (fixConnectionPtr->status == FixConnection::Status::OPEN) {
      if (this->pingTimerByMethodByConnectionIdMap.find(fixConnectionPtr->id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
              this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method)->cancel();
      }
      this->pingTimerByMethodByConnectionIdMap[fixConnectionPtr->id][method] = this->serviceContextPtr->tlsClientPtr->set_timer(
          pingIntervalMilliSeconds - pongTimeoutMilliSeconds,
          [fixConnectionPtr, that = shared_from_base<FixServiceFtx>(), pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
            if (that->fixConnectionPtrByConnectionIdMap.find(fixConnectionPtr->id) != that->fixConnectionPtrByConnectionIdMap.end()) {
              if (ec) {
                CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", ping timer error: " + ec.message());
                that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
              } else {
                if (that->fixConnectionPtrByConnectionIdMap.at(fixConnectionPtr->id)->status == FixConnection::Status::OPEN) {
                  ErrorCode ec;
                  pingMethod(fixConnectionPtr);
                  if (ec) {
                    that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "ping");
                  }
                  if (pongTimeoutMilliSeconds <= 0) {
                    return;
                  }
                  if (that->pongTimeOutTimerByMethodByConnectionIdMap.find(fixConnectionPtr->id) != that->pongTimeOutTimerByMethodByConnectionIdMap.end() &&
                      that->pongTimeOutTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
                          that->pongTimeOutTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).end()) {
                    that->pongTimeOutTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method)->cancel();
                  }
                  that->pongTimeOutTimerByMethodByConnectionIdMap[fixConnectionPtr->id][method] = that->serviceContextPtr->tlsClientPtr->set_timer(
                      pongTimeoutMilliSeconds, [fixConnectionPtr, that, pingMethod, pongTimeoutMilliSeconds, method](ErrorCode const& ec) {
                        if (that->fixConnectionPtrByConnectionIdMap.find(fixConnectionPtr->id) != that->fixConnectionPtrByConnectionIdMap.end()) {
                          if (ec) {
                            CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", pong time out timer error: " + ec.message());
                            that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                          } else {
                            if (that->fixConnectionPtrByConnectionIdMap.at(fixConnectionPtr->id)->status == FixConnection::Status::OPEN) {
                              auto now = UtilTime::now();
                              if (that->lastPongTpByMethodByConnectionIdMap.find(fixConnectionPtr->id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                                  that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
                                      that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).end() &&
                                  std::chrono::duration_cast<std::chrono::milliseconds>(
                                      now - that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method))
                                          .count() >= pongTimeoutMilliSeconds) {
                                auto thisFixConnectionPtr = fixConnectionPtr;
                                ErrorCode ec;
                                that->onFail(fixConnectionPtr, "pong timeout");
                                if (ec) {
                                  that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "shutdown");
                                }
                              } else {
                                auto thisFixConnectionPtr = fixConnectionPtr;
                                that->setPingPongTimer(method, thisFixConnectionPtr, pingMethod);
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
  const size_t readMessageChunkSize = CCAPI_HFFIX_READ_MESSAGE_CHUNK_SIZE;
  std::map<std::string, std::array<char, 1 << 20>> readMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> readMessageBufferReadLengthByConnectionIdMap;
  std::map<std::string, std::array<char, 1 << 20>> writeMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> writeMessageBufferWrittenLengthByConnectionIdMap;
  std::map<std::string, std::shared_ptr<FixConnection>> fixConnectionPtrByConnectionIdMap;
  std::map<std::string, int> sequenceSentByConnectionIdMap;
  std::map<std::string, std::map<std::string, std::string>> credentialByConnectionIdMap;
  std::string baseUrlFix;
  std::string apiKeyName;
  std::string apiSecretName;
  std::string hostFix;
  std::string portFix;
  std::string protocolVersion;
  std::string targetCompID;

  std::string apiSubaccountName;
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_FTX_H_
