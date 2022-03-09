#ifndef INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#define INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class Request CCAPI_FINAL {
 public:
  static constexpr int operationTypeCustom = 0x100;
  static constexpr int operationTypeGenericPublicRequest = 0x200;
  static constexpr int operationTypeGenericPrivateRequest = 0x300;
  static constexpr int operationTypeFix = 0x400;
  static constexpr int operationTypeMarketData = 0x500;
  static constexpr int operationTypeExecutionManagement = 0x600;
  static constexpr int operationTypeExecutionManagementOrder = operationTypeExecutionManagement;
  static constexpr int operationTypeExecutionManagementAccount = 0x700;
  enum class Operation {
    CUSTOM = operationTypeCustom,
    GENERIC_PUBLIC_REQUEST = operationTypeGenericPublicRequest,
    GENERIC_PRIVATE_REQUEST = operationTypeGenericPrivateRequest,
    FIX = operationTypeFix,
    GET_RECENT_TRADES = operationTypeMarketData,
    GET_RECENT_AGG_TRADES,
    GET_INSTRUMENT,
    GET_INSTRUMENTS,
    CREATE_ORDER = operationTypeExecutionManagementOrder,
    CANCEL_ORDER,
    GET_ORDER,
    GET_OPEN_ORDERS,
    CANCEL_OPEN_ORDERS,
    GET_ACCOUNTS = operationTypeExecutionManagementAccount,
    GET_ACCOUNT_BALANCES,
    GET_ACCOUNT_POSITIONS,
  };
  static std::string operationToString(Operation operation) {
    std::string output;
    switch (operation) {
      case Operation::CUSTOM:
        output = "CUSTOM";
        break;
      case Operation::GENERIC_PUBLIC_REQUEST:
        output = "GENERIC_PUBLIC_REQUEST";
        break;
      case Operation::GENERIC_PRIVATE_REQUEST:
        output = "GENERIC_PRIVATE_REQUEST";
        break;
      case Operation::FIX:
        output = "FIX";
        break;
      case Operation::GET_RECENT_TRADES:
        output = "GET_RECENT_TRADES";
        break;
      case Operation::GET_RECENT_AGG_TRADES:
        output = "GET_RECENT_AGG_TRADES";
        break;
      case Operation::GET_INSTRUMENT:
        output = "GET_INSTRUMENT";
        break;
      case Operation::GET_INSTRUMENTS:
        output = "GET_INSTRUMENTS";
        break;
      case Operation::CREATE_ORDER:
        output = "CREATE_ORDER";
        break;
      case Operation::CANCEL_ORDER:
        output = "CANCEL_ORDER";
        break;
      case Operation::GET_ORDER:
        output = "GET_ORDER";
        break;
      case Operation::GET_OPEN_ORDERS:
        output = "GET_OPEN_ORDERS";
        break;
      case Operation::CANCEL_OPEN_ORDERS:
        output = "CANCEL_OPEN_ORDERS";
        break;
      case Operation::GET_ACCOUNTS:
        output = "GET_ACCOUNTS";
        break;
      case Operation::GET_ACCOUNT_BALANCES:
        output = "GET_ACCOUNT_BALANCES";
        break;
      case Operation::GET_ACCOUNT_POSITIONS:
        output = "GET_ACCOUNT_POSITIONS";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  Request() {}
  Request(Operation operation, std::string exchange, std::string instrument = "", std::string correlationId = "",
          std::map<std::string, std::string> credential = {})
      : operation(operation), exchange(exchange), instrument(instrument), correlationId(correlationId), credential(credential) {
    if (operation == Operation::CUSTOM) {
      this->serviceName = CCAPI_UNKNOWN;
    } else if (operation == Operation::GENERIC_PUBLIC_REQUEST) {
      this->serviceName = CCAPI_MARKET_DATA;
    } else if (operation == Operation::GENERIC_PRIVATE_REQUEST) {
      this->serviceName = CCAPI_EXECUTION_MANAGEMENT;
    } else if (operation == Operation::FIX) {
      this->serviceName = CCAPI_FIX;
    } else {
      this->serviceName = static_cast<int>(operation) >= operationTypeExecutionManagement ? CCAPI_EXECUTION_MANAGEMENT : CCAPI_MARKET_DATA;
    }
    if (this->correlationId.empty()) {
      this->correlationId = UtilString::generateRandomString(CCAPI_CORRELATION_ID_GENERATED_LENGTH);
    }
  }
  std::string toString() const {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    std::string output =
        "Request [exchange = " + exchange + ", instrument = " + instrument + ", serviceName = " + serviceName + ", correlationId = " + correlationId +
        (this->serviceName == CCAPI_FIX ? ", paramListFix = " + ccapi::toString(paramListFix) : ", paramList = " + ccapi::toString(paramList)) +
        ", credential = " + ccapi::toString(shortCredential) + ", operation = " + operationToString(operation) +
        ", timeSent = " + UtilTime::getISOTimestamp(timeSent) + "]";
    return output;
  }
  const std::string& getCorrelationId() const { return correlationId; }
  const std::string& getExchange() const { return exchange; }
  const std::string& getInstrument() const { return instrument; }
  const std::map<std::string, std::string>& getCredential() const { return credential; }
  const std::string& getServiceName() const { return serviceName; }
  void appendParam(const std::map<std::string, std::string>& param) { this->paramList.push_back(param); }
  void appendParamFix(const std::vector<std::pair<int, std::string> >& param) { this->paramListFix.push_back(param); }
  void setParamListFix(const std::vector<std::vector<std::pair<int, std::string> > >& paramListFix) { this->paramListFix = paramListFix; }
  Operation getOperation() const { return operation; }
  const std::vector<std::map<std::string, std::string> >& getParamList() const { return paramList; }
  const std::vector<std::vector<std::pair<int, std::string> > >& getParamListFix() const { return paramListFix; }
  void setParamList(const std::vector<std::map<std::string, std::string> >& paramList) { this->paramList = paramList; }
  std::map<std::string, std::string> getFirstParamWithDefault(const std::map<std::string, std::string> defaultValue = {}) const {
    if (this->paramList.empty()) {
      return defaultValue;
    } else {
      return this->paramList.front();
    }
  }
  // 'getTimeSent' only works in C++. For other languages, please use 'getTimeSentISO'.
  TimePoint getTimeSent() const { return timeSent; }
  std::string getTimeSentISO() const { return UtilTime::getISOTimestamp(timeSent); }
  std::pair<long long, long long> getTimeSentPair() const { return UtilTime::divide(timeSent); }
  void setTimeSent(TimePoint timeSent) { this->timeSent = timeSent; }
  int getIndex() const { return index; }
  void setIndex(int index) { this->index = index; }
  void setCredential(const std::map<std::string, std::string>& credential) { this->credential = credential; }
  void setCorrelationId(const std::string& correlationId) { this->correlationId = correlationId; }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  std::string exchange;
  std::string instrument;
  std::string serviceName;
  std::string correlationId;
  std::vector<std::map<std::string, std::string> > paramList;
  std::map<std::string, std::string> credential;
  Operation operation;
  std::vector<std::vector<std::pair<int, std::string> > > paramListFix;
  TimePoint timeSent{std::chrono::seconds{0}};
  int index{};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
