#ifndef EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_USDT_PERPETUAL_H
#define EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_USDT_PERPETUAL_H
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_PERPETUAL

#include "ccapi_cpp/service/ccapi_execution_management_service.h"

namespace ccapi {
    class ExecutionManagementServiceHuobiUsdtPerpetual CCAPI_FINAL : public ExecutionManagementService {
    public:
        ExecutionManagementServiceHuobiUsdtPerpetual(std::function<void(Event &event)> eventHandler,
                                                     SessionOptions sessionOptions,
                                                     SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
                : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
            CCAPI_LOGGER_FUNCTION_ENTER;
            this->name = CCAPI_EXCHANGE_NAME_HUOBI_USDT_PERPETUAL;
            this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
            this->setHostFromUrl(this->baseUrlRest);
            this->apiKeyName = CCAPI_HUOBI_USDT_PERPETUAL_API_KEY;
            this->apiSecretName = CCAPI_HUOBI_USDT_PERPETUAL_API_SECRET;
            this->setupCredential({this->apiKeyName, this->apiSecretName});
            this->createOrderTarget = "/linear-swap-api/v1/swap_cross_order";
            this->cancelOrderTarget = "/linear-swap-api/v1/swap_cross_cancel";
//            this->cancelOrderByClientOrderIdTarget = "/linear-swap-api/v1/swap_cancel";
            this->getOrderTarget = "/linear-swap-api/v1/swap_cross_order_info";
//            this->getOrderByClientOrderIdTarget = "/v1/order/orders/getClientOrder";
//            this->getOpenOrdersTarget = "/v1/order/openOrders";
            this->orderStatusOpenSet = {"created", "submitted", "partial-filled", "canceling"};
            CCAPI_LOGGER_FUNCTION_EXIT;
        }

    protected:
        void signRequest(http::request<http::string_body> &req, const std::string &path,
                         const std::map<std::string, std::string> &queryParamMap,
                         const std::map<std::string, std::string> &credential) {
            std::string preSignedText;
            preSignedText += std::string(req.method_string());
            preSignedText += "\n";
            preSignedText += "api.hbdm.com";
//            preSignedText += req.base().at(http::field::host).to_string();
            preSignedText += "\n";
            preSignedText += path;
            preSignedText += "\n";
            std::string queryString;
            int i = 0;
            for (const auto &kv : queryParamMap) {
                queryString += kv.first;
                queryString += "=";
                queryString += kv.second;
                if (i < queryParamMap.size() - 1) {
                    queryString += "&";
                }
                i++;
            }
            preSignedText += queryString;

            std::cout << "PreSignedText:" << std::endl;
            std::cout << preSignedText << std::endl;

            auto apiSecret = mapGetWithDefault(credential, this->apiSecretName, {});
            auto signature = UtilAlgorithm::base64Encode(
                    Hmac::hmac(Hmac::ShaVersion::SHA256, preSignedText, apiSecret));
            queryString += "&Signature=";
            queryString += Url::urlEncode(signature);
            req.target(path + "?" + queryString);

            std::cout << "QueryString:" << std::endl;
            std::cout << queryString << std::endl;

//            std::cout << "REQUEST (Target):" << std::endl;
//            std::cout << req.target() << std::endl;
//            std::cout << "REQUEST (Header, Body):" << std::endl;
//            std::cout << req << std::endl;
        }

        void appendParam(rj::Document &document, rj::Document::AllocatorType &allocator,
                         const std::map<std::string, std::string> &param,
                         const std::map<std::string, std::string> regularizationMap = {}) {
            for (const auto &kv : param) {
                auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first)
                                                                                       : kv.first;
                auto value = kv.second;
                if (key == "direction") {
                    value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
                }
                if (key == "offset") {
                    value = value == "OPEN" ? "open" : "close";
                }
                if (key == "volume") {
                    document.AddMember(rj::Value(key.c_str(), allocator).Move(), std::stoi(value),
                                       allocator);
                } else {
                    document.AddMember(rj::Value(key.c_str(), allocator).Move(),
                                       rj::Value(value.c_str(), allocator).Move(),
                                       allocator);
                }
            }

