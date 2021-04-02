#ifndef EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_PERPETUAL_SWAP_H
#define EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_PERPETUAL_SWAP_H
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX_PERPETUAL_SWAP
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
    class ExecutionManagementServiceOkexPerpetualSwap CCAPI_FINAL : public ExecutionManagementService {
    public:
        ExecutionManagementServiceOkexPerpetualSwap(std::function<void(Event &event)> eventHandler,
                                                SessionOptions sessionOptions,
                                                SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
                : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
            CCAPI_LOGGER_FUNCTION_ENTER;
            this->name = CCAPI_EXCHANGE_NAME_OKEX_PERPETUAL_SWAP;
            this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
            this->setHostFromUrl(this->baseUrlRest);
            this->apiKeyName = CCAPI_OKEX_PERPETUAL_SWAP_API_KEY;
            this->apiSecretName = CCAPI_OKEX_PERPETUAL_SWAP_API_SECRET;
            this->apiPassphraseName = CCAPI_OKEX_PERPETUAL_SWAP_API_PASSPHRASE;
            this->setupCredential({this->apiKeyName, this->apiSecretName, this->apiPassphraseName});
            this->createOrderTarget = "/api/swap/v3/order";
            this->cancelOrderTarget = "/api/swap/v3/cancel_order/<instrument_id>/<order_id>";
            this->getOrderTarget = "/api/swap/v3/orders/<instrument_id>/<order_id>";
            this->getOpenOrdersTarget = "/api/swap/v3/orders/<instrument_id>";
            this->orderStatusOpenSet = {"0", "1", "3", "4", "6"};
            CCAPI_LOGGER_FUNCTION_EXIT;
        }

    protected:
      void signRequest(http::request<http::string_body>& req, const std::string& body, const std::map<std::string, std::string>& credential) {
        auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
        auto preSignedText = req.base().at("OK-ACCESS-TIMESTAMP").to_string();
        preSignedText += std::string(req.method_string());
        preSignedText += req.target().to_string();
        preSignedText += body;
        auto signature = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, preSignedText));
        req.set("OK-ACCESS-SIGN", signature);
        req.body() = body;
        req.prepare_payload();
      }
      void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> regularizationMap = {}) {
        for (const auto& kv : param) {
          auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
          auto value = kv.second;
          if (key == "side") {
            value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
          }
          document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
        }
      }
      void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string& symbolId) {
        document.AddMember("instrument_id", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
      }
      void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
        req.set(beast::http::field::content_type, "application/json");
        auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
        req.set("OK-ACCESS-KEY", apiKey);
        std::string millisecondStr = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
        req.set("OK-ACCESS-TIMESTAMP", millisecondStr.substr(0, millisecondStr.length() - 3) + "." + millisecondStr.substr(millisecondStr.length() - 3));
        auto apiPassphrase = mapGetWithDefault(credential, this->apiPassphraseName, {});
        req.set("OK-ACCESS-PASSPHRASE", apiPassphrase);
        switch (operation) {
          case Request::Operation::CREATE_ORDER:
          {
            req.method(http::verb::post);
            const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
            req.target(this->createOrderTarget);
            rj::Document document;
            document.SetObject();
            rj::Document::AllocatorType& allocator = document.GetAllocator();
            this->appendParam(document, allocator, param, {
                {CCAPI_EM_ORDER_QUANTITY , "size"},
                {CCAPI_EM_ORDER_LIMIT_PRICE , "price"},
                {CCAPI_EM_CLIENT_ORDER_ID , "client_oid"}
            });
            if (!symbolId.empty()) {
              this->appendSymbolId(document, allocator, symbolId);
            }            
            rj::StringBuffer stringBuffer;
            rj::Writer<rj::StringBuffer> writer(stringBuffer);
            document.Accept(writer);
            auto body = stringBuffer.GetString();
            this->signRequest(req, body, credential);
          }
          break;
          // case Request::Operation::CANCEL_ORDER:
          // {
          //   req.method(http::verb::delete_);
          //   const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
          //   std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID)
          //       : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
          //       : "";
          //   auto target = std::regex_replace(this->cancelOrderTarget, std::regex("\\{id\\}"), id);
          //   if (!symbolId.empty()) {
          //     target += "?product_id=";
          //     target += symbolId;
          //   }
          //   req.target(target);
          //   this->signRequest(req, "", credential);
          // }
          // break;
          // case Request::Operation::GET_ORDER:
          // {
          //   req.method(http::verb::get);
          //   const std::map<std::string, std::string> param = request.getFirstParamWithDefault();
          //   std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID)
          //       : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
          //       : "";
          //   auto target = std::regex_replace(this->getOrderTarget, std::regex("\\{id\\}"), id);
          //   req.target(target);
          //   this->signRequest(req, "", credential);
          // }
          // break;
          // case Request::Operation::GET_OPEN_ORDERS:
          // {
          //   req.method(http::verb::get);
          //   auto target = this->getOpenOrdersTarget;
          //   if (!symbolId.empty()) {
          //     target += "?product_id=";
          //     target += symbolId;
          //   }
          //   req.target(target);
          //   this->signRequest(req, "", credential);
          // }
          // break;
          // case Request::Operation::CANCEL_OPEN_ORDERS:
          // {
          //   req.method(http::verb::delete_);
          //   auto target = this->cancelOpenOrdersTarget;
          //   if (!symbolId.empty()) {
          //     target += "?product_id=";
          //     target += symbolId;
          //   }
          //   req.target(target);
          //   this->signRequest(req, "", credential);
          // }
          // break;
          default:
          CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
        }
      }
      std::vector<Element> extractOrderInfoFromRequest(const Request& request, const Request::Operation operation, const rj::Document& document) override {
        const std::map<std::string, std::pair<std::string, JsonDataType> >& extractionFieldNameMap = {
          {CCAPI_EM_ORDER_ID, std::make_pair("order_id", JsonDataType::STRING)},
          {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("client_oid", JsonDataType::STRING)},
          {CCAPI_EM_ORDER_SIDE, std::make_pair("side", JsonDataType::STRING)},
          {CCAPI_EM_ORDER_QUANTITY, std::make_pair("size", JsonDataType::STRING)},
          {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("price", JsonDataType::STRING)},
          {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("filled_qty", JsonDataType::STRING)},
          {CCAPI_EM_ORDER_STATUS, std::make_pair("state", JsonDataType::STRING)},
          {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("instrument_id", JsonDataType::STRING)}
        };
        std::vector<Element> elementList;
        if (operation == Request::Operation::CANCEL_ORDER) {
          Element element;
          element.insert(CCAPI_EM_ORDER_ID, document.GetString());
          elementList.emplace_back(element);
        } else if (operation == Request::Operation::CANCEL_OPEN_ORDERS) {
          for (const auto& x : document.GetArray()) {
            Element element;
            element.insert(CCAPI_EM_ORDER_ID, x.GetString());
            elementList.emplace_back(element);
          }
        } else {
          if (document.IsObject()) {
            elementList.emplace_back(ExecutionManagementService::extractOrderInfo(document, extractionFieldNameMap));
          } else {
            for (const auto& x : document.GetArray()) {
              elementList.emplace_back(ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap));
            }
          }
        }
        return elementList;
      }
        std::string apiPassphraseName;

// #ifdef GTEST_INCLUDE_GTEST_GTEST_H_
//
//         public:
//          using ExecutionManagementService::convertRequest;
//          using ExecutionManagementService::processSuccessfulTextMessage;
//          FRIEND_TEST(ExecutionManagementServiceOkexPerpetualSwapTest, signRequest);
// #endif
    };
} /* namespace ccapi */
#endif
#endif
#endif //EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_PERPETUAL_SWAP_H
