#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_COINBASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_COINBASE_H_
#ifdef CCAPI_ENABLE_SERVICE_FIX
#ifdef CCAPI_ENABLE_EXCHANGE_COINBASE
// #include "ccapi_cpp/service/ccapi_service.h"
// #include <cstdlib>
// #include <functional>
// #include <iostream>
// #include <memory>
// #include <string>
// #include "boost/shared_ptr.hpp"
// #include "ccapi_cpp/ccapi_event.h"
// #include "ccapi_cpp/ccapi_hmac.h"
// #include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/service/ccapi_service.h"
// #define HFFIX_NO_BOOST_DATETIME
#include "ccapi_cpp/ccapi_hmac.h"
#include "hffix.hpp"
namespace hff = hffix;
namespace ccapi {
class FixServiceCoinbase : public Service {
 public:
  FixServiceCoinbase(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                     ServiceContextPtr serviceContextPtr)
      : Service(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    this->exchangeName = CCAPI_EXCHANGE_NAME_COINBASE;
    // this->baseUrl = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName);
    CCAPI_LOGGER_INFO("this->sessionConfigs.getUrlFixBase()=" + toString(this->sessionConfigs.getUrlFixBase()));
    this->baseUrlFix = this->sessionConfigs.getUrlFixBase().at(this->exchangeName);
    this->setHostFixFromUrlFix(this->baseUrlFix);
    this->apiKeyName = CCAPI_COINBASE_API_KEY;
    this->apiSecretName = CCAPI_COINBASE_API_SECRET;
    this->apiPassphraseName = CCAPI_COINBASE_API_PASSPHRASE;
    this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
    try {
      this->tcpResolverResults = this->resolver.resolve(this->hostFix, this->portFix);
    } catch (const std::exception& e) {
      CCAPI_LOGGER_FATAL(std::string("e.what() = ") + e.what());
    }
    // hff::dictionary_init_field(this->field_dictionary);
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  virtual ~FixServiceCoinbase() {}

 protected:
  void setHostFixFromUrlFix(std::string baseUrlFix) {
    auto hostPort = this->extractHostFromUrl(baseUrlFix);
    this->hostFix = hostPort.first;
    this->portFix = hostPort.second;
  }
  void subscribe(const std::vector<Subscription>& subscriptionList) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrlFix = " + this->baseUrlFix);
    if (this->shouldContinue.load()) {
      for (const auto& subscription : subscriptionList) {
        wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [that = shared_from_base<FixServiceCoinbase>(), subscription]() {
          // FixConnection fixConnection(that->baseUrlFix, subscription);
          auto thatSubscription = subscription;
          that->connect(thatSubscription);
        });
      }
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void subscribe(const Subscription& subscription) {
    CCAPI_LOGGER_FUNCTION_ENTER;
    CCAPI_LOGGER_DEBUG("this->baseUrlFix = " + this->baseUrlFix);
    if (this->shouldContinue.load()) {
      wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [that = shared_from_base<FixServiceCoinbase>(), subscription]() {
        // FixConnection fixConnection(that->baseUrlFix, subscription);
        auto thatSubscription = subscription;
        that->connect(thatSubscription);
      });
    }
    CCAPI_LOGGER_FUNCTION_EXIT;
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
    CCAPI_LOGGER_TRACE("before async_connect");
    beast::ssl_stream<beast::tcp_stream>& stream = *streamPtr;
    beast::get_lowest_layer(stream).async_connect(
        this->tcpResolverResults, beast::bind_front_handler(&FixServiceCoinbase::onConnect_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_connect");
  }
  void onConnect_3(std::shared_ptr<FixConnection> fixConnectionPtr, beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    CCAPI_LOGGER_TRACE("async_connect callback start");
    auto now = UtilTime::now();
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, ec, "connect", {fixConnectionPtr->subscription.getCorrelationId()});
      return;
    }
    CCAPI_LOGGER_TRACE("fixConnectionPtr " + toString(*fixConnectionPtr));
    CCAPI_LOGGER_TRACE("connected");

    beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_handshake");
    stream.async_handshake(ssl::stream_base::client,
                           beast::bind_front_handler(&FixServiceCoinbase::onHandshake_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_handshake");
  }
  void onHandshake_3(std::shared_ptr<FixConnection> fixConnectionPtr, beast::error_code ec) {
    CCAPI_LOGGER_TRACE("async_handshake callback start");
    auto now = UtilTime::now();
    auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
    if (ec) {
      CCAPI_LOGGER_TRACE("fail");
      this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, ec, "handshake", {fixConnectionPtr->subscription.getCorrelationId()});
      return;
    }
    CCAPI_LOGGER_TRACE("handshaked");
    auto connectionId = fixConnectionPtr->id;
    Event event;
    event.setType(Event::Type::SESSION_STATUS);
    Message message;
    message.setTimeReceived(now);
    message.setType(Message::Type::SESSION_CONNECTION_UP);
    message.setCorrelationIdList({fixConnectionPtr->subscription.getCorrelationId()});
    Element element;
    element.insert(CCAPI_CONNECTION_ID, connectionId);
    message.setElementList({element});
    event.setMessageList({message});
    this->eventHandler(event);
    this->fixConnectionPtrByConnectionIdMap.insert({connectionId, fixConnectionPtr});
    auto& readMessageBuffer = this->readMessageBufferByConnectionIdMap[connectionId];
    auto& readMessageBufferReadLength = this->readMessageBufferReadLengthByConnectionIdMap[connectionId];
    this->startRead_3(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                      std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize));
    auto& writeMessageBuffer = writeMessageBufferByConnectionIdMap[connectionId];
    hff::message_writer messageWriter(writeMessageBuffer.data(), writeMessageBuffer.data() + writeMessageBuffer.size());

    messageWriter.push_back_header("FIX.4.2");  // Write BeginString and BodyLength.
    auto msgType = "A";
    messageWriter.push_back_string(hff::tag::MsgType, msgType);
    auto credential = fixConnectionPtr->subscription.getCredential();
    if (credential.empty()) {
      credential = this->credentialDefault;
    }
    this->credentialByConnectionIdMap[connectionId] = credential;
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
    auto senderCompID = apiKey;
    messageWriter.push_back_string(hff::tag::SenderCompID, apiKey);
    auto targetCompID = "Coinbase";
    messageWriter.push_back_string(hff::tag::TargetCompID, targetCompID);
    auto msgSeqNum = std::to_string(++this->sequenceSentByConnectionIdMap[connectionId]);
    messageWriter.push_back_string(hff::tag::MsgSeqNum, msgSeqNum);
    messageWriter.push_back_string(hff::tag::SendingTime, nowFixTimeStr);
    for (const auto& x : fixConnectionPtr->subscription.getOptionMap()) {
      messageWriter.push_back_string(std::stoi(x.first), x.second);
    }
    messageWriter.push_back_string(hff::tag::EncryptMethod, "0");
    messageWriter.push_back_string(hff::tag::HeartBtInt, std::to_string(sessionOptions.heartbeatFixIntervalMilliSeconds / 1000));

    auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName);
    messageWriter.push_back_string(hff::tag::Password, apiPassphrase);

