#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
#ifdef ENABLE_SERVICE_EXECUTION_MANAGEMENT
#if defined(ENABLE_EXCHANGE_BINANCE_US) || defined(ENABLE_EXCHANGE_BINANCE) || defined(ENABLE_EXCHANGE_BINANCE_FUTURES)
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
class ExecutionManagementServiceBinanceBase : public ExecutionManagementService {
 public:
  ExecutionManagementServiceBinanceBase(std::function<void(Event& event)> eventHandler, SessionOptions sessionOptions,
                                      SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
  }
  virtual ~ExecutionManagementServiceBinanceBase() {
  }

 protected:
  void signRequest(std::string& queryString, const std::map<std::string, std::string>& param, const TimePoint& now, const std::map<std::string, std::string>& credential) {
    if (param.find("timestamp") == param.end()) {
      queryString += "timestamp=";
      queryString += std::to_string(std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch()).count());
      queryString += "&";
    }
    queryString.pop_back();
    auto apiSecret = mapGetWithDefault(credential, this->apiSecretName, {});
    std::string signature = UtilAlgorithm::hmacHex(apiSecret, queryString);
    queryString += "&signature=";
    queryString += signature;
  }
  void appendParam(std::string& queryString, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> regularizationMap = {}) {
    for (const auto& kv : param) {
      queryString += regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
      queryString += "=";
      queryString += kv.second;
      queryString += "&";
    }
  }
  void appendSymbolId(std::string& queryString, const std::string symbolId) {
    queryString += "symbol=";
    queryString += symbolId;
    queryString += "&";
  }
  void convertReq(const Request& request, const TimePoint& now, http::request<http::string_body>& req, const std::map<std::string, std::string>& credential, const std::string& symbolId, const Request::Operation operation) override {
    auto apiKey = mapGetWithDefault(credential, this->apiKeyName, {});
    req.set("X-MBX-APIKEY", apiKey);
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        req.method(http::verb::post);
        std::string queryString;
        const std::map<std::string, std::string>& param = request.getParamList().at(0);
        this->appendParam(queryString, param, {
            {CCAPI_EM_ORDER_SIDE , "side"},
            {CCAPI_EM_ORDER_QUANTITY , "quantity"},
            {CCAPI_EM_ORDER_LIMIT_PRICE , "price"},
            {CCAPI_EM_CLIENT_ORDER_ID , "newClientOrderId"}
        });
        this->appendSymbolId(queryString, symbolId);
        if (param.find("type") == param.end()) {
          queryString += "type=LIMIT&";
          if (param.find("timeInForce") == param.end()) {
            queryString += "timeInForce=GTC&";
          }
        }
        this->signRequest(queryString, param, now, credential);
        req.target(std::string(this->createOrderTarget) + "?" + queryString);
      }
      break;
      case Request::Operation::CANCEL_ORDER:
      {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string>& param = request.getParamList().at(0);
        this->appendParam(queryString, param, {
            {CCAPI_EM_ORDER_ID , "orderId"},
            {CCAPI_EM_CLIENT_ORDER_ID , "origClientOrderId"}
        });
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->cancelOrderTarget + "?" + queryString);
      }
      break;
      case Request::Operation::GET_ORDER:
      {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string>& param = request.getParamList().at(0);
        this->appendParam(queryString, param, {
            {CCAPI_EM_ORDER_ID , "orderId"},
            {CCAPI_EM_CLIENT_ORDER_ID , "origClientOrderId"}
        });
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->getOrderTarget + "?" + queryString);
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        req.method(http::verb::get);
        std::string queryString;
        const std::map<std::string, std::string>& param = {};
        this->appendParam(queryString, param);
        if (!symbolId.empty()) {
          this->appendSymbolId(queryString, symbolId);
        }
        this->signRequest(queryString, param, now, credential);
        req.target(this->getOpenOrdersTarget + "?" + queryString);
      }
      break;
      case Request::Operation::CANCEL_OPEN_ORDERS:
      {
        req.method(http::verb::delete_);
        std::string queryString;
        const std::map<std::string, std::string>& param = {};
        this->appendParam(queryString, param);
        this->appendSymbolId(queryString, symbolId);
        this->signRequest(queryString, param, now, credential);
        req.target(this->cancelOpenOrdersTarget + "?" + queryString);
      }
      break;
      default:
      CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }
  Element extractOrderInfo(const rj::Value& x) {
    return ExecutionManagementService::extractOrderInfo(x, {
      {CCAPI_EM_ORDER_ID, std::make_pair("orderId", JsonDataType::INTEGER)},
      {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("clientOrderId", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origQty", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("executedQty", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair(this->isFutures ? "cumQuote" : "cummulativeQuoteQty", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_STATUS, std::make_pair("status", JsonDataType::STRING)},
      {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("symbol", JsonDataType::STRING)}
    });
  }
  std::vector<Message> processSuccessfulTextMessage(const Request& request, const std::string& textMessage, const TimePoint& timeReceived) override {
    rj::Document document;
    document.Parse(textMessage.c_str());
    Message message;
    message.setTimeReceived(timeReceived);
    message.setCorrelationIdList({request.getCorrelationId()});
    std::vector<Element> elementList;
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::CREATE_ORDER:
      {
        message.setType(Message::Type::CREATE_ORDER);
        elementList.emplace_back(this->extractOrderInfo(document));
      }
      break;
      case Request::Operation::CANCEL_ORDER:
      {
        message.setType(Message::Type::CANCEL_ORDER);
        elementList.emplace_back(this->extractOrderInfo(document));
      }
      break;
      case Request::Operation::GET_ORDER:
      {
        message.setType(Message::Type::GET_ORDER);
        elementList.emplace_back(this->extractOrderInfo(document));
      }
      break;
      case Request::Operation::GET_OPEN_ORDERS:
      {
        message.setType(Message::Type::GET_OPEN_ORDERS);
        for (const auto& x : document.GetArray()) {
          elementList.emplace_back(this->extractOrderInfo(x));
        }
      }
      break;
      case Request::Operation::CANCEL_OPEN_ORDERS:
      {
        message.setType(Message::Type::CANCEL_OPEN_ORDERS);
        for (const auto& x : document.GetArray()) {
          elementList.emplace_back(this->extractOrderInfo(x));
        }
      }
      break;
      default:
      CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
    message.setElementList(elementList);
    std::vector<Message> messageList;
    messageList.push_back(std::move(message));
    return messageList;
  }
  std::string convertOrderStatus(const std::string& status) override {
    return this->orderStatusOpenSet.find(status) != this->orderStatusOpenSet.end() ? CCAPI_EM_ORDER_STATUS_OPEN
    : this->orderStatusClosedSet.find(status) != this->orderStatusClosedSet.end() ? CCAPI_EM_ORDER_STATUS_CLOSED
    : CCAPI_EM_ORDER_STATUS_UNKNOWN;
  }
  std::set<std::string> orderStatusOpenSet = {"NEW", "PARTIALLY_FILLED"};
  std::set<std::string> orderStatusClosedSet = {"FILLED", "CANCELED", "REJECTED", "EXPIRED"};
  std::string createOrderTarget;
  std::string cancelOrderTarget;
  std::string getOrderTarget;
  std::string getOpenOrdersTarget;
  std::string cancelOpenOrdersTarget;
  bool isFutures{};
};
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_BINANCE_BASE_H_
