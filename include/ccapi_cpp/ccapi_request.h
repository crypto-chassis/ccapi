#ifndef INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#define INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
// We use macros instead of static constants in the Request class so that SWIG can properly generate C# bindings
#define CCAPI_REQUEST_OPERATION_TYPE_CUSTOM 0x100
#define CCAPI_REQUEST_OPERATION_TYPE_GENERIC_PUBLIC_REQUEST 0x200
#define CCAPI_REQUEST_OPERATION_TYPE_GENERIC_PRIVATE_REQUEST 0x300
#define CCAPI_REQUEST_OPERATION_TYPE_FIX 0x400
#define CCAPI_REQUEST_OPERATION_TYPE_MARKET_DATA 0x500
#define CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT 0x600
#define CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ORDER CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT
#define CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ACCOUNT 0x700
namespace ccapi {
/**
 * A single request. Request objects are created using Request constructors. They are used with Session::sendRequest() or Session::sendRequestByWebsocket() or
 * Session::sendRequestByFix(). The Request object contains the parameters for a single request. Once a Request has been created its fields can be further
 * modified using the convenience functions appendParam() or appendParamFix() or setParamList() or setParamListFix(). A correlation id can be used as the unique
 * identifier to tag all data associated with this request.
 */
class Request CCAPI_FINAL {
 public:
  enum class Operation {
    CUSTOM = CCAPI_REQUEST_OPERATION_TYPE_CUSTOM,
    GENERIC_PUBLIC_REQUEST = CCAPI_REQUEST_OPERATION_TYPE_GENERIC_PUBLIC_REQUEST,
    GENERIC_PRIVATE_REQUEST = CCAPI_REQUEST_OPERATION_TYPE_GENERIC_PRIVATE_REQUEST,
    FIX = CCAPI_REQUEST_OPERATION_TYPE_FIX,
    GET_RECENT_TRADES = CCAPI_REQUEST_OPERATION_TYPE_MARKET_DATA,
    GET_HISTORICAL_TRADES,
    GET_RECENT_AGG_TRADES,
    GET_HISTORICAL_AGG_TRADES,
    GET_RECENT_CANDLESTICKS,
    GET_HISTORICAL_CANDLESTICKS,
    GET_MARKET_DEPTH,
    GET_INSTRUMENT,
    GET_INSTRUMENTS,
    CREATE_ORDER = CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ORDER,
    CANCEL_ORDER,
    GET_ORDER,
    GET_OPEN_ORDERS,
    CANCEL_OPEN_ORDERS,
    GET_ACCOUNTS = CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT_ACCOUNT,
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
      case Operation::GET_HISTORICAL_TRADES:
        output = "GET_HISTORICAL_TRADES";
        break;
      case Operation::GET_RECENT_AGG_TRADES:
        output = "GET_RECENT_AGG_TRADES";
        break;
      case Operation::GET_HISTORICAL_AGG_TRADES:
        output = "GET_HISTORICAL_AGG_TRADES";
        break;
      case Operation::GET_RECENT_CANDLESTICKS:
        output = "GET_RECENT_CANDLESTICKS";
        break;
      case Operation::GET_HISTORICAL_CANDLESTICKS:
        output = "GET_HISTORICAL_CANDLESTICKS";
        break;
      case Operation::GET_MARKET_DEPTH:
        output = "GET_MARKET_DEPTH";
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
      this->serviceName = static_cast<int>(operation) >= CCAPI_REQUEST_OPERATION_TYPE_EXECUTION_MANAGEMENT ? CCAPI_EXECUTION_MANAGEMENT : CCAPI_MARKET_DATA;
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
        "Request [exchange = " + exchange + ", marginType = " + marginType + ", instrument = " + instrument + ", serviceName = " + serviceName +
        ", correlationId = " + correlationId + ", secondaryCorrelationId = " + secondaryCorrelationId +
        (this->serviceName == CCAPI_FIX ? ", paramListFix = " + ccapi::toString(paramListFix) : ", paramList = " + ccapi::toString(paramList)) +
        ", credential = " + ccapi::toString(shortCredential) + ", operation = " + operationToString(operation) +
        ", timeSent = " + UtilTime::getISOTimestamp(timeSent) + ", index = " + ccapi::toString(index) + ", localIpAddress = " + localIpAddress +
        ", baseUrl = " + baseUrl + "]";
    return output;
  }
  const std::string& getCorrelationId() const { return correlationId; }
  const std::string& getSecondaryCorrelationId() const { return secondaryCorrelationId; }
  const std::string& getExchange() const { return exchange; }
  const std::string& getMarginType() const { return marginType; }
  const std::string& getInstrument() const { return instrument; }
  const std::map<std::string, std::string>& getCredential() const { return credential; }
  const std::string& getServiceName() const { return serviceName; }
  void appendParam(const std::map<std::string, std::string>& param) { this->paramList.push_back(param); }
  void appendParamFix(const std::vector<std::pair<int, std::string> >& param) { this->paramListFix.push_back(param); }
  void appendParamListFix(const std::vector<std::vector<std::pair<int, std::string> > >& paramList) {
    this->paramListFix.insert(std::end(this->paramListFix), std::begin(paramList), std::end(paramList));
  }
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
  const std::string& getLocalIpAddress() const { return localIpAddress; }
  const std::string& getBaseUrl() const { return baseUrl; }
  const std::string& getHost() const { return host; }
  const std::string& getPort() const { return port; }
  void setIndex(int index) { this->index = index; }
  void setCredential(const std::map<std::string, std::string>& credential) { this->credential = credential; }
  void setCorrelationId(const std::string& correlationId) { this->correlationId = correlationId; }
  void setSecondaryCorrelationId(const std::string& secondaryCorrelationId) { this->secondaryCorrelationId = secondaryCorrelationId; }
  void setMarginType(const std::string& marginType) { this->marginType = marginType; }
  void setLocalIpAddress(const std::string& localIpAddress) { this->localIpAddress = localIpAddress; }
  void setBaseUrl(const std::string& baseUrl) {
    this->baseUrl = baseUrl;
    this->setBaseUrlParts();
  }
  void setBaseUrlParts() {
    auto splitted1 = UtilString::split(this->baseUrl, "://");
    if (splitted1.size() >= 2) {
      auto splitted2 = UtilString::split(UtilString::split(splitted1.at(1), "/").at(0), ":");
      this->host = splitted2.at(0);
      if (splitted2.size() == 2) {
        this->port = splitted2.at(1);
      } else {
        if (splitted1.at(0) == "https" || splitted1.at(0) == "wss") {
          this->port = CCAPI_HTTPS_PORT_DEFAULT;
        } else {
          this->port = CCAPI_HTTP_PORT_DEFAULT;
        }
      }
    }
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  std::string exchange;
  std::string marginType;
  std::string instrument;
  std::string serviceName;
  std::string correlationId;
  std::string secondaryCorrelationId;
  std::vector<std::map<std::string, std::string> > paramList;
  std::map<std::string, std::string> credential;
  Operation operation;
  std::vector<std::vector<std::pair<int, std::string> > > paramListFix;
  TimePoint timeSent{std::chrono::seconds{0}};
  int index{};
  std::string localIpAddress;
  std::string baseUrl;
  std::string host;
  std::string port;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
