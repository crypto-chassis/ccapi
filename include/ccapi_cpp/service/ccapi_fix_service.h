#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifndef CCAPI_FIX_READ_BUFFER_SIZE
#define CCAPI_FIX_READ_BUFFER_SIZE (1 << 20)
#endif
#ifndef CCAPI_FIX_WRITE_BUFFER_SIZE
#define CCAPI_FIX_WRITE_BUFFER_SIZE (1 << 20)
#endif
#include "ccapi_cpp/service/ccapi_service.h"
#include "hffix.hpp"
namespace hff = hffix;

namespace ccapi {

/**
 * Defines a service which provides access to exchange API and normalizes them. This is a base class that implements generic functionalities for dealing with
 * exchange FIX APIs. The Session object is responsible for routing requests and subscriptions to the desired concrete service.
 */

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
    std::replace(output.begin(), output.end(), '\x01', '|');
    return output;
  }

  static std::string printableString(const std::string& s) {
    std::string output(s);
    std::replace(output.begin(), output.end(), '\x01', '|');
    return output;
  }

  void setHostFixFromUrlFix(std::string& aHostFix, std::string& aPortFix, const std::string& baseUrlFix) {
    auto hostPort = this->extractHostFromUrl(baseUrlFix);
    aHostFix = hostPort.first;
    aPortFix = hostPort.second;
  }

  // each subscription creates a unique FIX connection
  void subscribe(std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    if (this->shouldContinue.load()) {
      for (auto& subscription : subscriptionList) {
        boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<FixService>(), subscription]() mutable {
          auto now = UtilTime::now();
          subscription.setTimeSent(now);
          auto credential = subscription.getCredential();
          if (credential.empty()) {
            credential = that->credentialDefault;
          }

          const auto& fieldSet = subscription.getFieldSet();
          if (fieldSet.find(CCAPI_FIX_MARKET_DATA) != fieldSet.end()) {
            auto fixConnectionPtr = std::make_shared<FixConnection>(that->baseUrlFixMarketData, subscription, credential);
            that->setFixConnectionStream(fixConnectionPtr);
            CCAPI_LOGGER_WARN("about to subscribe with new fixConnectionPtr " + toString(*fixConnectionPtr));
            that->connect(fixConnectionPtr);
          } else {
            auto fixConnectionPtr = std::make_shared<FixConnection>(that->baseUrlFix, subscription, credential);
            that->setFixConnectionStream(fixConnectionPtr);
            CCAPI_LOGGER_WARN("about to subscribe with new fixConnectionPtr " + toString(*fixConnectionPtr));
            that->connect(fixConnectionPtr);
          }
        });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  virtual void onFail(std::shared_ptr<FixConnection> fixConnectionPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->clearStates(fixConnectionPtr);
    this->onFail_(fixConnectionPtr);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void clearStates(std::shared_ptr<FixConnection> fixConnectionPtr) {
    CCAPI_LOGGER_INFO("clear states for fixConnectionPtr " + toString(*fixConnectionPtr));
    auto& connectionId = fixConnectionPtr->id;
    this->readMessageBufferByConnectionIdMap.erase(connectionId);
    this->readMessageBufferReadLengthByConnectionIdMap.erase(connectionId);
    this->writeMessageBufferByConnectionIdMap.erase(connectionId);
    this->writeMessageBufferWrittenLengthByConnectionIdMap.erase(connectionId);
    this->fixConnectionPtrByIdMap.erase(connectionId);
    auto urlBase = fixConnectionPtr->url;
    this->fixMsgSeqNumByConnectionIdMap.erase(fixConnectionPtr->id);
  }

  void onFail_(std::shared_ptr<FixConnection> fixConnectionPtr) {
    fixConnectionPtr->status = FixConnection::Status::FAILED;
    this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE_DUE_TO_CONNECTION_FAILURE,
                  "connection " + toString(*fixConnectionPtr) + " has failed before opening", {fixConnectionPtr->subscription.getCorrelationId()});
    auto urlBase = fixConnectionPtr->url;
    CCAPI_LOGGER_TRACE("urlBase = " + urlBase);
    CCAPI_LOGGER_TRACE("this->connectNumRetryOnFailByConnectionUrlMap = " + toString(this->connectNumRetryOnFailByConnectionUrlMap));
    long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[urlBase], 6)));
    CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
    if (this->connectRetryOnFailTimerByConnectionIdMap.find(fixConnectionPtr->id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
      this->connectRetryOnFailTimerByConnectionIdMap.at(fixConnectionPtr->id)->cancel();
    }
    auto timerPtr = std::make_shared<boost::asio::steady_timer>(*this->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(seconds * 1000));
    timerPtr->async_wait([fixConnectionPtr, that = shared_from_base<FixService>(), urlBase](ErrorCode const& ec) {
      if (that->fixConnectionPtrByIdMap.find(fixConnectionPtr->id) == that->fixConnectionPtrByIdMap.end()) {
        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
            CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", connect retry on fail timer error: " + ec.message());
            that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
          }
        } else {
          CCAPI_LOGGER_INFO("about to retry");
          that->connect(fixConnectionPtr);
          that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
        }
      }
    });
    this->connectRetryOnFailTimerByConnectionIdMap[fixConnectionPtr->id] = timerPtr;
  }

  void setFixConnectionStream(std::shared_ptr<FixConnection> fixConnectionPtr) {
    if (fixConnectionPtr->isSecure) {
      fixConnectionPtr->streamPtr =
          std::make_shared<beast::ssl_stream<beast::tcp_stream>>(*this->serviceContextPtr->ioContextPtr, *this->serviceContextPtr->sslContextPtr);
    } else {
      fixConnectionPtr->streamPtr = std::make_shared<beast::tcp_stream>(*this->serviceContextPtr->ioContextPtr);
    }
  }

  virtual void connect(std::shared_ptr<FixConnection> fixConnectionPtr) {
    fixConnectionPtr->status = FixConnection::Status::CONNECTING;
    CCAPI_LOGGER_DEBUG("fixConnectionPtr = " + fixConnectionPtr->toString());
    this->startResolveFix(fixConnectionPtr);
  }

  void startResolveFix(std::shared_ptr<FixConnection> fixConnectionPtr) {
    auto newResolverPtr = std::make_shared<tcp::resolver>(*this->serviceContextPtr->ioContextPtr);
    newResolverPtr->async_resolve(fixConnectionPtr->host, fixConnectionPtr->port,
                                  beast::bind_front_handler(&FixService::onResolveFix, shared_from_base<FixService>(), fixConnectionPtr, newResolverPtr));
  }

  void onResolveFix(std::shared_ptr<FixConnection> fixConnectionPtr, std::shared_ptr<tcp::resolver> newResolverPtr, beast::error_code ec,
                    tcp::resolver::results_type tcpNewResolverResultsFix) {
    if (ec) {
      CCAPI_LOGGER_TRACE("dns resolve fail");
      this->onFail(fixConnectionPtr);
      return;
    }
    this->startConnectFix(fixConnectionPtr, this->sessionOptions.fixConnectTimeoutMilliseconds, tcpNewResolverResultsFix);
  }

  void startConnectFix(std::shared_ptr<FixConnection> fixConnectionPtr, long timeoutMilliseconds, tcp::resolver::results_type tcpResolverResults) {
    std::visit(
        [&](auto& streamPtr) {
          using StreamType = std::decay_t<decltype(*streamPtr)>;

          if (timeoutMilliseconds > 0) {
            beast::get_lowest_layer(*streamPtr).expires_after(std::chrono::milliseconds(timeoutMilliseconds));
          }

          if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
            if (!SSL_set_tlsext_host_name(streamPtr->native_handle(), fixConnectionPtr->host.c_str())) {
              beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
              CCAPI_LOGGER_DEBUG("error SSL_set_tlsext_host_name: " + ec.message());
              this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, ec, "set SNI Hostname", {fixConnectionPtr->subscription.getCorrelationId()});
              return;
            }
          }

          CCAPI_LOGGER_TRACE("before async_connect");

          beast::get_lowest_layer(*streamPtr)
              .async_connect(tcpResolverResults, beast::bind_front_handler(&FixService::onConnectFix, shared_from_base<FixService>(), fixConnectionPtr));

          CCAPI_LOGGER_TRACE("after async_connect");
        },
        fixConnectionPtr->streamPtr);
  }

  void onConnectFix(std::shared_ptr<FixConnection> fixConnectionPtr, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    auto now = UtilTime::now();
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(fixConnectionPtr);
      return;
    }
    CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
    CCAPI_LOGGER_TRACE("connected");

    std::visit(
        [&](auto& streamPtr) {
          using StreamType = std::decay_t<decltype(*streamPtr)>;
          if constexpr (std::is_same_v<StreamType, beast::ssl_stream<beast::tcp_stream>>) {
            CCAPI_LOGGER_TRACE("before ssl async_handshake");

            streamPtr->async_handshake(ssl::stream_base::client,
                                       beast::bind_front_handler(&FixService::onSslHandshakeFix, shared_from_base<FixService>(), fixConnectionPtr));

            CCAPI_LOGGER_TRACE("after ssl async_handshake");
          } else {
            this->onSslHandshakeFix(fixConnectionPtr, {});
          }
        },
        fixConnectionPtr->streamPtr);
  }

  void onSslHandshakeFix(std::shared_ptr<FixConnection> fixConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onFail(fixConnectionPtr);
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");

    std::visit(
        [&](auto& streamPtr) {
          auto& stream = *streamPtr;
          beast::get_lowest_layer(stream).expires_never();
        },
        fixConnectionPtr->streamPtr);

    this->onOpen(fixConnectionPtr);
  }

  void onOpen(std::shared_ptr<FixConnection> fixConnectionPtr) {
    auto now = UtilTime::now();
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
      element.insert(CCAPI_CONNECTION_URL, fixConnectionPtr->url);
      message.setElementList({element});
      event.setMessageList({message});
      this->eventHandler(event, nullptr);
    }
    this->fixConnectionPtrByCorrelationIdMap.insert({fixConnectionPtr->subscription.getCorrelationId(), fixConnectionPtr});
    this->fixConnectionPtrByIdMap.insert({connectionId, fixConnectionPtr});
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
    auto credential = fixConnectionPtr->subscription.getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    auto& readMessageBuffer = this->readMessageBufferByConnectionIdMap[connectionId];
    auto& readMessageBufferReadLength = this->readMessageBufferReadLengthByConnectionIdMap[connectionId];
    CCAPI_LOGGER_TRACE("about to start read");
    CCAPI_LOGGER_TRACE("readMessageBufferReadLength = " + toString(readMessageBufferReadLength));
    this->startReadFix(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                       std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize));
    std::map<int, std::string> logonOptionMap;
    for (const auto& x : fixConnectionPtr->subscription.getOptionMap()) {
      if (UtilString::isNumber(x.first)) {
        logonOptionMap.insert({std::stoi(x.first), x.second});
      }
    }
    auto param = this->createLogonParam(fixConnectionPtr, nowFixTimeStr, logonOptionMap);
    this->writeMessage(fixConnectionPtr, nowFixTimeStr, {param});
  }

  void startReadFix(std::shared_ptr<FixConnection> fixConnectionPtr, void* data, size_t requestedNumBytesToRead) {
    std::visit(
        [&](auto& streamPtr) {
          CCAPI_LOGGER_TRACE("before async_read");
          CCAPI_LOGGER_TRACE("requestedNumBytesToRead = " + toString(requestedNumBytesToRead));
          streamPtr->async_read_some(boost::asio::buffer(data, requestedNumBytesToRead),
                                     beast::bind_front_handler(&FixService::onReadFix, shared_from_base<FixService>(), fixConnectionPtr));
          CCAPI_LOGGER_TRACE("after async_read");
        },
        fixConnectionPtr->streamPtr);
  }

  void onReadFix(std::shared_ptr<FixConnection> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("n = " + toString(n));
    auto now = UtilTime::now();
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      // this->close(fixConnectionPtr);
      this->onClose(fixConnectionPtr, ec);
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
      message.setCorrelationIdList({fixConnectionPtr->subscription.getCorrelationId()});
      if (reader.is_valid()) {
        try {
          CCAPI_LOGGER_DEBUG("received " + printableString(reader.message_begin(), reader.message_end() - reader.message_begin()));
          auto it = reader.message_type();
          auto messageType = it->value().as_string_view();
          CCAPI_LOGGER_DEBUG("received a " + std::string(messageType) + " message");
          element.insert(it->tag(), messageType);
          if (messageType == "0") {
            shouldEmitEvent = false;
            CCAPI_LOGGER_DEBUG("Heartbeat: " + toString(*fixConnectionPtr));
          } else if (messageType == "1") {
            shouldEmitEvent = false;
            CCAPI_LOGGER_DEBUG("Test Request: " + toString(*fixConnectionPtr));
            if (reader.find_with_hint(hff::tag::TestReqID, it)) {
              this->writeMessage(fixConnectionPtr, nowFixTimeStr,
                                 {{
                                     {hff::tag::MsgType, "0"},
                                     {hff::tag::TestReqID, std::string(it->value().as_string_view())},
                                 }});
            }
          } else {
            it = it + 5;
            while (it->tag() != hffix::tag::CheckSum) {
              element.insert(it->tag(), it->value().as_string_view());
              ++it;
            }
            if (reader.find_with_hint(hff::tag::MsgSeqNum, it) && it->value().as_string_view() == "1") {
              if (messageType == "A") {
                event.setType(Event::Type::AUTHORIZATION_STATUS);
                message.setType(Message::Type::AUTHORIZATION_SUCCESS);
                if (this->pingTimerByMethodByConnectionIdMap.find(fixConnectionPtr->id) != this->pingTimerByMethodByConnectionIdMap.end()) {
                  for (const auto& x : this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id)) {
                    x.second->cancel();
                  }
                  this->pingTimerByMethodByConnectionIdMap.erase(fixConnectionPtr->id);
                }

                this->setPingPongTimer(
                    PingPongMethod::FIX_PROTOCOL_LEVEL, fixConnectionPtr,
                    [that = shared_from_base<FixService>()](std::shared_ptr<FixConnection> fixConnectionPtr) {
                      CCAPI_LOGGER_TRACE("pingMethod is triggered");
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
                if (fixConnectionPtr->status == FixConnection::Status::OPEN) {
                  this->writeMessage(fixConnectionPtr, nowFixTimeStr,
                                     {{
                                         {hff::tag::MsgType, "5"},
                                     }});
                } else {
                  this->onClose(fixConnectionPtr, {});
                }
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
      this->startReadFix(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                         std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize));
    } else {
      CCAPI_LOGGER_TRACE("about to start read");
      this->startReadFix(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                         std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize - readMessageBufferReadLength));
    }
    this->onPongByMethod(PingPongMethod::FIX_PROTOCOL_LEVEL, fixConnectionPtr, now);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void startWriteFix(std::shared_ptr<FixConnection> fixConnectionPtr, void* data, size_t numBytesToWrite) {
    std::visit(
        [&](auto& streamPtr) {
          CCAPI_LOGGER_TRACE("before async_write");
          CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
          boost::asio::async_write(*streamPtr, boost::asio::buffer(data, numBytesToWrite),
                                   beast::bind_front_handler(&FixService::onWriteFix, shared_from_base<FixService>(), fixConnectionPtr));
          CCAPI_LOGGER_TRACE("after async_write");
        },
        fixConnectionPtr->streamPtr);
  }

  void onWriteFix(std::shared_ptr<FixConnection> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    auto now = UtilTime::now();
    auto& connectionId = fixConnectionPtr->id;
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      ErrorCode ec;
      //   this->close(fixConnectionPtr);
      this->onClose(fixConnectionPtr, ec);
      return;
    }
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    writeMessageBufferWrittenLength -= n;
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    if (writeMessageBufferWrittenLength > 0) {
      std::memmove(writeMessageBuffer.data(), writeMessageBuffer.data() + n, writeMessageBufferWrittenLength);
      CCAPI_LOGGER_TRACE("about to start write");
      this->startWriteFix(fixConnectionPtr, writeMessageBuffer.data(), writeMessageBufferWrittenLength);
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
    size_t n = writeMessageBufferWrittenLength;
    for (const auto& param : paramList) {
      auto commonParam = this->createCommonParam(fixConnectionPtr, nowFixTimeStr);
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
      this->startWriteFix(fixConnectionPtr, writeMessageBuffer.data(), n);
    }
    writeMessageBufferWrittenLength = n;
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
  }

  void onPongByMethod(PingPongMethod method, std::shared_ptr<FixConnection> fixConnectionPtr, const TimePoint& timeReceived) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->lastPongTpByMethodByConnectionIdMap[fixConnectionPtr->id][method] = timeReceived;
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void sendRequestByFix(const std::string& fixOrderEntrySubscriptionCorrelationId, Request& request, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("now = " + toString(now));
    boost::asio::post(*this->serviceContextPtr->ioContextPtr, [that = shared_from_base<FixService>(), fixOrderEntrySubscriptionCorrelationId,
                                                               request]() mutable {
      auto now = UtilTime::now();
      CCAPI_LOGGER_DEBUG("request = " + toString(request));
      CCAPI_LOGGER_TRACE("now = " + toString(now));
      request.setTimeSent(now);
      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
      auto it = that->fixConnectionPtrByCorrelationIdMap.find(fixOrderEntrySubscriptionCorrelationId);
      if (it == that->fixConnectionPtrByCorrelationIdMap.end()) {
        that->onError(Event::Type::REQUEST_STATUS, Message::Type::REQUEST_FAILURE, "FIX connection was not found", {fixOrderEntrySubscriptionCorrelationId});
        return;
      }
      auto& fixConnectionPtr = it->second;
      CCAPI_LOGGER_TRACE("fixConnectionPtr = " + toString(*fixConnectionPtr));
      that->writeMessage(fixConnectionPtr, nowFixTimeStr, request.getParamListFix());
    });
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  void setPingPongTimer(PingPongMethod method, std::shared_ptr<FixConnection> fixConnectionPtr, std::function<void(std::shared_ptr<FixConnection>)> pingMethod,
                        bool pingNow = false) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_TRACE("method = " + pingPongMethodToString(method));
    auto pingIntervalMilliseconds = this->pingIntervalMillisecondsByMethodMap[method];
    auto pongTimeoutMilliseconds = this->pongTimeoutMillisecondsByMethodMap[method];
    CCAPI_LOGGER_TRACE("pingIntervalMilliseconds = " + toString(pingIntervalMilliseconds));
    CCAPI_LOGGER_TRACE("pongTimeoutMilliseconds = " + toString(pongTimeoutMilliseconds));
    if (pingIntervalMilliseconds <= pongTimeoutMilliseconds) {
      return;
    }
    CCAPI_LOGGER_DEBUG("fixConnectionPtr->status = " + FixConnection::statusToString(fixConnectionPtr->status));
    if (fixConnectionPtr->status == FixConnection::Status::OPEN) {
      if (pingNow) {
        pingMethod(fixConnectionPtr);
      }
      if (this->pingTimerByMethodByConnectionIdMap.find(fixConnectionPtr->id) != this->pingTimerByMethodByConnectionIdMap.end() &&
          this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
              this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).end()) {
        this->pingTimerByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method)->cancel();
      }
      auto timerPtr = std::make_shared<boost::asio::steady_timer>(*this->serviceContextPtr->ioContextPtr,
                                                                  std::chrono::milliseconds(pingIntervalMilliseconds - pongTimeoutMilliseconds));
      timerPtr->async_wait([fixConnectionPtr, that = shared_from_base<FixService>(), pingMethod, pongTimeoutMilliseconds, method](ErrorCode const& ec) {
        if (that->fixConnectionPtrByIdMap.find(fixConnectionPtr->id) != that->fixConnectionPtrByIdMap.end()) {
          if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
              CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", ping timer error: " + ec.message());
              that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
            }
          } else {
            if (that->fixConnectionPtrByIdMap.at(fixConnectionPtr->id)->status == FixConnection::Status::OPEN) {
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
              auto timerPtr =
                  std::make_shared<boost::asio::steady_timer>(*that->serviceContextPtr->ioContextPtr, std::chrono::milliseconds(pongTimeoutMilliseconds));
              timerPtr->async_wait([fixConnectionPtr, that, pingMethod, pongTimeoutMilliseconds, method](ErrorCode const& ec) {
                if (that->fixConnectionPtrByIdMap.find(fixConnectionPtr->id) != that->fixConnectionPtrByIdMap.end()) {
                  if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                      CCAPI_LOGGER_ERROR("fixConnectionPtr = " + toString(*fixConnectionPtr) + ", pong timeout timer error: " + ec.message());
                      that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
                    }
                  } else {
                    if (that->fixConnectionPtrByIdMap.at(fixConnectionPtr->id)->status == FixConnection::Status::OPEN) {
                      auto now = UtilTime::now();

                      if (that->lastPongTpByMethodByConnectionIdMap.find(fixConnectionPtr->id) != that->lastPongTpByMethodByConnectionIdMap.end() &&
                          that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).find(method) !=
                              that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).end() &&
                          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                                that->lastPongTpByMethodByConnectionIdMap.at(fixConnectionPtr->id).at(method))
                                  .count() >= pongTimeoutMilliseconds) {
                        that->close(fixConnectionPtr);
                      } else {
                        that->setPingPongTimer(method, fixConnectionPtr, pingMethod);
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

  virtual std::vector<std::pair<int, std::string>> createCommonParam(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& nowFixTimeStr) {
    return {};
  }

  virtual std::vector<std::pair<int, std::string>> createLogonParam(std::shared_ptr<FixConnection> fixConnectionPtr, const std::string& nowFixTimeStr,
                                                                    const std::map<int, std::string> logonOptionMap = {}) {
    return {};
  }

  void close(std::shared_ptr<FixConnection> fixConnectionPtr) {
    CCAPI_LOGGER_DEBUG("fixConnectionPtr->status = " + FixConnection::statusToString(fixConnectionPtr->status));
    if (fixConnectionPtr->status == FixConnection::Status::CLOSING || fixConnectionPtr->status == FixConnection::Status::CLOSED) {
      return;
    }

    std::visit(
        [&](auto& streamPtr) {
          const auto& now = UtilTime::now();
          const auto& nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
          this->writeMessage(fixConnectionPtr, nowFixTimeStr,
                             {{
                                 {hff::tag::MsgType, "5"},
                             }});
        },
        fixConnectionPtr->streamPtr);

    fixConnectionPtr->status = FixConnection::Status::CLOSING;
  }

  virtual void onClose(std::shared_ptr<FixConnection> fixConnectionPtr, ErrorCode ec) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("fixConnectionPtr->status = " + FixConnection::statusToString(fixConnectionPtr->status));
    if (fixConnectionPtr->status == FixConnection::Status::CLOSING || fixConnectionPtr->status == FixConnection::Status::CLOSED) {
      return;
    }

    auto now = UtilTime::now();
    fixConnectionPtr->status = FixConnection::Status::CLOSED;
    CCAPI_LOGGER_INFO("connection " + toString(*fixConnectionPtr) + " is closed");
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_DOWN);
    Element element;
    element.insert(CCAPI_CONNECTION_ID, fixConnectionPtr->id);
    element.insert(CCAPI_CONNECTION_URL, fixConnectionPtr->url);
    message.setElementList({element});
    std::vector<std::string> correlationIdList{fixConnectionPtr->subscription.getCorrelationId()};
    CCAPI_LOGGER_DEBUG("correlationIdList = " + toString(correlationIdList));
    message.setCorrelationIdList(correlationIdList);
    event.setMessageList({message});
    this->eventHandler(event, nullptr);
    CCAPI_LOGGER_INFO("connection " + toString(*fixConnectionPtr) + " is closed");
    this->clearStates(fixConnectionPtr);
    this->setFixConnectionStream(fixConnectionPtr);
    this->fixConnectionPtrByCorrelationIdMap.erase(fixConnectionPtr->subscription.getCorrelationId());
    this->fixConnectionPtrByIdMap.erase(fixConnectionPtr->id);
    if (this->shouldContinue.load()) {
      this->connect(fixConnectionPtr);
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }

  const size_t readMessageChunkSize = CCAPI_HFFIX_READ_MESSAGE_CHUNK_SIZE;
  std::map<std::string, std::array<char, CCAPI_FIX_READ_BUFFER_SIZE>> readMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> readMessageBufferReadLengthByConnectionIdMap;
  std::map<std::string, std::array<char, CCAPI_FIX_WRITE_BUFFER_SIZE>> writeMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> writeMessageBufferWrittenLengthByConnectionIdMap;
  std::map<std::string, std::shared_ptr<FixConnection>> fixConnectionPtrByIdMap;
  std::string fixApiKeyName;
  std::string fixApiPrivateKeyPathName;
  std::string fixApiPrivateKeyPasswordName;
  std::string baseUrlFix;
  std::string hostFix;
  std::string portFix;
  std::string baseUrlFixMarketData;
  std::string hostFixMarketData;
  std::string portFixMarketData;
  std::string protocolVersion;
  std::string senderCompId;
  std::string targetCompId;

  std::map<std::string, unsigned int> fixMsgSeqNumByConnectionIdMap;
  std::map<std::string, std::shared_ptr<FixConnection>> fixConnectionPtrByCorrelationIdMap;
};

} /* namespace ccapi */
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_H_
