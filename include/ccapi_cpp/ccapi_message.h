#ifndef INCLUDE_CCAPI_CPP_CCAPI_MESSAGE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MESSAGE_H_
#include <chrono>
#include <vector>

#include "ccapi_cpp/ccapi_element.h"
#include "ccapi_cpp/ccapi_logger.h"
namespace ccapi {
/**
 * A handle to a single message. Message objects are obtained from the getMessageList() function of the Event object. Each Message is associated with one or
 * more correlation id values. The Message contents are represented as Elements and can be accessed via the getElementList() function. Each Message object
 * consists of an Type attribute and a RecapType attribute. The exchange timestamp (if any) associated with the Messsage object can be retrieved via the
 * getTime() function. The library timestamp can be retrieved via the getTimeReceived() function.
 */
class Message CCAPI_FINAL {
 public:
  enum class RecapType {
    UNKNOWN,
    SOLICITED,  // A recap. For market depth, it represents the initial order book snapshot. For public trade, it represents the most recent historical trades.
                // This is the first batch of data points.
    NONE,  // Normal data tick, not a recap. For market depth, it represents the updated order book state. For public trade, it represents the new trades. These
           // are the batches of data points after the first batch.
  };
  static std::string recapTypeToString(RecapType recapType) {
    std::string output;
    switch (recapType) {
      case RecapType::UNKNOWN:
        output = "UNKNOWN";
        break;
      case RecapType::NONE:
        output = "NONE";
        break;
      case RecapType::SOLICITED:
        output = "SOLICITED";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  enum class Type {
    UNKNOWN,
    AUTHORIZATION_SUCCESS,
    AUTHORIZATION_FAILURE,
    MARKET_DATA_EVENTS_MARKET_DEPTH,
    MARKET_DATA_EVENTS_TRADE,
    MARKET_DATA_EVENTS_AGG_TRADE,
    EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE,
    EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE,
    SUBSCRIPTION_STARTED,
    SUBSCRIPTION_FAILURE,
    SESSION_CONNECTION_UP,
    SESSION_CONNECTION_DOWN,
    INCORRECT_STATE_FOUND,
    CREATE_ORDER,
    CANCEL_ORDER,
    GET_ORDER,
    GET_OPEN_ORDERS,
    CANCEL_OPEN_ORDERS,
    GET_ACCOUNTS,
    GET_ACCOUNT_BALANCES,
    GET_ACCOUNT_POSITIONS,
    GET_RECENT_TRADES,
    GET_RECENT_AGG_TRADES,
    GET_INSTRUMENT,
    GET_INSTRUMENTS,
    RESPONSE_ERROR,
    REQUEST_FAILURE,
    GENERIC_ERROR,
    CUSTOM,
    FIX,
    FIX_FAILURE,
    GENERIC_PUBLIC_REQUEST,
    GENERIC_PUBLIC_SUBSCRIPTION,
    GENERIC_PRIVATE_REQUEST,
  };
  static std::string typeToString(Type type) {
    std::string output;
    switch (type) {
      case Type::UNKNOWN:
        output = "UNKNOWN";
        break;
      case Type::MARKET_DATA_EVENTS_MARKET_DEPTH:
        output = "MARKET_DATA_EVENTS_MARKET_DEPTH";
        break;
      case Type::MARKET_DATA_EVENTS_TRADE:
        output = "MARKET_DATA_EVENTS_TRADE";
        break;
      case Type::MARKET_DATA_EVENTS_AGG_TRADE:
        output = "MARKET_DATA_EVENTS_AGG_TRADE";
        break;
      case Type::AUTHORIZATION_SUCCESS:
        output = "AUTHORIZATION_SUCCESS";
        break;
      case Type::AUTHORIZATION_FAILURE:
        output = "AUTHORIZATION_FAILURE";
        break;
      case Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE:
        output = "EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE";
        break;
      case Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE:
        output = "EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE";
        break;
      case Type::SUBSCRIPTION_STARTED:
        output = "SUBSCRIPTION_STARTED";
        break;
      case Type::SUBSCRIPTION_FAILURE:
        output = "SUBSCRIPTION_FAILURE";
        break;
      case Type::SESSION_CONNECTION_UP:
        output = "SESSION_CONNECTION_UP";
        break;
      case Type::SESSION_CONNECTION_DOWN:
        output = "SESSION_CONNECTION_DOWN";
        break;
      case Type::INCORRECT_STATE_FOUND:
        output = "INCORRECT_STATE_FOUND";
        break;
      case Type::CREATE_ORDER:
        output = "CREATE_ORDER";
        break;
      case Type::CANCEL_ORDER:
        output = "CANCEL_ORDER";
        break;
      case Type::GET_ORDER:
        output = "GET_ORDER";
        break;
      case Type::GET_OPEN_ORDERS:
        output = "GET_OPEN_ORDERS";
        break;
      case Type::CANCEL_OPEN_ORDERS:
        output = "CANCEL_OPEN_ORDERS";
        break;
      case Type::GET_ACCOUNTS:
        output = "GET_ACCOUNTS";
        break;
      case Type::GET_ACCOUNT_BALANCES:
        output = "GET_ACCOUNT_BALANCES";
        break;
      case Type::GET_ACCOUNT_POSITIONS:
        output = "GET_ACCOUNT_POSITIONS";
        break;
      case Type::GET_RECENT_TRADES:
        output = "GET_RECENT_TRADES";
        break;
      case Type::GET_RECENT_AGG_TRADES:
        output = "GET_RECENT_AGG_TRADES";
        break;
      case Type::GET_INSTRUMENT:
        output = "GET_INSTRUMENT";
        break;
      case Type::GET_INSTRUMENTS:
        output = "GET_INSTRUMENTS";
        break;
      case Type::RESPONSE_ERROR:
        output = "RESPONSE_ERROR";
        break;
      case Type::REQUEST_FAILURE:
        output = "REQUEST_FAILURE";
        break;
      case Type::GENERIC_ERROR:
        output = "GENERIC_ERROR";
        break;
      case Type::CUSTOM:
        output = "CUSTOM";
        break;
      case Type::FIX:
        output = "FIX";
        break;
      case Type::FIX_FAILURE:
        output = "FIX_FAILURE";
        break;
      case Type::GENERIC_PUBLIC_REQUEST:
        output = "GENERIC_PUBLIC_REQUEST";
        break;
      case Type::GENERIC_PUBLIC_SUBSCRIPTION:
        output = "GENERIC_PUBLIC_SUBSCRIPTION";
        break;
      case Type::GENERIC_PRIVATE_REQUEST:
        output = "GENERIC_PRIVATE_REQUEST";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  std::string toString() const {
    std::string output = "Message [type = " + typeToString(type) + ", recapType = " + recapTypeToString(recapType) +
                         ", time = " + UtilTime::getISOTimestamp(time) + ", timeReceived = " + UtilTime::getISOTimestamp(timeReceived) +
                         ", elementList = " + ccapi::firstNToString(elementList, 10) + ", correlationIdList = " + ccapi::toString(correlationIdList) +
                         ", secondaryCorrelationIdMap = " + ccapi::toString(secondaryCorrelationIdMap) + "]";
    return output;
  }
  std::string toStringPretty(const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) const {
    std::string sl(leftToIndent, ' ');
    std::string ss(leftToIndent + space, ' ');
    std::string output = (indentFirstLine ? sl : "") + "Message [\n" + ss + "type = " + typeToString(type) + ",\n" + ss +
                         "recapType = " + recapTypeToString(recapType) + ",\n" + ss + "time = " + UtilTime::getISOTimestamp(time) + ",\n" + ss +
                         "timeReceived = " + UtilTime::getISOTimestamp(timeReceived) + ",\n" + ss +
                         "elementList = " + ccapi::firstNToStringPretty(elementList, 10, space, space + leftToIndent, false) + ",\n" + ss +
                         "correlationIdList = " + ccapi::toString(correlationIdList) + ",\n" + ss +
                         "secondaryCorrelationIdMap = " + ccapi::toString(secondaryCorrelationIdMap) + "\n" + sl + "]";
    return output;
  }
  const std::vector<Element>& getElementList() const { return elementList; }
  void setElementList(const std::vector<Element>& elementList) { this->elementList = elementList; }
  void setElementList(std::vector<Element>& elementList) { this->elementList = std::move(elementList); }
  const std::vector<std::string>& getCorrelationIdList() const { return correlationIdList; }
  const std::map<std::string, std::string>& getSecondaryCorrelationIdMap() const { return secondaryCorrelationIdMap; }
  void setCorrelationIdList(const std::vector<std::string>& correlationIdList) { this->correlationIdList = correlationIdList; }
  void setSecondaryCorrelationIdMap(const std::map<std::string, std::string>& secondaryCorrelationIdMap) {
    this->secondaryCorrelationIdMap = secondaryCorrelationIdMap;
  }
  // 'getTime' only works in C++. For other languages, please use 'getTimeISO'.
  TimePoint getTime() const { return time; }
  std::string getTimeISO() const { return UtilTime::getISOTimestamp(time); }
  std::pair<long long, long long> getTimeUnix() const { return UtilTime::divide(time); }
  std::pair<long long, long long> getTimePair() const { return UtilTime::divide(time); }
  void setTime(TimePoint time) { this->time = time; }
  RecapType getRecapType() const { return recapType; }
  void setRecapType(RecapType recapType) { this->recapType = recapType; }
  Type getType() const { return type; }
  void setType(Type type) { this->type = type; }
  // 'getTimeReceived' only works in C++. For other languages, please use 'getTimeReceivedISO'.
  TimePoint getTimeReceived() const { return timeReceived; }
  std::string getTimeReceivedISO() const { return UtilTime::getISOTimestamp(timeReceived); }
  std::pair<long long, long long> getTimeReceivedUnix() const { return UtilTime::divide(timeReceived); }
  std::pair<long long, long long> getTimeReceivedPair() const { return UtilTime::divide(timeReceived); }
  void setTimeReceived(TimePoint timeReceived) { this->timeReceived = timeReceived; }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  TimePoint time{std::chrono::seconds{0}};
  TimePoint timeReceived{std::chrono::seconds{0}};
  std::vector<Element> elementList;
  std::vector<std::string> correlationIdList;
  std::map<std::string, std::string> secondaryCorrelationIdMap;
  Type type{Type::UNKNOWN};
  RecapType recapType{RecapType::UNKNOWN};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MESSAGE_H_
