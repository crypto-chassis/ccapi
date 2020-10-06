#ifndef INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_H_
#include "ccapi_cpp/ccapi_macro.h"
#include <string>
#include <set>
#include "ccapi_cpp/ccapi_correlationId.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
class Subscription final {
 public:
  Subscription(std::string topic, std::string fields, std::string options = "", CorrelationId correlationId =
                   CorrelationId())
      : correlationId(correlationId) {
    std::string trimmedTopic(topic);
    if (trimmedTopic.rfind("/", 0) == 0) {
      trimmedTopic = trimmedTopic.substr(1);
    }
    auto splittedTopic = UtilString::split(trimmedTopic, "/");
    this->exchange = splittedTopic.at(0);
    this->instrument = splittedTopic.at(1);
    std::vector<std::string> fieldList = UtilString::split(fields, ",");
    this->fieldSet = std::set<std::string>(fieldList.begin(), fieldList.end());
    std::vector<std::string> optionList;
    if (!options.empty()) {
      optionList = UtilString::split(options, "&");
    }
    this->optionMap[CCAPI_EXCHANGE_NAME_MARKET_DEPTH_MAX] =
    CCAPI_EXCHANGE_VALUE_MARKET_DEPTH_MAX_DEFAULT;
    this->optionMap[CCAPI_EXCHANGE_NAME_CONFLATE_INTERVAL_MILLISECONDS] =
    CCAPI_EXCHANGE_VALUE_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
    this->optionMap[CCAPI_EXCHANGE_NAME_CONFLATE_GRACE_PERIOD_MILLISECONDS] =
    CCAPI_EXCHANGE_VALUE_CONFLATE_GRACE_PERIOD_MILLISECONDS_DEFAULT;
    for (const auto & option : optionList) {
      auto optionKeyValue = UtilString::split(option, "=");
      this->optionMap[optionKeyValue.at(0)] = optionKeyValue.at(1);
    }
  }
  std::string toString() const {
    std::string output = "Subscription [exchange = " + exchange + ", instrument = " + instrument + ", fieldSet = "
        + ccapi::toString(fieldSet) + ", optionMap = " + ccapi::toString(optionMap) + ", correlationId = "
        + correlationId.toString() + "]";
    return output;
  }
  const CorrelationId& getCorrelationId() const {
    return correlationId;
  }
  const std::string& getExchange() const {
    return exchange;
  }
  const std::string& getInstrument() const {
    return instrument;
  }
  const std::set<std::string>& getFieldSet() const {
    return fieldSet;
  }
  const std::map<std::string, std::string>& getOptionMap() const {
    return optionMap;
  }

 private:
  std::string exchange;
  std::string instrument;
  std::set<std::string> fieldSet;
  std::map<std::string, std::string> optionMap;
  CorrelationId correlationId;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_H_
