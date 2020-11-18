#ifndef INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#define INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
#include <string>
#include <map>
#include <mutex>
#include <condition_variable>
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class Request final {
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
      default:
        CCAPI_LOGGER_FATAL("");
    }
    return output;
  }
  Request(Operation operation, std::map<std::string, std::string> credential, std::string exchange, std::string instrument = "", std::string correlationId =
      "")
      : operation(operation), credential(credential), exchange(exchange), instrument(instrument), correlationId(correlationId) {
    this->serviceName = CCAPI_EXECUTION_MANAGEMENT;
    if (this->correlationId.empty()) {
      this->correlationId = UtilString::generateRandomString(CCAPI_CORRELATION_ID_GENERATED_LENGTH);
    }
  }
  std::string toString() const {
    std::string output = "Request [exchange = " + exchange + ", instrument = " + instrument + ", serviceName = "+serviceName+", correlationId = "
        + correlationId +", paramMap = "+ccapi::toString(paramMap)+ ", credential = "
        + ccapi::toString(credential) + ", operation = " + operationToString(operation) + "]";
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
  void setParam(std::string name, std::string value) {
      this->paramMap[name] = value;
  }
  std::string getParam(std::string name) {
    return this->paramMap.at(name);
  }
  const std::map<std::string, std::string>& getParamMap() const {
    return paramMap;
  }
  Operation getOperation() const {
    return operation;
  }

 private:
  std::string exchange;
  std::string instrument;
  std::string serviceName;
  std::string correlationId;
  std::map<std::string, std::string> paramMap;
  std::map<std::string, std::string> credential;
  Operation operation;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_REQUEST_H_