            // Add order type
            std::string order_price_type_key = "order_price_type";
            std::string order_price_type_value = "limit";
            document.AddMember(rj::Value(order_price_type_key.c_str(), allocator).Move(),
                               rj::Value(order_price_type_value.c_str(), allocator).Move(),
                               allocator);
            // Add lever_rate
            std::string lever_rate_key = "lever_rate";
            std::string lever_rate_value = "5";
            document.AddMember(rj::Value(lever_rate_key.c_str(), allocator).Move(), std::stoi(lever_rate_value),
                               allocator);
        }

        void
        appendParam(std::map<std::string, std::string> &queryParamMap, const std::map<std::string, std::string> &param,
                    const std::map<std::string, std::string> regularizationMap = {}) {
            for (const auto &kv : param) {
                queryParamMap.insert(std::make_pair(
                        regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first)
                                                                                    : kv.first,
                        Url::urlEncode(kv.second)));
            }
        }

        void
        appendSymbolId(rj::Document &document, rj::Document::AllocatorType &allocator, const std::string symbolId) {
            document.AddMember("contract_code", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        }

        void appendSymbolId(std::map<std::string, std::string> &queryParamMap, const std::string symbolId) {
            queryParamMap.insert(std::make_pair("contract_code", Url::urlEncode(symbolId)));
        }

        void convertReq(const Request &request, const TimePoint &now, http::request<http::string_body> &req,
                        const std::map<std::string, std::string> &credential, const std::string &symbolId,
                        const Request::Operation operation) override {
            req.set(beast::http::field::content_type, "application/json");
            auto apiKey = mapGetWithDefault(credential, this->apiKeyName, {});
            std::map<std::string, std::string> queryParamMap;
            queryParamMap.insert(std::make_pair("AccessKeyId", apiKey));
            queryParamMap.insert(std::make_pair("SignatureMethod", "HmacSHA256"));
            queryParamMap.insert(std::make_pair("SignatureVersion", "2"));
            std::string timestamp = UtilTime::getISOTimestamp<std::chrono::seconds>(now, "%FT%T");
            queryParamMap.insert(std::make_pair("Timestamp", Url::urlEncode(timestamp)));
            switch (operation) {
                case Request::Operation::CREATE_ORDER: {
                    req.method(http::verb::post);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    rj::Document document;
                    document.SetObject();
                    rj::Document::AllocatorType &allocator = document.GetAllocator();
                    this->appendParam(document, allocator, param, {
                            {CCAPI_EM_ORDER_SIDE,        "direction"},
                            {CCAPI_EM_ORDER_QUANTITY,    "volume"},
                            {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                            {CCAPI_EM_CLIENT_ORDER_ID,   "client-order-id"},
                            {CCAPI_EM_ORDER_ID,          "order_id"},
                            {CCAPI_EM_ORDER_OFFSET,      "offset"}
                    });
                    this->appendSymbolId(document, allocator, symbolId);
                    rj::StringBuffer stringBuffer;
                    rj::Writer<rj::StringBuffer> writer(stringBuffer);
                    document.Accept(writer);
                    auto body = stringBuffer.GetString();
                    req.body() = body;
                    req.prepare_payload();
                    this->signRequest(req, this->createOrderTarget, queryParamMap, credential);
                }
                    break;
                case Request::Operation::CANCEL_ORDER: {
                    req.method(http::verb::post);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
                    std::string id = shouldUseOrderId ? param.at(CCAPI_EM_ORDER_ID)
                                                      : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ?
                                                        "client:" + param.at(CCAPI_EM_CLIENT_ORDER_ID)
                                                                                                            : "";
                    auto target = shouldUseOrderId ? std::regex_replace(this->cancelOrderTarget,
                                                                        std::regex("\\{order\\-id\\}"), id)
                                                   : this->cancelOrderByClientOrderIdTarget;
                    if (!shouldUseOrderId) {
                        rj::Document document;
                        document.SetObject();
                        rj::Document::AllocatorType &allocator = document.GetAllocator();
                        this->appendParam(document, allocator, param, {
                                {CCAPI_EM_CLIENT_ORDER_ID, "client-order-id"}
                        });
                        rj::StringBuffer stringBuffer;
                        rj::Writer<rj::StringBuffer> writer(stringBuffer);
                        document.Accept(writer);
                        auto body = stringBuffer.GetString();
                        req.body() = body;
                        req.prepare_payload();
                    }
                    this->signRequest(req, target, queryParamMap, credential);
                }
                    break;
                case Request::Operation::GET_ORDER: {
                    req.method(http::verb::get);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    auto shouldUseOrderId = param.find(CCAPI_EM_ORDER_ID) != param.end();
                    std::string id = shouldUseOrderId ? param.at(CCAPI_EM_ORDER_ID)
                                                      : param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(
                                    CCAPI_EM_CLIENT_ORDER_ID)
                                                                                                            : "";
                    auto target = shouldUseOrderId ? std::regex_replace(this->getOrderTarget,
                                                                        std::regex("\\{order\\-id\\}"), id)
                                                   : this->getOrderByClientOrderIdTarget;
                    req.target(target);
                    if (!shouldUseOrderId) {
                        this->appendParam(queryParamMap, param, {
                                {CCAPI_EM_ACCOUNT_ID, "order_id"}
                        });
                        queryParamMap.insert(std::make_pair("client-order-id", Url::urlEncode(id)));
                    }
                    this->signRequest(req, target, queryParamMap, credential);
                }
                    break;
                case Request::Operation::GET_OPEN_ORDERS: {
                    req.method(http::verb::get);
                    this->appendParam(queryParamMap, {}, {
                            {CCAPI_EM_ACCOUNT_ID, "order_id"}
                    });
                    if (!symbolId.empty()) {
                        this->appendSymbolId(queryParamMap, symbolId);
                    }
                    this->signRequest(req, this->getOpenOrdersTarget, queryParamMap, credential);
                }
                    break;
                default:
                    CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
            }
        }

        std::vector<Element>
        extractOrderInfo(const Request::Operation operation, const rj::Document &document) override {
            // Print order response
            rj::StringBuffer buffer;
            rj::Writer<rj::StringBuffer> writer(buffer);
            rj::Document d = rj::Document();
            d.CopyFrom(document, d.GetAllocator());
            d.Accept(writer);

            std::cout << "RESPONSE:" << std::endl;
            std::cout << buffer.GetString() << std::endl;

            const std::map<std::string, std::pair<std::string, JsonDataType> > &extractionFieldNameMap = {
                    {CCAPI_EM_ORDER_ID,                                     std::make_pair("id",
                                                                                           JsonDataType::INTEGER)},
                    {CCAPI_EM_CLIENT_ORDER_ID,                              std::make_pair("client-order-id",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_SIDE,                                   std::make_pair("type",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_QUANTITY,                               std::make_pair("amount",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_LIMIT_PRICE,                            std::make_pair("price",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY,             std::make_pair("filled-amount",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_CUMULATIVE_FILLED_PRICE_TIMES_QUANTITY, std::make_pair("filled-cash-amount",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_STATUS,                                 std::make_pair("state",
                                                                                           JsonDataType::STRING)},
                    {CCAPI_EM_ORDER_INSTRUMENT,                             std::make_pair("symbol",
                                                                                           JsonDataType::STRING)}
            };
            std::vector<Element> elementList;
            const rj::Value &data = document["data"];
            if (operation == Request::Operation::CREATE_ORDER || operation == Request::Operation::CANCEL_ORDER) {
                Element element;
                element.insert(CCAPI_EM_ORDER_ID, std::string(data.GetString()));
                elementList.emplace_back(element);
            } else if (data.IsObject()) {
                elementList.emplace_back(ExecutionManagementService::extractOrderInfo(data, extractionFieldNameMap));
            } else {
                for (const auto &x : data.GetArray()) {
                    elementList.emplace_back(ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap));
                }
            }
            return elementList;
        }

        std::string cancelOrderByClientOrderIdTarget;
        std::string getOrderByClientOrderIdTarget;
#ifdef GTEST_INCLUDE_GTEST_GTEST_H_

        public:
  using ExecutionManagementService::convertRequest;
  using ExecutionManagementService::processSuccessfulTextMessage;
  FRIEND_TEST(ExecutionManagementServiceHuobiUsdtPerpetualTest, signRequest);
#endif
    };
} /* namespace ccapi */
#endif
#endif
#endif //EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_HUOBI_USDT_PERPETUAL_H
