#ifndef INCLUDE_CCAPI_CPP_CCAPI_MESSAGE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MESSAGE_H_
#include <chrono>
#include <vector>
#include "ccapi_cpp/ccapi_element.h"
#include "ccapi_cpp/ccapi_logger.h"
namespace ccapi {
class Message CCAPI_FINAL {
 public:
  enum class RecapType {
    UNKNOWN,
    NONE,      // normal data tick; not a recap
    SOLICITED  // generated on request by subscriber
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
    MARKET_DATA_EVENTS,
    EXECUTION_MANAGEMENT_EVENTS,
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
    GET_RECENT_TRADES,
    ORDER_MATCHED,
    RESPONSE_ERROR,
    REQUEST_FAILURE,
    GENERIC_ERROR
  };
  static std::string typeToString(Type type) {
    std::string output;
    switch (type) {
      case Type::UNKNOWN:
        output = "UNKNOWN";
        break;
      case Type::MARKET_DATA_EVENTS:
        output = "MARKET_DATA_EVENTS";
        break;
      case Type::EXECUTION_MANAGEMENT_EVENTS:
        output = "EXECUTION_MANAGEMENT_EVENTS";
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
      case Type::GET_RECENT_TRADES:
        output = "GET_RECENT_TRADES";
        break;
      case Type::ORDER_MATCHED:
        output = "ORDER_MATCHED";
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
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  std::string toString() const {
    std::string output = "Message [type = " + typeToString(type) + ", recapType = " + recapTypeToString(recapType) +
                         ", time = " + UtilTime::getISOTimestamp(time) + ", timeReceived = " + UtilTime::getISOTimestamp(timeReceived) +
                         ", elementList = " + ccapi::firstNToString(elementList, 10) + ", correlationIdList = " + ccapi::toString(correlationIdList) + "]";
    return output;
  }
  std::string toStringPretty(const int space = 2, const int leftToIndent = 0, const bool indentFirstLine = true) const {
    std::string sl(leftToIndent, ' ');
    std::string ss(leftToIndent + space, ' ');
    std::string output = (indentFirstLine ? sl : "") + "Message [\n" + ss + "type = " + typeToString(type) + ",\n" + ss +
                         "recapType = " + recapTypeToString(recapType) + ",\n" + ss + "time = " + UtilTime::getISOTimestamp(time) + ",\n" + ss +
                         "timeReceived = " + UtilTime::getISOTimestamp(timeReceived) + ",\n" + ss +
                         "elementList = " + ccapi::firstNToStringPretty(elementList, 10, space, space + leftToIndent, false) + ",\n" + ss +
                         "correlationIdList = " + ccapi::toString(correlationIdList) + "\n" + sl + "]";
    return output;
  }
  const std::vector<Element>& getElementList() const { return elementList; }
  void setElementList(const std::vector<Element>& elementList) { this->elementList = elementList; }
  void setElementList(std::vector<Element>& elementList) { this->elementList = std::move(elementList); }
  const std::vector<std::string>& getCorrelationIdList() const { return correlationIdList; }
  void setCorrelationIdList(const std::vector<std::string>& correlationIdList) { this->correlationIdList = correlationIdList; }
  TimePoint getTime() const { return time; }
  std::string getTimeISO() const { return UtilTime::getISOTimestamp(time); }
  std::pair<long long, long long> getTimePair() const { return UtilTime::divide(time); }
  void setTime(TimePoint time) { this->time = time; }
  RecapType getRecapType() const { return recapType; }
  void setRecapType(RecapType recapType) { this->recapType = recapType; }
  Type getType() const { return type; }
  void setType(Type type) { this->type = type; }
  TimePoint getTimeReceived() const { return timeReceived; }
  std::string getTimeReceivedISO() const { return UtilTime::getISOTimestamp(timeReceived); }
  std::pair<long long, long long> getTimeReceivedPair() const { return UtilTime::divide(timeReceived); }
  void setTimeReceived(TimePoint timeReceived) { this->timeReceived = timeReceived; }

 private:
  TimePoint time{std::chrono::seconds{0}};
  TimePoint timeReceived{std::chrono::seconds{0}};
  std::vector<Element> elementList;
  std::vector<std::string> correlationIdList;
  Type type{Type::UNKNOWN};
  RecapType recapType{RecapType::UNKNOWN};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MESSAGE_H_
