#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifndef CCAPI_FIX_READ_BUFFER_SIZE
#define CCAPI_FIX_READ_BUFFER_SIZE 1 << 20
#endif
#ifndef CCAPI_FIX_WRITE_BUFFER_SIZE
#define CCAPI_FIX_WRITE_BUFFER_SIZE 1 << 20
#endif
#include "ccapi_cpp/service/ccapi_service.h"
#include "hffix.hpp"
namespace hff = hffix;
namespace ccapi {
/**
 * Defines a service which provides access to exchange API and normalizes them. This is a base class that implements generic functionalities for dealing with
 * exchange FIX APIs. The Session object is responsible for routing requests and subscriptions to the desired concrete service.
 */
template <class T>
class FixService : public Service {
 public:
  FixService(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
             ServiceContextPtr serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {}
  virtual ~FixService() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 protected:
#endif
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
  void setHostFixFromUrlFix(std::string& aHostFix, std::string& aPortFix, const std::string& baseUrlFix) {
    auto hostPort = this->extractHostFromUrl(baseUrlFix);
    aHostFix = hostPort.first;
    aPortFix = hostPort.second;
  }
  void subscribeByFix(Subscription& subscription) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrlFix = " + this->baseUrlFix);
    if (this->shouldContinue.load()) {
      boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<FixService>(), subscription]() {
        auto now = UtilTime::now();
        auto thatSubscription = subscription;
        thatSubscription.setTimeSent(now);
        that->connect(thatSubscription);
      });
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onFail(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const std::string& errorMessage) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_ERROR("errorMessage = " + errorMessage);
    this->clearStates(fixConnectionPtr);
    this->onFail_(fixConnectionPtr, errorMessage);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual void onFail(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const ErrorCode& ec, const std::string& what) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->onFail(fixConnectionPtr, what + ": " + ec.message() + ", category: " + ec.category().name());
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void clearStates(std::shared_ptr<FixConnection<T>> fixConnectionPtr) {
    CCAPI_LOGGER_INFO("clear states for fixConnectionPtr " + toString(*fixConnectionPtr));
    auto& connectionId = fixConnectionPtr->id;
    this->readMessageBufferByConnectionIdMap.erase(connectionId);
    this->readMessageBufferReadLengthByConnectionIdMap.erase(connectionId);
    this->writeMessageBufferByConnectionIdMap.erase(connectionId);
    this->writeMessageBufferWrittenLengthByConnectionIdMap.erase(connectionId);
    this->fixConnectionPtrByIdMap.erase(connectionId);
    this->sequenceSentByConnectionIdMap.erase(connectionId);
    this->credentialByConnectionIdMap.erase(connectionId);
    auto urlBase = fixConnectionPtr->url;
    this->connectNumRetryOnFailByConnectionUrlMap.erase(urlBase);
  }
  void onFail_(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const std::string& errorMessage) {
    fixConnectionPtr->status = FixConnection<T>::Status::FAILED;
    this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, errorMessage, {fixConnectionPtr->subscription.getCorrelationId()});
    auto urlBase = fixConnectionPtr->url;
    CCAPI_LOGGER_TRACE("urlBase = " + urlBase);
    CCAPI_LOGGER_TRACE("this->connectNumRetryOnFailByConnectionUrlMap = " + toString(this->connectNumRetryOnFailByConnectionUrlMap));
    long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[urlBase], 6)));
    CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(fixConnectionPtr->id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(fixConnectionPtr->id)->cancel();
    }
    TimerPtr timerPtr(new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(seconds * 1000)));
    timerPtr->async_wait([fixConnectionPtr, that = shared_from_base<FixService>(), urlBase](ErrorCode const& ec) {
      if (that->fixConnectionPtrByIdMap.find(fixConnectionPtr->id) == that->fixConnectionPtrByIdMap.end()) {
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
    this->connectRetryOnFailTimerByConnectionIdMap[fixConnectionPtr->id] = timerPtr;
  }
  std::shared_ptr<T> createStreamFix(net::io_context* iocPtr, net::ssl::context* ctxPtr, const std::string& host);
  void connect(Subscription& subscription) {
    std::string aHostFix = this->hostFix;
    std::string aPortFix = this->portFix;
    std::string field = subscription.getField();
    if (field == CCAPI_FIX_MARKET_DATA) {
      aHostFix = this->hostFixMarketData;
      aPortFix = this->portFixMarketData;
    } else if (field == CCAPI_FIX_EXECUTION_MANAGEMENT) {
      aHostFix = this->hostFixExecutionManagement;
      aPortFix = this->portFixExecutionManagement;
    }
    std::shared_ptr<T> streamPtr(nullptr);
    try {
      streamPtr = this->createStreamFix(this->serviceContextPtr->ioContextPtr, this->serviceContextPtr->sslContextPtr, aHostFix);
    } catch (const beast::error_code& ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, ec, "create stream", {subscription.getCorrelationId()});
      return;
    }
    std::shared_ptr<FixConnection<T>> fixConnectionPtr(new FixConnection<T>(aHostFix, aPortFix, subscription, streamPtr));
    fixConnectionPtr->status = FixConnection<T>::Status::CONNECTING;
    CCAPI_LOGGER_TRACE("before async_connect");
    T& stream = *streamPtr;
    beast::get_lowest_layer(stream).async_connect(field == CCAPI_FIX_MARKET_DATA            ? this->tcpResolverResultsFixMarketData
                                                  : field == CCAPI_FIX_EXECUTION_MANAGEMENT ? this->tcpResolverResultsFixExecutionManagement
                                                                                            : this->tcpResolverResultsFix,
                                                  beast::bind_front_handler(&FixService::onConnect_3, shared_from_base<FixService>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_connect");
  }
  void onConnect_3(std::shared_ptr<FixConnection<T>> fixConnectionPtr, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    // CCAPI_LOGGER_TRACE("async_connect callback start");
    // auto now = UtilTime::now();
    // if (ec) {
    //   CCAPI_LOGGER_TRACE("fail");
    //   this->onFail(fixConnectionPtr, ec, "connect");
    //   return;
    // }
    // CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
    // CCAPI_LOGGER_TRACE("connected");
    // T& stream = *fixConnectionPtr->streamPtr;
    // if (this->useSsl) {
    //   CCAPI_LOGGER_TRACE("before async_handshake");
    //   stream.async_handshake(ssl::stream_base::client, beast::bind_front_handler(&FixService::onHandshake_3, shared_from_base<FixService>(),
    //   fixConnectionPtr)); CCAPI_LOGGER_TRACE("after async_handshake");
    // } else {
    //   this->start(fixConnectionPtr);
    // }
  }
  void onHandshake_3(std::shared_ptr<FixConnection<T>> fixConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    auto now = UtilTime::now();
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(fixConnectionPtr, ec, "handshake");
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    this->start(fixConnectionPtr, now);
  }
  void start(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const TimePoint& now) {
    auto& connectionId = fixConnectionPtr->id;
    fixConnectionPtr->status = FixConnection<T>::Status::OPEN;
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
      this->eventHandler(event, nullptr);
    }
    this->fixConnectionPtrByIdMap.insert({connectionId, fixConnectionPtr});
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
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
  void startRead_3(std::shared_ptr<FixConnection<T>> fixConnectionPtr, void* data, size_t requestedNumBytesToRead) {
    T& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_read");
    CCAPI_LOGGER_TRACE("requestedNumBytesToRead = " + toString(requestedNumBytesToRead));
    stream.async_read_some(boost::asio::buffer(data, requestedNumBytesToRead),
                           beast::bind_front_handler(&FixService::onRead_3, shared_from_base<FixService>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead_3(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("n = " + toString(n));
    auto now = UtilTime::now();
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      Event event;
      event.setType(Event::Type::SESSION_STATUS);
      Message message;
      message.setTimeReceived(now);
      message.setType(Message::Type::SESSION_CONNECTION_DOWN);
      message.setCorrelationIdList({fixConnectionPtr->subscription.getCorrelationId()});
      Element element(true);
      auto& connectionId = fixConnectionPtr->id;
      element.insert(CCAPI_CONNECTION_ID, connectionId);
      message.setElementList({element});
      event.setMessageList({message});
      this->eventHandler(event, nullptr);
      this->onFail(fixConnectionPtr, ec, "read");
      return;
    }
    if (fixConnectionPtr->status != FixConnection<T>::Status::OPEN) {
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
          CCAPI_LOGGER_DEBUG("received " + printableString(reader.message_begin(), reader.message_end() - reader.message_begin()));
          auto it = reader.message_type();
          auto messageType = it->value().as_string();
          CCAPI_LOGGER_DEBUG("received a " + messageType + " message");
          element.insert(it->tag(), messageType);
          if (messageType == "0") {
            shouldEmitEvent = false;
            CCAPI_LOGGER_DEBUG("Heartbeat: " + toString(*fixConnectionPtr));
            // #ifdef CCAPI_FIX_SERVICE_SHOULD_RESPOND_HEARTBEAT_WITH_HEARTBEAT
            //             this->writeMessage(fixConnectionPtr, nowFixTimeStr,
            //                                {{
            //                                    {hff::tag::MsgType, "0"},
            //                                }});
            // #endif
          } else if (messageType == "1") {
            shouldEmitEvent = false;
            CCAPI_LOGGER_DEBUG("Test Request: " + toString(*fixConnectionPtr));
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
                    [that = shared_from_base<FixService>()](std::shared_ptr<FixConnection<T>> fixConnectionPtr) {
                      auto now = UtilTime::now();
                      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
                      that->writeMessage(
                          fixConnectionPtr, nowFixTimeStr,
                          {{
                               {hff::tag::MsgType, "0"},
                           },
                           {
                               {hff::tag::MsgType, "1"},
                               {hff::tag::TestReqID, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())},
                           }});
                    },
                    true);
              } else {
                event.setType(Event::Type::AUTHORIZATION_STATUS);
                message.setType(Message::Type::AUTHORIZATION_FAILURE);
              }
            } else {
              event.setType(Event::Type::FIX);
              message.setType(Message::Type::FIX);
              if (messageType == "5") {
                this->writeMessage(fixConnectionPtr, nowFixTimeStr,
                                   {{
                                       {hff::tag::MsgType, "5"},
                                   }});
              }
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
        this->eventHandler(event, nullptr);
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
  void startWrite_3(std::shared_ptr<FixConnection<T>> fixConnectionPtr, void* data, size_t numBytesToWrite) {
    T& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_write");
    CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
    boost::asio::async_write(stream, boost::asio::buffer(data, numBytesToWrite),
                             beast::bind_front_handler(&FixService::onWrite_3, shared_from_base<FixService>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite_3(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
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
  void writeMessage(std::shared_ptr<FixConnection<T>> fixConnectionPtr, const std::string& nowFixTimeStr,
                    const std::vector<std::vector<std::pair<int, std::string>>>& paramList) {
    if (fixConnectionPtr->status != FixConnection<T>::Status::OPEN) {
      CCAPI_LOGGER_WARN("should write no more messages");
      return;
    }
    auto& connectionId = fixConnectionPtr->id;
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    size_t n = writeMessageBufferWrittenLength;
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
    CCAPI_LOGGER_DEBUG("about to send " + printableString(writeMessageBuffer.data(), n));
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    if (writeMessageBufferWrittenLength == 0) {
      CCAPI_LOGGER_TRACE("about to start write");
      this->startWrite_3(fixConnectionPtr, writeMessageBuffer.data(), n);
    }
    writeMessageBufferWrittenLength = n;
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
  }
  void onPongByMethod(PingPongMethod method, std::shared_ptr<FixConnection<T>> fixConnectionPtr, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->lastPongTpByMethodByConnectionIdMap[fixConnectionPtr->id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void sendRequestByFix(Request& request, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("now = " + toString(now));
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<FixService>(), request]() mutable {
      auto now = UtilTime::now();
      CCAPI_LOGGER_DEBUG("request = " + toString(request));
      CCAPI_LOGGER_TRACE("now = " + toString(now));
      request.setTimeSent(now);
      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
      auto& connectionId = request.getCorrelationId();
      auto it = that->fixConnectionPtrByIdMap.find(connectionId);
      if (it == that->fixConnectionPtrByIdMap.end()) {
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
        that->eventHandler(event, nullptr);
        return;
      }
      auto& fixConnectionPtr = it->second;
      CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
      that->writeMessage(fixConnectionPtr, nowFixTimeStr, request.getParamListFix());
    });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void setPingPongTimer(PingPongMethod method, std::shared_ptr<FixConnection<T>> fixConnectionPtr,
                        std::function<void(std::shared_ptr<FixConnection<T>>)> pingMethod, bool pingNow = false) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliseconds = this->pingIntervalMillisecondsByMethodMap[method];
    auto pongTimeoutMilliseconds = this->pongTimeoutMillisecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliseconds = " + toString(pingIntervalMilliseconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliseconds = " + toString(pongTimeoutMilliseconds));
    if (pingIntervalMilliseconds <= pongTimeoutMilliseconds) {
      return;
    }
    if (fixConnectionPtr->status == FixConnection<T>::Status::OPEN) {
      if (pingNow) {
        pingMethod(fixConnectionPtr);
      }
      if (this->pingTimerByMethodByConnectionIdMap.find(fixConnectionPtr->id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
              this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method)->cancel();
      }
      TimerPtr timerPtr(
          new boost::asio::steady_timer(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pingIntervalMilliseconds - pongTimeoutMilliseconds)));
      timerPtr->async_wait([fixConnectionPtr, that = shared_from_base<FixService>(), pingMethod, pongTimeoutMilliseconds, method](ErrorCode const& ec) {
        if (that->fixConnectionPtrByIdMap.find(fixConnectionPtr->id) != that->fixConnectionPtrByIdMap.end()) {
          if (ec) {
            CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", ping timer error: " + ec.message());
            that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
          } else {
            if (that->fixConnectionPtrByIdMap.at(fixConnectionPtr->id)->status == FixConnection<T>::Status::OPEN) {
              ErrorCode ec;
              pingMethod(fixConnectionPtr);
              if (ec) {
                that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "ping");
              }
              if (pongTimeoutMilliseconds <= 0) {
                return;
              }
              if (that->pongTimeOutTimerByMethodByConnectionIdMap.find(fixConnectionPtr->id) != that->pongTimeOutTimerByMethodByConnectionIdMap.end() &&
                  that->pongTimeOutTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
                      that->pongTimeOutTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).end()) {
                that->pongTimeOutTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method)->cancel();
              }
              TimerPtr timerPtr(new boost::asio::steady_timer(*that->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pongTimeoutMilliseconds)));
              timerPtr->async_wait([fixConnectionPtr, that, pingMethod, pongTimeoutMilliseconds, method](ErrorCode const& ec) {
                if (that->fixConnectionPtrByIdMap.find(fixConnectionPtr->id) != that->fixConnectionPtrByIdMap.end()) {
                  if (ec) {
                    CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", pong timeout timer error: " + ec.message());
                    that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                  } else {
                    if (that->fixConnectionPtrByIdMap.at(fixConnectionPtr->id)->status == FixConnection<T>::Status::OPEN) {
                      auto now = UtilTime::now();
                      if (that->lastPongTpByMethodByConnectionIdMap.find(fixConnectionPtr->id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                          that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
                              that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).end() &&
                          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                                that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method))
                                  .count() >= pongTimeoutMilliseconds) {
                        auto thisFixConnectionPtr = fixConnectionPtr;
                        that->onFail(fixConnectionPtr, "pong timeout");
                      } else {
                        auto thisFixConnectionPtr = fixConnectionPtr;
                        that->setPingPongTimer(method, thisFixConnectionPtr, pingMethod);
                      }
                    }
                  }
                }
              });
              that->pongTimeOutTimerByMethodByConnectionIdMap[fixConnectionPtr->id][method] = timerPtr;
            }
          }
        }
      });
      this->pingTimerByMethodByConnectionIdMap[fixConnectionPtr->id][method] = timerPtr;
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual std::vector<std::pair<int, std::string>> createCommonParam(const std::string& connectionId, const std::string& nowFixTimeStr) { return {}; }
  virtual std::vector<std::pair<int, std::string>> createLogonParam(const std::string& connectionId, const std::string& nowFixTimeStr,
                                                                    const std::map<int, std::string> logonOptionMap = {}) {
    return {};
  }
  const size_t readMessageChunkSize = CCAPI_HFFIX_READ_MESSAGE_CHUNK_SIZE;
  std::map<std::string, std::array<char, CCAPI_FIX_READ_BUFFER_SIZE>> readMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> readMessageBufferReadLengthByConnectionIdMap;
  std::map<std::string, std::array<char, CCAPI_FIX_WRITE_BUFFER_SIZE>> writeMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> writeMessageBufferWrittenLengthByConnectionIdMap;
  std::map<std::string, std::shared_ptr<FixConnection<T>>> fixConnectionPtrByIdMap;
  std::map<std::string, int> sequenceSentByConnectionIdMap;
  std::map<std::string, std::map<std::string, std::string>> credentialByConnectionIdMap;
  std::string apiKeyName;
  std::string apiSecretName;
  std::string baseUrlFix;
  std::string hostFix;
  std::string portFix;
  tcp::resolver::results_type tcpResolverResultsFix;
  std::string baseUrlFixMarketData;
  std::string hostFixMarketData;
  std::string portFixMarketData;
  tcp::resolver::results_type tcpResolverResultsFixMarketData;
  std::string baseUrlFixExecutionManagement;
  std::string hostFixExecutionManagement;
  std::string portFixExecutionManagement;
  tcp::resolver::results_type tcpResolverResultsFixExecutionManagement;
  std::string protocolVersion;
  std::string senderCompID;
  std::string targetCompID;
};
template <>
inline std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> FixService<beast::ssl_stream<beast::tcp_stream>>::createStreamFix(net::io_context* iocPtr,
                                                                                                                               net::ssl::context* ctxPtr,
                                                                                                                               const std::string& host) {
  std::shared_ptr<beast::ssl_stream<beast::tcp_stream>> streamPtr(new beast::ssl_stream<beast::tcp_stream>(*iocPtr, *ctxPtr));
  // Set SNI Hostname (many hosts need this to handshake successfully)
  if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), host.c_str())) {
    beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
    CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
    throw ec;
  }
  return streamPtr;
}
template <>
inline std::shared_ptr<beast::tcp_stream> FixService<beast::tcp_stream>::createStreamFix(net::io_context* iocPtr, net::ssl::context* ctxPtr,
                                                                                         const std::string& host) {
  std::shared_ptr<beast::tcp_stream> streamPtr(new beast::tcp_stream(*iocPtr));
  return streamPtr;
}
template <>
inline void FixService<beast::ssl_stream<beast::tcp_stream>>::onConnect_3(std::shared_ptr<FixConnection<beast::ssl_stream<beast::tcp_stream>>> fixConnectionPtr,
                                                                          beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
  CCAPI_LOGGER_TRACE("async_connect callback start");
  auto now = UtilTime::now();
  if (ec) {
    CCAPI_LOGGER_TRACE("fail");
    this->onFail(fixConnectionPtr, ec, "connect");
    return;
  }
  CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
  CCAPI_LOGGER_TRACE("connected");
  beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
  CCAPI_LOGGER_TRACE("before async_handshake");
  stream.async_handshake(ssl::stream_base::client, beast::bind_front_handler(&FixService::onHandshake_3, shared_from_base<FixService>(), fixConnectionPtr));
  CCAPI_LOGGER_TRACE("after async_handshake");
}
template <>
inline void FixService<beast::tcp_stream>::onConnect_3(std::shared_ptr<FixConnection<beast::tcp_stream>> fixConnectionPtr, beast::error_code ec,
                                                       tcp::resolver::results_type::endpoint_type) {
  CCAPI_LOGGER_TRACE("async_connect callback start");
  auto now = UtilTime::now();
  if (ec) {
    CCAPI_LOGGER_TRACE("fail");
    this->onFail(fixConnectionPtr, ec, "connect");
    return;
  }
  CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
  CCAPI_LOGGER_TRACE("connected");
  beast::tcp_stream& stream = *fixConnectionPtr->streamPtr;
  this->start(fixConnectionPtr, now);
}
} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_H_
