#ifndef EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_PERPETUAL_SWAP_H
#define EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_PERPETUAL_SWAP_H
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_OKEX_PERPETUAL_SWAP
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
namespace ccapi {
    class ExecutionManagementServiceOkexPerpetual CCAPI_FINAL : public ExecutionManagementService {
    public:
        ExecutionManagementServiceOkexPerpetual(std::function<void(Event &event)> eventHandler,
                                                SessionOptions sessionOptions,
                                                SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr)
                : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
            CCAPI_LOGGER_FUNCTION_ENTER;
            this->name = CCAPI_EXCHANGE_NAME_OKEX_PERPETUAL;
            this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
            this->setHostFromUrl(this->baseUrlRest);
            this->apiKeyName = CCAPI_OKEX_PERPETUAL_API_KEY;
            this->apiSecretName = CCAPI_OKEX_PERPETUAL_API_SECRET;
            this->apiPassphraseName = CCAPI_COINBASE_API_PASSPHRASE;
            this->setupCredential({this->apiKeyName, this->apiSecretName});
            this->createOrderTarget = "/api/swap/v3/order";
            this->cancelOrderTarget = "/api/swap/v3/cancel_order";
            // this->editOrderTarget = "/api/swap/v3/amend_order";
//    this->cancelOrderByClientOrderIdTarget = "/v1/order/orders/submitCancelClientOrder";
            this->getOrderTarget = "/api/swap/v3/orders";
//    this->getOrderByClientOrderIdTarget = "/v1/order/orders/getClientOrder";
            this->getOpenOrdersTarget = "/api/swap/v3/orders";
            this->orderStatusOpenSet = {"created", "submitted", "partial-filled", "canceling"};
            CCAPI_LOGGER_FUNCTION_EXIT;
        }

    protected:
        void signRequest(http::request<http::string_body> &req, const TimePoint &now, const std::string &body,
                         const std::map<std::string, std::string> &credential) {
            auto apiSecret = mapGetWithDefault(credential, this->apiSecretName, {});
//            auto apiKey = mapGetWithDefault(credential, this->apiKeyName, {});
//            std::cout << "TIMESTAMP:" << std::endl;
            std::string ts = getISOCurrentTimestamp<std::chrono::milliseconds>();
//            std::cout << ts << std::endl;

            std::string method(req.method_string());
            std::string path(req.target());
            std::string body_str(req.body());

            std::string data = ts + method + path + body_str;
            std::string sign = UtilAlgorithm::base64Encode(Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, data));

            req.set("OK-ACCESS-SIGN", sign);
            req.set("OK-ACCESS-TIMESTAMP", ts);
            req.prepare_payload();

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

