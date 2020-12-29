#ifndef INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_MESSAGE_H_
#define INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_MESSAGE_H_
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_util_private.h"
// #include "ccapi_message.h"
namespace ccapi {
class MarketDataMessage CCAPI_FINAL {
 public:
  enum class Type {
    UNKNOWN,
    MARKET_DATA_EVENTS
  };
  enum class RecapType {
    UNKNOWN,
    NONE,  // normal data tick; not a recap
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
  enum class DataType {
    BID = 0,
    ASK = 1,
    TRADE = 2
  };
  static std::string dataTypeToString(DataType dataType) {
    std::string output;
    switch (dataType) {
      case DataType::BID:
        output = "BID";
        break;
      case DataType::ASK:
        output = "ASK";
        break;
      case DataType::TRADE:
        output = "TRADE";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  enum class DataFieldType {
    PRICE = 0,
    SIZE = 1,
    TRADE_ID = 2,
    IS_BUYER_MAKER = 3
  };
  static std::string dataFieldTypeToString(DataFieldType dataFieldType) {
    std::string output;
    switch (dataFieldType) {
      case DataFieldType::PRICE:
        output = "PRICE";
        break;
      case DataFieldType::SIZE:
        output = "SIZE";
        break;
      case DataFieldType::TRADE_ID:
        output = "TRADE_ID";
        break;
      case DataFieldType::IS_BUYER_MAKER:
        output = "IS_BUYER_MAKER";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  typedef std::map<DataFieldType, std::string> TypeForDataPoint;
  typedef std::map<DataType, std::vector<std::map<DataFieldType, std::string> > > TypeForData;
  static std::string dataToString(const TypeForData& data) {
    std::string output1 = "{";
    auto size1 = data.size();
    auto i1 = 0;
    for (const auto& elem1 : data) {
      output1 += dataTypeToString(elem1.first);
      output1 += "=";
      std::string output2 = "[ ";
      auto size2 = elem1.second.size();
      auto i2 = 0;
      for (const auto& elem2 : elem1.second) {
        std::string output3 = "{";
        auto size3 = elem2.size();
        auto i3 = 0;
        for (const auto& elem3 : elem2) {
          output3 += dataFieldTypeToString(elem3.first);
          output3 += "=";
          output3 += ccapi::toString(elem3.second);
          if (i3 < size3 - 1) {
            output3 += ", ";
          }
          ++i3;
        }
        output3 += "}";
        output2 += output3;
        if (i2 < size2 - 1) {
          output2 += ", ";
        }
        ++i2;
      }
      output2 += " ]";
      output1 += output2;
      if (i1 < size1 - 1) {
        output1 += ", ";
      }
      ++i1;
    }
    output1 += "}";
    return output1;
  }
  static std::string typeToString(Type type) {
    std::string output;
    switch (type) {
      case Type::UNKNOWN:
        output = "UNKNOWN";
        break;
      case Type::MARKET_DATA_EVENTS:
        output = "MARKET_DATA_EVENTS";
        break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    return output;
  }
  std::string toString() const {
    std::string output = "MarketDataMessage [type = " + typeToString(type) + ", recapType = "
        + recapTypeToString(recapType) + ", tp = " + ccapi::toString(tp) + ", exchangeSubscriptionId = "
        + exchangeSubscriptionId + ", data = " + dataToString(data) + "]";
    return output;
  }
  Type type{Type::UNKNOWN};
  RecapType recapType{RecapType::UNKNOWN};
  TimePoint tp{std::chrono::seconds{0}};
  std::string exchangeSubscriptionId;
  TypeForData data;
};
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_CCAPI_MARKET_DATA_MESSAGE_H_