    // No encryption.

    // 10 second heartbeat interval.
    std::vector<std::string> prehashFieldList{nowFixTimeStr, msgType, msgSeqNum, senderCompID, targetCompID, apiPassphrase};
    auto prehashStr = UtilString::join(prehashFieldList, "\x01");
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
    auto rawData = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, UtilAlgorithm::base64Decode(apiSecret), prehashStr));
    messageWriter.push_back_string(hff::tag::RawData, rawData);

    messageWriter.push_back_trailer();  // write CheckSum.
    this->startWrite_3(fixConnectionPtr, writeMessageBuffer.data(), messageWriter.message_end() - writeMessageBuffer.data());

    // CCAPI_LOGGER_TRACE("before async_write");
    // auto numBytesToWrite = messageWriter.message_end() - writeMessageBuffer.data();
    // CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
    // boost::asio::async_write(stream, boost::asio::buffer(writeMessageBuffer.data(), numBytesToWrite),
    //                         beast::bind_front_handler(&FixServiceCoinbase::onWrite_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));
    // this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId] += numBytesToWrite;
    // CCAPI_LOGGER_TRACE("after async_write");
  }
  void startRead_3(std::shared_ptr<FixConnection> fixConnectionPtr, void* data, size_t numBytesToRead) {
    beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
    // if (wsConnection.status == WsConnection::Status::CLOSING && !this->shouldProcessRemainingMessageOnClosingByConnectionIdMap[wsConnection.id]) {
    //   CCAPI_LOGGER_WARN("should not process remaining message on closing");
    //   return;
    // }
    CCAPI_LOGGER_TRACE("before async_read");
    // auto numBytesToRead = std::min(sizeof(readMessageBuffer) - readMessageBufferReadLength, readMessageChunkSize);
    CCAPI_LOGGER_TRACE("numBytesToRead = " + toString(numBytesToRead));
    // boost::asio::async_read(stream, boost::asio::buffer(readMessageBuffer + readMessageBufferReadLength, numBytesToRead),
    //                         beast::bind_front_handler(&FixServiceCoinbase::onRead_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));
    boost::asio::async_read(stream, boost::asio::buffer(data, numBytesToRead),
                            beast::bind_front_handler(&FixServiceCoinbase::onRead_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));
    CCAPI_LOGGER_TRACE("after async_read");
  }
  void onRead_3(std::shared_ptr<FixConnection> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    std::map<int, std::string> field_dictionary;
    hff::dictionary_init_field(field_dictionary);

    if (!ec) {
      auto& connectionId = fixConnectionPtr->id;
      auto& readMessageBuffer = readMessageBufferByConnectionIdMap[connectionId];
      auto& readMessageBufferReadLength = this->readMessageBufferReadLengthByConnectionIdMap[connectionId];
      readMessageBufferReadLength += n;
      hff::message_reader reader(readMessageBuffer.data(), readMessageBuffer.data() + readMessageBufferReadLength);

      // Try to read as many complete messages as there are in the buffer.
      for (; reader.is_complete(); reader = reader.next_message_reader()) {
        if (reader.is_valid()) {
          // Here is a complete message. Read fields out of the reader.
          try {
            if (reader.message_type()->value() == "A") {
              std::cout << "Logon message\n";

              hff::message_reader::const_iterator i = reader.begin();

              if (reader.find_with_hint(hff::tag::SenderCompID, i)) std::cout << "SenderCompID = " << i++->value() << '\n';

              if (reader.find_with_hint(hff::tag::MsgSeqNum, i)) std::cout << "MsgSeqNum    = " << i++->value().as_int<int>() << '\n';

              // if (reader.find_with_hint(hff::tag::SendingTime, i))
              //     std::cout
              //         << "SendingTime  = "
              //         << i++->value().as_timestamp() << '\n';

              std::cout << "The next field is " << hff::field_name(i->tag(), field_dictionary) << " = " << i->value() << '\n';

              std::cout << '\n';
            } else if (reader.message_type()->value() == "D") {
              std::cout << "New Order Single message\n";

              hff::message_reader::const_iterator i = reader.begin();

              if (reader.find_with_hint(hff::tag::Side, i)) std::cout << (i++->value().as_char() == '1' ? "Buy " : "Sell ");

              if (reader.find_with_hint(hff::tag::Symbol, i)) std::cout << i++->value() << " ";

              if (reader.find_with_hint(hff::tag::OrderQty, i)) std::cout << i++->value().as_int<int>();

              if (reader.find_with_hint(hff::tag::Price, i)) {
                int mantissa, exponent;
                i->value().as_decimal(mantissa, exponent);
                std::cout << " @ $" << mantissa << "E" << exponent;
                ++i;
              }

              std::cout << "\n\n";
            }

          } catch (std::exception& ex) {
            std::cerr << "Error reading fields: " << ex.what() << '\n';
          }

        } else {
          // An invalid, corrupted FIX message. Do not try to read fields
          // out of this reader. The beginning of the invalid message is
          // at location reader.message_begin() in the buffer, but the
          // end of the invalid message is unknown (because it's invalid).
          //
          // Stay in this for loop, because the
          // messager_reader::next_message_reader() function will see
          // that this message is invalid and it will search the
          // remainder of the buffer for the text "8=FIX", to see if
          // there might be a complete or partial valid message anywhere
          // else in the remainder of the buffer.
          //
          // Set the return code non-zero to indicate that there was
          // an invalid message, and print the first 64 chars of the
          // invalid message.
          // return_code = 1;
          std::cerr << "Error Invalid FIX message: ";
          std::cerr.write(reader.message_begin(), std::min(ssize_t(64), readMessageBuffer.data() + readMessageBufferReadLength - reader.message_begin()));
          std::cerr << "...\n";
        }
      }
      readMessageBufferReadLength = reader.buffer_end() - reader.buffer_begin();

      if (readMessageBufferReadLength > 0) {  // Then there is an incomplete message at the end of the buffer.
        // Move the partial portion of the incomplete message to buffer[0].
        std::memmove(readMessageBuffer.data(), reader.buffer_begin(), readMessageBufferReadLength);
      }

      this->startRead_3(fixConnectionPtr, readMessageBuffer.data() + readMessageBufferReadLength,
                        std::min(readMessageBuffer.size() - readMessageBufferReadLength, this->readMessageChunkSize));
    }
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
  void sendRequestFix(const Request& request, const TimePoint& now) override {
    CCAPI_LOGGER_FUNCTION_ENTER;
    wspp::lib::asio::post(this->serviceContextPtr->tlsClientPtr->get_io_service(), [that = shared_from_base<FixServiceCoinbase>(), request, now]() {
      CCAPI_LOGGER_DEBUG("request = " + toString(request));
      auto nowFixTimeStr = UtilTime::convertTimePointToFIXTime(now);
      auto& connectionId = request.getCorrelationId();
      // std::shared_ptr<FixConnection> fixConnectionPtr;

      auto it = that->fixConnectionPtrByConnectionIdMap.find(connectionId);
      if (it == that->fixConnectionPtrByConnectionIdMap.end()) {
        // error connection not found
        return;
      }
      auto& fixConnectionPtr = it->second;
      auto& writeMessageBuffer = that->writeMessageBufferByConnectionIdMap[connectionId];
      auto& writeMessageBufferWrittenLength = that->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
      auto numBytesToWrite = 0;
      for (const auto& param : request.getParamListFix()) {
        hffix::message_writer messageWriter(writeMessageBuffer.data() + writeMessageBufferWrittenLength + numBytesToWrite,
                                            writeMessageBuffer.data() + writeMessageBuffer.size());
        messageWriter.push_back_header("FIX.4.2");  // Write BeginString and BodyLength.
        auto it = param.begin();
        if (it != param.end()) {
          messageWriter.push_back_string(it->first, it->second);
          messageWriter.push_back_string(hff::tag::SenderCompID, mapGetWithDefault(that->credentialByConnectionIdMap[connectionId], that->apiKeyName));
          messageWriter.push_back_string(hff::tag::TargetCompID, "Coinbase");
          auto msgSeqNum = std::to_string(++that->sequenceSentByConnectionIdMap[connectionId]);
          messageWriter.push_back_string(hff::tag::MsgSeqNum, msgSeqNum);
          messageWriter.push_back_string(hff::tag::SendingTime, nowFixTimeStr);
          ++it;
          while (it != param.end()) {
            messageWriter.push_back_string(it->first, it->second);
            ++it;
          }
        }
        messageWriter.push_back_trailer();  // write CheckSum.
        numBytesToWrite += messageWriter.message_end() - messageWriter.message_begin();
      }

      if (writeMessageBufferWrittenLength == 0) {
        that->startWrite_3(fixConnectionPtr, writeMessageBuffer.data(), numBytesToWrite);
      }
      writeMessageBufferWrittenLength += numBytesToWrite;

      // CCAPI_LOGGER_TRACE("before async_write");
      // auto numBytesToWrite = new_order.message_end() - buffer;
      // CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
      // boost::asio::async_write(stream, boost::asio::buffer(writeMessageBuffer, numBytesToWrite),
      //                         beast::bind_front_handler(&FixServiceCoinbase::onWrite_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));
      // CCAPI_LOGGER_TRACE("after async_write");
    });

    CCAPI_LOGGER_FUNCTION_EXIT;
  }
  void startWrite_3(std::shared_ptr<FixConnection> fixConnectionPtr, void* data, size_t numBytesToWrite) {
    beast::ssl_stream<beast::tcp_stream>& stream = *fixConnectionPtr->streamPtr;
    CCAPI_LOGGER_TRACE("before async_write");
    // auto numBytesToWrite = messageWriter.message_end() - writeMessageBuffer.data();
    CCAPI_LOGGER_TRACE("numBytesToWrite = " + toString(numBytesToWrite));
    boost::asio::async_write(stream, boost::asio::buffer(data, numBytesToWrite),
                             beast::bind_front_handler(&FixServiceCoinbase::onWrite_3, shared_from_base<FixServiceCoinbase>(), fixConnectionPtr));

    CCAPI_LOGGER_TRACE("after async_write");
  }
  void onWrite_3(std::shared_ptr<FixConnection> fixConnectionPtr, const boost::system::error_code& ec, std::size_t n) {
    auto& connectionId = fixConnectionPtr->id;
    auto& writeMessageBuffer = this->writeMessageBufferByConnectionIdMap[connectionId];
    auto& writeMessageBufferWrittenLength = this->writeMessageBufferWrittenLengthByConnectionIdMap[connectionId];
    writeMessageBufferWrittenLength -= n;
    CCAPI_LOGGER_TRACE("writeMessageBufferWrittenLength = " + toString(writeMessageBufferWrittenLength));
    if (writeMessageBufferWrittenLength > 0) {
      std::memmove(writeMessageBuffer.data(), writeMessageBuffer.data() + n, writeMessageBufferWrittenLength);
      this->startWrite_3(fixConnectionPtr, writeMessageBuffer.data(), writeMessageBufferWrittenLength);
    }
  }
  // const size_t chunksize = 4096;
  const size_t readMessageChunkSize = CCAPI_HFFIX_READ_MESSAGE_CHUNK_SIZE;

  // char readMessageBuffer[1 << 20]; // depends on correlationId
  // size_t readMessageBufferReadLength{};  // The number of bytes read in buffer[]. depends on correlationId
  // char writeMessageBuffer[1 << 20]; // depends on correlationId
  // size_t writeMessageBufferWrittenLength{};

  std::map<std::string, std::array<char, 1 << 20>> readMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> readMessageBufferReadLengthByConnectionIdMap;
  std::map<std::string, std::array<char, 1 << 20>> writeMessageBufferByConnectionIdMap;
  std::map<std::string, size_t> writeMessageBufferWrittenLengthByConnectionIdMap;

  std::map<std::string, std::shared_ptr<FixConnection>> fixConnectionPtrByConnectionIdMap;
  std::map<std::string, int> sequenceSentByConnectionIdMap;
  std::map<std::string, int> sequenceReceivedByConnectionIdMap;

  std::map<std::string, std::map<std::string, std::string>> credentialByConnectionIdMap;

  // std::map<std::string, bool> isWritingMessageByConnectionIdMap;
  // std::map<std::string, std::queue> pendingWriteMessageListByConnectionIdMap;

  // std::map<int, std::string> field_dictionary;

  std::string baseUrlFix;
  std::string apiKeyName;
  std::string apiSecretName;
  std::string apiPassphraseName;
  std::string hostFix;
  std::string portFix;
  // size_t fred; // Number of bytes read from fread().
  // void onFail_(WsConnection& wsConnection) {
  //   wsConnection.status = WsConnection::Status::FAILED;
  //   this->onError(Event::Type::FIX_STATUS, Message::Type::FIX_FAILURE, "connection " + toString(wsConnection) + " has failed before
  //   opening"); WsConnection thisWsConnection = wsConnection; this->wsConnectionMap.erase(thisWsConnection.id);
  //   this->instrumentGroupByWsConnectionIdMap.erase(thisWsConnection.id);
  //   auto urlBase = UtilString::split(thisWsConnection.url, "?").at(0);
  //   long seconds = std::round(UtilAlgorithm::exponentialBackoff(1, 1, 2, std::min(this->connectNumRetryOnFailByConnectionUrlMap[thisWsConnection.url], 6)));
  //   CCAPI_LOGGER_INFO("about to set timer for " + toString(seconds) + " seconds");
  //   if (this->connectRetryOnFailTimerByConnectionIdMap.find(thisWsConnection.id) != this->connectRetryOnFailTimerByConnectionIdMap.end()) {
  //     this->connectRetryOnFailTimerByConnectionIdMap.at(thisWsConnection.id)->cancel();
  //   }
  //   this->connectRetryOnFailTimerByConnectionIdMap[thisWsConnection.id] = this->serviceContextPtr->tlsClientPtr->set_timer(
  //       seconds * 1000, [thisWsConnection, that = shared_from_base<MarketDataService>(), urlBase](ErrorCode const& ec) {
  //         if (that->wsConnectionMap.find(thisWsConnection.id) == that->wsConnectionMap.end()) {
  //           if (ec) {
  //             CCAPI_LOGGER_ERROR("wsConnection = " + toString(thisWsConnection) + ", connect retry on fail timer error: " + ec.message());
  //             that->onError(Event::Type::FIX_STATUS, Message::Type::GENERIC_ERROR, ec, "timer");
  //           } else {
  //             CCAPI_LOGGER_INFO("about to retry");
  //             auto thatWsConnection = thisWsConnection;
  //             thatWsConnection.assignDummyId();
  //             that->prepareConnect(thatWsConnection);
  //             that->connectNumRetryOnFailByConnectionUrlMap[urlBase] += 1;
  //           }
  //         }
  //       });
  // }
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_FIX_SERVICE_COINBASE_H_