                if (key == "type") {
                    auto pos = param.find("OFFSET");
                    if (pos == param.end()) {
                        std::cout << "Error in append param" << std::endl;
                    } else {
                        std::string offset = pos->second;
                        if (offset == "OPEN") {
                            value = value == CCAPI_EM_ORDER_SIDE_BUY ? "1" : "2";
                        } else if (offset == "CLOSE") {
                            value = value == CCAPI_EM_ORDER_SIDE_BUY ? "3" : "4";
                        }
                    }
                }
                if (key == "offset") {
                    continue;
                }
                document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(),
                                   allocator);
            }
        }

        void
        appendSymbolId(rj::Document &document, rj::Document::AllocatorType &allocator, const std::string symbolId) {
            document.AddMember("instrument_id", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        }

        void convertReq(const Request &request, const TimePoint &now, http::request<http::string_body> &req,
                        const std::map<std::string, std::string> &credential, const std::string &symbolId,
                        const Request::Operation operation) override {
            req.set(beast::http::field::content_type, "application/json");
            auto apiKey = mapGetWithDefault(credential, this->apiKeyName, {});
            req.set("OK-ACCESS-KEY", apiKey);

            auto pos = credential.find("OKEX_PERPETUAL_API_PASSPHRASE");
            if (pos == credential.end()) {
                std::cout << "Error with adding passphrase in request" << std::endl;
            } else {
                std::string apiPassphrase = pos->second;
                req.set("OK-ACCESS-PASSPHRASE", apiPassphrase);
            }

            switch (operation) {
                case Request::Operation::CREATE_ORDER: {
                    req.method(http::verb::post);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    req.target(this->createOrderTarget);
                    rj::Document document;
                    document.SetObject();
                    rj::Document::AllocatorType &allocator = document.GetAllocator();
                    this->appendSymbolId(document, allocator, symbolId);
                    this->appendParam(document, allocator, param, {
                            {CCAPI_EM_ORDER_SIDE,        "type"},
                            {CCAPI_EM_ORDER_LIMIT_PRICE, "price"},
                            {CCAPI_EM_ORDER_QUANTITY,    "size"},
                            {CCAPI_EM_CLIENT_ORDER_ID,   "client_oid"},
                            {CCAPI_EM_ORDER_OFFSET,      "offset"}
                    });

                    rj::StringBuffer stringBuffer;
                    rj::Writer<rj::StringBuffer> writer(stringBuffer);
                    document.Accept(writer);
                    auto body = stringBuffer.GetString();
                    req.body() = body;
                    this->signRequest(req, now, body, credential);
                }
                    break;
                case Request::Operation::CANCEL_ORDER: {
                    req.method(http::verb::post);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID)
                                                                                  :
                                     param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? param.at(
                                             CCAPI_EM_CLIENT_ORDER_ID)
                                                                                         : "";
                    auto target = this->cancelOrderTarget;
                    target += "/";
                    target += symbolId;
                    target += "/";
                    target += id;

                    req.target(target);
                    this->signRequest(req, now, "", credential);
                }
                    break;
                case Request::Operation::GET_ORDER: {
                    req.method(http::verb::get);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID)
                                                                                  :
                                     param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end() ? "client:" + param.at(
                                             CCAPI_EM_CLIENT_ORDER_ID)
                                                                                         : "";
                    auto target = this->getOrderTarget;
                    target += "/";
                    target += symbolId;
                    target += "/";
                    target += id;

                    req.target(target);
                    this->signRequest(req, now, "", credential);
                }
                    break;
                case Request::Operation::GET_OPEN_ORDERS: {
                    req.method(http::verb::get);
                    auto target = this->getOpenOrdersTarget;
                    target += "/";
                    target += symbolId;

                    req.target(target);
                    this->signRequest(req, now, "", credential);
                }
                    break;
                case Request::Operation::EDIT_ORDER: {
                    req.method(http::verb::post);
                    const std::map<std::string, std::string> &param = request.getParamList().at(0);
                    auto target = this->editOrderTarget;
                    target += "/";
                    target += symbolId;
                    req.target(target);

                    rj::Document document;
                    document.SetObject();
                    rj::Document::AllocatorType &allocator = document.GetAllocator();
                    this->appendParam(document, allocator, param, {
                            {CCAPI_EM_ORDER_ID,        "order_id"},
                            {CCAPI_EM_ORDER_LIMIT_PRICE, "new_price"},
                            {CCAPI_EM_ORDER_QUANTITY,    "new_size"},
                    });

                    rj::StringBuffer stringBuffer;
                    rj::Writer<rj::StringBuffer> writer(stringBuffer);
                    document.Accept(writer);
                    auto body = stringBuffer.GetString();
                    req.body() = body;
                    this->signRequest(req, now, "", credential);
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
                element.insert(CCAPI_EM_ORDER_ID, std::string(document.GetString()));
                elementList.emplace_back(element);
            } else if (document.IsObject()) {
                elementList.emplace_back(ExecutionManagementService::extractOrderInfo(document, extractionFieldNameMap));
            } else {
                for (const auto &x : document.GetArray()) {
                    elementList.emplace_back(ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap));
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
//          FRIEND_TEST(ExecutionManagementServiceOkexPerpetualTest, signRequest);
// #endif
    };
} /* namespace ccapi */
#endif
#endif
#endif //EXAMPLE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_OKEX_PERPETUAL_SWAP_H
