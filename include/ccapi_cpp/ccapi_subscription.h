#ifndef INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_H_
#define INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_H_
#include <set>
#include <string>

#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
namespace ccapi {
/**
 * A single subscription. A 'Subscription' is used when calling 'Session::subscribe()' or 'Session::subscribeByFix'. Subscription objects are created using
 * Subscription constructors. A correlation id can be used as the unique identifier to tag all data associated with this subscription.
 */
class Subscription CCAPI_FINAL {
 public:
  Subscription() {}
  Subscription(std::string exchange, std::string instrument, std::string field, std::string options = "", std::string correlationId = "",
               std::map<std::string, std::string> credential = {})
      : exchange(exchange), instrument(instrument), field(field), correlationId(correlationId), credential(credential) {
    auto originalInstrumentSet = UtilString::splitToSet(instrument, ",");
    std::copy_if(originalInstrumentSet.begin(), originalInstrumentSet.end(), std::inserter(this->instrumentSet, this->instrumentSet.end()),
                 [](const std::string& value) { return !value.empty(); });
    auto originalFieldSet = UtilString::splitToSet(field, ",");
    std::copy_if(originalFieldSet.begin(), originalFieldSet.end(), std::inserter(this->fieldSet, this->fieldSet.end()),
                 [](const std::string& value) { return !value.empty(); });
    if (field == CCAPI_GENERIC_PUBLIC_SUBSCRIPTION) {
      this->rawOptions = options;
    } else {
      std::vector<std::string> optionList;
      if (!options.empty()) {
        optionList = UtilString::split(options, "&");
      }
      this->optionMap[CCAPI_MARKET_DEPTH_MAX] = CCAPI_MARKET_DEPTH_MAX_DEFAULT;
      this->optionMap[CCAPI_CONFLATE_INTERVAL_MILLISECONDS] = CCAPI_CONFLATE_INTERVAL_MILLISECONDS_DEFAULT;
      this->optionMap[CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS] = CCAPI_CONFLATE_GRACE_PERIOD_MILLISECONDS_DEFAULT;
      this->optionMap[CCAPI_MARKET_DEPTH_RETURN_UPDATE] = CCAPI_MARKET_DEPTH_RETURN_UPDATE_DEFAULT;
      this->optionMap[CCAPI_FETCH_MARKET_DEPTH_INITIAL_SNAPSHOT_DELAY_MILLISECONDS] = CCAPI_FETCH_MARKET_DEPTH_INITIAL_SNAPSHOT_DELAY_MILLISECONDS_DEFAULT;
      this->optionMap[CCAPI_CANDLESTICK_INTERVAL_SECONDS] = CCAPI_CANDLESTICK_INTERVAL_SECONDS_DEFAULT;
      for (const auto& option : optionList) {
        auto optionKeyValue = UtilString::split(option, "=");
        this->optionMap[optionKeyValue.at(0)] = optionKeyValue.at(1);
      }
    }
    std::set<std::string> executionManagementSubscriptionFieldSet = {std::string(CCAPI_EM_ORDER_UPDATE), std::string(CCAPI_EM_PRIVATE_TRADE),
                                                                     std::string(CCAPI_EM_BALANCE_UPDATE), std::string(CCAPI_EM_POSITION_UPDATE)};
    if (field == CCAPI_GENERIC_PUBLIC_SUBSCRIPTION) {
      this->serviceName = CCAPI_MARKET_DATA;
    } else if (field == CCAPI_FIX || field == CCAPI_FIX_MARKET_DATA || field == CCAPI_FIX_EXECUTION_MANAGEMENT) {
      this->serviceName = CCAPI_FIX;
    } else if (std::includes(executionManagementSubscriptionFieldSet.begin(), executionManagementSubscriptionFieldSet.end(), this->fieldSet.begin(),
                             this->fieldSet.end())) {
      this->serviceName = CCAPI_EXECUTION_MANAGEMENT;
    } else if (field == CCAPI_MARKET_DEPTH || field == CCAPI_TRADE || field == CCAPI_AGG_TRADE || field == CCAPI_CANDLESTICK) {
      this->serviceName = CCAPI_MARKET_DATA;
    }
    CCAPI_LOGGER_TRACE("this->serviceName = " + this->serviceName);
    if (this->correlationId.empty()) {
      this->correlationId = UtilString::generateRandomString(CCAPI_CORRELATION_ID_GENERATED_LENGTH);
    }
  }
  std::string toString() const {
    std::map<std::string, std::string> shortCredential;
    for (const auto& x : credential) {
      shortCredential.insert(std::make_pair(x.first, UtilString::firstNCharacter(x.second, CCAPI_CREDENTIAL_DISPLAY_LENGTH)));
    }
    std::string output = "Subscription [exchange = " + exchange + ", marginType = " + marginType + ", instrumentType = " + instrumentType +
                         ", instrument = " + instrument + ", field = " + field + ", optionMap = " + ccapi::toString(optionMap) +
                         ", correlationId = " + correlationId + ", credential = " + ccapi::toString(shortCredential) + ", serviceName = " + serviceName +
                         ", timeSent = " + UtilTime::getISOTimestamp(timeSent) + "]";
    return output;
  }
  const std::string& getCorrelationId() const { return correlationId; }
  const std::string& getExchange() const { return exchange; }
  const std::string& getInstrument() const { return instrument; }
  const std::string& getInstrumentType() const { return instrumentType; }
  const std::string& getField() const { return field; }
  const std::string& getRawOptions() const { return rawOptions; }
  const std::map<std::string, std::string>& getOptionMap() const { return optionMap; }
  const std::map<std::string, std::string>& getCredential() const { return credential; }
  const std::string& getServiceName() const { return serviceName; }
  const std::set<std::string>& getInstrumentSet() const { return instrumentSet; }
  const std::set<std::string>& getFieldSet() const { return fieldSet; }
  const std::string getSerializedOptions() const {
    std::string output;
    if (this->rawOptions.empty()) {
      int i = 0;
      for (const auto& option : this->optionMap) {
        output += option.first;
        output += "=";
        output += option.second;
        if (i < this->optionMap.size() - 1) {
          output += "&";
        }
        ++i;
      }
    } else {
      output = this->rawOptions;
    }
    return output;
  }
  const std::string getSerializedCredential() const { return ccapi::toString(this->credential); }
  // 'getTimeSent' only works in C++. For other languages, please use 'getTimeSentISO'.
  TimePoint getTimeSent() const { return timeSent; }
  std::string getTimeSentISO() const { return UtilTime::getISOTimestamp(timeSent); }
  std::pair<long long, long long> getTimeSentPair() const { return UtilTime::divide(timeSent); }
  const std::string& getMarginType() const { return marginType; }
  void setTimeSent(TimePoint timeSent) { this->timeSent = timeSent; }
  void setInstrumentType(const std::string& instrumentType) { this->instrumentType = instrumentType; }
  void setMarginType(const std::string& marginType) { this->marginType = marginType; }
  enum class Status {
    UNKNOWN,
    SUBSCRIBING,
    SUBSCRIBED,
    UNSUBSCRIBING,
    UNSUBSCRIBED,
  };
  static std::string statusToString(Status status) {
    std::string output;
    switch (status) {
      case Status::UNKNOWN:
        output = "UNKNOWN";
        break;
      case Status::SUBSCRIBING:
        output = "SUBSCRIBING";
        break;
      case Status::SUBSCRIBED:
        output = "SUBSCRIBED";
        break;
      case Status::UNSUBSCRIBING:
        output = "UNSUBSCRIBING";
        break;
      case Status::UNSUBSCRIBED:
        output = "UNSUBSCRIBED";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif
  std::string exchange;
  std::string marginType;
  std::string instrumentType;
  std::string instrument;
  std::string field;
  std::string rawOptions;
  std::map<std::string, std::string> optionMap;
  std::string correlationId;
  std::map<std::string, std::string> credential;
  std::string serviceName;
  std::set<std::string> instrumentSet;
  std::set<std::string> fieldSet;
  TimePoint timeSent{std::chrono::seconds{0}};
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_SUBSCRIPTION_H_
