#ifndef INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
#define INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_FTX
#include "ccapi_cpp/service/ccapi_execution_management_service.h"
//used for ftx authentication
constexpr char hexmap[] = {'0',
                           '1',
                           '2',
                           '3',
                           '4',
                           '5',
                           '6',
                           '7',
                           '8',
                           '9',
                           'a',
                           'b',
                           'c',
                           'd',
                           'e',
                           'f'};
std::string string_to_hex(unsigned char* data, std::size_t len)
{
    std::string s(len * 2, ' ');
    for (std::size_t i = 0; i < len; ++i) {
        s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
}


namespace ccapi {
    class ExecutionManagementServiceFtx CCAPI_FINAL: public ExecutionManagementService {
        public: ExecutionManagementServiceFtx(std:: function < void(Event & event) > eventHandler, SessionOptions sessionOptions,
            SessionConfigs sessionConfigs, ServiceContextPtr serviceContextPtr): ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
            CCAPI_LOGGER_FUNCTION_ENTER;
            this->name = CCAPI_EXCHANGE_NAME_FTX;
            this->baseUrlRest = this->sessionConfigs.getUrlRestBase().at(this->name);
            this->setHostFromUrl(this->baseUrlRest);
            this->apiKeyName = CCAPI_FTX_API_KEY;
            this->apiSecretName = CCAPI_FTX_API_SECRET;
            this->setupCredential({
                this->apiKeyName,
                this->apiSecretName
            });
            this->createOrderTarget = "/api/orders";
            this->cancelOrderTarget = "/api/orders";
            this->getOrderTarget = "/api/orders";
            this->getOpenOrdersTarget = "/api/orders";
            this->cancelOpenOrdersTarget = "/api/orders";
            this->orderStatusOpenSet = {
                "new"
            };
            CCAPI_LOGGER_FUNCTION_EXIT;
        }

        protected:
        void signRequest(http::request<http::string_body>& req, const TimePoint& now, const std::string& body, const std::map<std::string, std::string>& credential) {
                auto apiSecret = mapGetWithDefault(credential, this->apiSecretName);
                std::string ts = std::to_string(std::chrono::duration_cast < std::chrono::milliseconds > (now.time_since_epoch()).count());

                std::string method(req.method_string());
                std::string path(req.target());
                std::string body_str(req.body());

                std::string data = ts + method + path;
                if (!body_str.empty()) {
                    data += body_str;
                }
                std::string hmacced = Hmac::hmac(Hmac::ShaVersion::SHA256, apiSecret, data);
                std::string sign = string_to_hex((unsigned char*)hmacced.c_str(), 32);

                req.set("FTX-SIGN", sign);
                req.set("FTX-TS", ts);
            }
        void appendParam(rj::Document& document, rj::Document::AllocatorType& allocator, const std::map<std::string, std::string>& param, const std::map<std::string, std::string> regularizationMap = {}) {
            for (const auto& kv : param) {
                auto key = regularizationMap.find(kv.first) != regularizationMap.end() ? regularizationMap.at(kv.first) : kv.first;
                auto value = kv.second;

                if (key == "side") {
                    value = value == CCAPI_EM_ORDER_SIDE_BUY ? "buy" : "sell";
                }
                if (key == "price" || key == "size"){
                    // `price` and `size` should be of double type on the document
                    auto doubleVal = rj::Value();
                    if(value != "NULL"){
                        double number_val =  atof(value.c_str());
                        doubleVal = rj::Value(number_val);
                    }
                    document.AddMember(rj::Value(key.c_str(), allocator).Move(), doubleVal, allocator);
                }else{
                    document.AddMember(rj::Value(key.c_str(), allocator).Move(), rj::Value(value.c_str(), allocator).Move(), allocator);
                }

            }
        }
        void appendSymbolId(rj::Document& document, rj::Document::AllocatorType& allocator, const std::string symbolId) {
            document.AddMember("market", rj::Value(symbolId.c_str(), allocator).Move(), allocator);
        }
        void convertReq(http::request<http::string_body>& req, const Request& request, const Request::Operation operation, const TimePoint& now, const std::string& symbolId, const std::map<std::string, std::string>& credential) override {
            req.set(beast::http::field::content_type, "application/json");
            auto apiKey = mapGetWithDefault(credential, this->apiKeyName);
            req.set("FTX-KEY", apiKey);

            switch (operation) {
                case Request::Operation::CREATE_ORDER:
                {
                    req.method(http::verb::post);
                    const std::map<std::string, std::string>& param = request.getParamList().at(0);
                    req.target(this->createOrderTarget);
                    rj::Document document;
                    document.SetObject();
                    rj::Document::AllocatorType& allocator = document.GetAllocator();
                    this->appendSymbolId(document, allocator, symbolId);
                    // order type handling
                    this->appendParam(document, allocator, param, {
                            {CCAPI_EM_ORDER_SIDE , "side"},
                            {CCAPI_EM_ORDER_LIMIT_PRICE , "price"},
                            {CCAPI_EM_ORDER_QUANTITY , "size"},
                            {CCAPI_EM_ORDER_TYPE , "type"},
                            {CCAPI_EM_CLIENT_ORDER_ID , "clientId"}
                    });

                    rj::StringBuffer stringBuffer;
                    rj::Writer<rj::StringBuffer> writer(stringBuffer);
                    document.Accept(writer);
                    auto body = stringBuffer.GetString();
                    req.body() = body;
                    req.prepare_payload();
                    this->signRequest(req, now, body, credential);
                }
                break;
                case Request::Operation::CANCEL_ORDER:
                {
                    req.method(http::verb::delete_);
                    const std::map<std::string, std::string>& param = request.getParamList().at(0);
                    std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID): "";
                    req.target(this->cancelOrderTarget +"/"+id);
                    this->signRequest(req, now, "", credential);
                }
                break;
                case Request::Operation::GET_ORDER:
                {
                    req.method(http::verb::get);
                    const std::map<std::string, std::string>& param = request.getParamList().at(0);
                    std::string id = param.find(CCAPI_EM_ORDER_ID) != param.end() ? param.at(CCAPI_EM_ORDER_ID): "";
                    req.target(this->getOrderTarget+"/"+id);
                    this->signRequest(req,now, "", credential);
                }
                break;
                case Request::Operation::GET_OPEN_ORDERS:
                {
                    req.method(http::verb::get);
                    auto target = this->getOpenOrdersTarget;
                    if (!symbolId.empty()) {
                        target += "?market=";
                        target += symbolId;
                    }
                    req.target(target);
                    this->signRequest(req, now,"", credential);
                }
                    break;
                case Request::Operation::CANCEL_OPEN_ORDERS:
                {
                    req.method(http::verb::delete_);
                    auto target = this->cancelOpenOrdersTarget;
                    if (!symbolId.empty()) {
                        target += "?market=";
                        target += symbolId;
                    }
                    req.target(target);
                    this->signRequest(req, now,"", credential);
                }
                    break;
                default:
                    CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
            }
        }
        std::vector < Element > extractOrderInfo(const Request::Operation operation,
                                                 const rj::Document & document) override {
            rj::StringBuffer buffer;
            rj::Writer<rj::StringBuffer> writer(buffer);
            rj::Document d = rj::Document();
            d.CopyFrom(document, d.GetAllocator());
            d.Accept(writer);

            std::cout <<  "GOT RESPONSE " << buffer.GetString() << std::endl;
            const std::map < std::string, std::pair < std::string, JsonDataType > > & extractionFieldNameMap = {
                    {
                            CCAPI_EM_ORDER_ID,
                            std::make_pair("id", JsonDataType::INTEGER)
                    },
                    {
                            CCAPI_EM_ORDER_SIDE,
                            std::make_pair("side", JsonDataType::STRING)
                    },
                    {
                            CCAPI_EM_ORDER_QUANTITY,
                            std::make_pair("size", JsonDataType::DOUBLE)
                    },
                    {
                            CCAPI_EM_ORDER_LIMIT_PRICE,
                            std::make_pair("price", JsonDataType::DOUBLE)
                    },
                    {
                            CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY,
                            std::make_pair("remainingSize", JsonDataType::DOUBLE)
                    },
                    {
                            CCAPI_EM_ORDER_STATUS,
                            std::make_pair("status", JsonDataType::STRING)
                    },
                    {
                            CCAPI_EM_ORDER_INSTRUMENT,
                            std::make_pair("market", JsonDataType::STRING)
                    },

            };
            std::vector < Element > elementList;
            if (operation == Request::Operation::CANCEL_ORDER || operation == Request::Operation::CANCEL_OPEN_ORDERS) {
                Element element;
                element.insert(CCAPI_EM_ORDER_ID, document["result"].GetString());
                elementList.emplace_back(element);
            }
            else if (document["result"].IsArray()) {
                for (const auto & x: document["result"].GetArray()) {
                    elementList.emplace_back(ExecutionManagementService::extractOrderInfo(x, extractionFieldNameMap));
                }
            }
            else if (document.IsObject()) {
                elementList.emplace_back(ExecutionManagementService::extractOrderInfo(document["result"], extractionFieldNameMap));
            }
            return elementList;
        }

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
        // TODO add more to ftx test
        public:
  using ExecutionManagementService::convertRequest;
  using ExecutionManagementService::processSuccessfulTextMessage;
  FRIEND_TEST(ExecutionManagementServiceFTXTest, signRequest);
#endif
    };
} /* namespace ccapi */
#endif
#endif
#endif  // INCLUDE_CCAPI_CPP_SERVICE_CCAPI_EXECUTION_MANAGEMENT_SERVICE_FTX_H_
