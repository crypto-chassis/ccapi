#ifndef INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#define INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#include <string>
#include <map>
#include <mutex>
#include <condition_variable>
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
class Request CCAPI_FINAL {
 public:
  enum class Operation {
      UNKNOWN,
      CREATE_ORDER,
      CANCEL_ORDER,
      GET_ORDER,
      GET_OPEN_ORDERS,
      CANCEL_OPEN_ORDERS
    };
  static std::string operationToString(Operation operation) {
    std::string output;
    switch (operation) {
      case Operation::UNKNOWN:
        output = "UNKNOWN";
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
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  Request(Operation operation, std::string exchange, std::string instrument = "", std::string correlationId =
      "", std::map<std::string, std::string> credential = {})
      : operation(operation), exchange(exchange), instrument(instrument), correlationId(correlationId), credential(credential) {
    this->serviceName = CCAPI_EXECUTION_MANAGEMENT;
    if (this->correlationId.empty()) {
      this->correlationId = UtilString::generateRandomString(CCAPI_CORRELATION_ID_GENERATED_LENGTH);
    }
  }
#ifdef SWIG
  Request(){}
#endif
  std::string toString() const {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    std::string output = "Request [exchange = " + exchange + ", instrument = " + instrument + ", serviceName = "+serviceName+", correlationId = "
        + correlationId +", paramList = "+ccapi::toString(paramList)+ ", credential = "
        + ccapi::toString(shortCredential) + ", operation = " + operationToString(operation) + "]";
    return output;
  }
  const std::string& getCorrelationId() const {
    return correlationId;
  }
  const std::string& getExchange() const {
    return exchange;
  }
  const std::string& getInstrument() const {
    return instrument;
  }
  const std::map<std::string, std::string>& getCredential() const {
    return credential;
  }
  const std::string& getServiceName() const {
    return serviceName;
  }
  void appendParam(const std::map<std::string, std::string>& param) {
      this->paramList.push_back(param);
  }
  Operation getOperation() const {
    return operation;
  }
  const std::vector<std::map<std::string, std::string> >& getParamList() const {
    return paramList;
  }
  void setParamList(const std::vector<std::map<std::string, std::string> >& paramList) {
    this->paramList = paramList;
  }

 private:
  std::string exchange;
  std::string instrument;
  std::string serviceName;
  std::string correlationId;
  std::vector<std::map<std::string, std::string> > paramList;
  std::map<std::string, std::string> credential;
  Operation operation;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
