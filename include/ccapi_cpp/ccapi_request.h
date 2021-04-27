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
  static constexpr int operationTypeMarketData = 0x100;
  static constexpr int operationTypeExecutionManagement = 0x200;
  enum class Operation {
    GET_RECENT_TRADES = operationTypeMarketData,
    CREATE_ORDER = operationTypeExecutionManagement,
    CANCEL_ORDER,
    GET_ORDER,
    GET_OPEN_ORDERS,
    CANCEL_OPEN_ORDERS,
    GET_ACCOUNT_BALANCES,
    GET_ACCOUNT_POSITIONS,
    GET_OTC_QUOTE,
    GET_OTC_QUOTE_STATUS,
    ACCEPT_OTC_QUOTE,
    GET_ACCOUNT_INFORMATION,
    GET_SINGLE_MARKET_INFORMATION
  };
  static std::string operationToString(Operation operation) {
    std::string output;
    switch (operation) {
      case Operation::GET_RECENT_TRADES:
        output = "GET_RECENT_TRADES";
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
      case Operation::GET_ACCOUNT_BALANCES:
        output = "GET_ACCOUNT_BALANCES";
        break;
      case Operation::GET_ACCOUNT_POSITIONS:
        output = "GET_ACCOUNT_POSITIONS";
        break;
      case Operation::GET_OTC_QUOTE:
        output = "GET_OTC_QUOTE";
        break;
      case Operation::GET_OTC_QUOTE_STATUS:
        output = "GET_OTC_QUOTE_STATUS";
        break;
      case Operation::ACCEPT_OTC_QUOTE:
        output = "ACCEPT_OTC_QUOTE";
        break;
      case Operation::GET_ACCOUNT_INFORMATION:
        output = "GET_ACCOUNT_INFORMATION";
        break;
      case Operation::GET_SINGLE_MARKET_INFORMATION:
        output = "GET_SINGLE_MARKET_INFORMATION";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  // enum class ApiType { UNKNOWN, REST, WEBSOCKET, FIX };
  // static std::string apiTypeToString(ApiType apiType) {
  //   std::string output;
  //   switch (apiType) {
  //     case ApiType::UNKNOWN:
  //       output = "UNKNOWN";
  //       break;
  //     case ApiType::REST:
  //       output = "REST";
  //       break;
  //     case ApiType::WEBSOCKET:
  //       output = "WEBSOCKET";
  //       break;
  //     case ApiType::FIX:
  //       output = "FIX";
  //       break;
  //     default:
  //       CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
  //   }
  //   return output;
  // }
  Request() {}
  Request(Operation operation, std::string exchange, std::string instrument = "", std::string correlationId = "",
          std::map<std::string, std::string> credential = {})
      : operation(operation), exchange(exchange), instrument(instrument), correlationId(correlationId), credential(credential) {
    this->serviceName = static_cast<int>(operation) >= operationTypeExecutionManagement ? CCAPI_EXECUTION_MANAGEMENT : CCAPI_MARKET_DATA;
    if (this->correlationId.empty()) {
      this->correlationId = UtilString::generateRandomString(CCAPI_CORRELATION_ID_GENERATED_LENGTH);
    }
  }
  std::string toString() const {
    std::map<std::string, std::string> shortCredential;
    for (const auto &x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    std::string output = "Request [exchange = " + exchange + ", instrument = " + instrument + ", serviceName = " + serviceName +
                         ", correlationId = " + correlationId + ", paramList = " + ccapi::toString(paramList) +
                         ", credential = " + ccapi::toString(shortCredential) + ", operation = " + operationToString(operation) + "]";
    return output;
  }
  const std::string &getCorrelationId() const { return correlationId; }
  const std::string &getExchange() const { return exchange; }
  const std::string &getInstrument() const { return instrument; }
  const std::map<std::string, std::string> &getCredential() const { return credential; }
  const std::string &getServiceName() const { return serviceName; }
  void appendParam(const std::map<std::string, std::string> &param) { this->paramList.push_back(param); }
  Operation getOperation() const { return operation; }
  const std::vector<std::map<std::string, std::string>> &getParamList() const { return paramList; }
  void setParamList(const std::vector<std::map<std::string, std::string>> &paramList) { this->paramList = paramList; }
  std::map<std::string, std::string> getFirstParamWithDefault(const std::map<std::string, std::string> defaultValue = {}) const {
    if (this->paramList.empty()) {
      return defaultValue;
    } else {
      return this->paramList.front();
    }
  }

 private:
  std::string exchange;
  std::string instrument;
  std::string serviceName;
  std::string correlationId;
  std::vector<std::map<std::string, std::string>> paramList;
  std::map<std::string, std::string> credential;
  Operation operation;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
