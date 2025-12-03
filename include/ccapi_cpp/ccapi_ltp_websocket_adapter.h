/**
 * @file ccapi_ltp_websocket_adapter.h
 * @brief LTP WebSocket Adapter for CCAPI
 *
 * 这个适配器将 ccapi 的 WebSocket 通信替换为 LTP UCLI FFI 接口
 * 支持通过 LTP 的发布/订阅机制进行订单下单和撤单
 *
 * @copyright Copyright (c) 2025
 */

#ifndef INCLUDE_CCAPI_CPP_CCAPI_LTP_WEBSOCKET_ADAPTER_H_
#define INCLUDE_CCAPI_CPP_CCAPI_LTP_WEBSOCKET_ADAPTER_H_

#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_queue.h"
#include "ccapi_cpp/ccapi_logger.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "ccapi_cpp/ccapi_util_private.h"
#include "ltp_ucli_ffi.h"
#include <map>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <chrono>
#include <functional>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <array>

namespace ccapi {

/**
 * @brief LTP WebSocket 适配器类
 *
 * 这个类封装了 LTP UCLI FFI 接口，提供与 ccapi WebSocket 兼容的接口
 */
class LTPWebSocketAdapter {
 public:
  /**
   * @brief 构造函数
   * @param orderPubTopic 订单发布主题（发送订单请求）
   * @param orderSubTopic 订单订阅主题（接收订单响应）
   * @param session Session 指针（用于事件分发）
   */
  LTPWebSocketAdapter(const std::string& orderPubTopic = "bf0_order_sub",
                      const std::string& orderSubTopic = "bf0_order_pub",
                      Session* session = nullptr)
      : orderPubTopic_(orderPubTopic),
        orderSubTopic_(orderSubTopic),
        running_(false),
        publisher_(nullptr),
        receiver_(nullptr),
        session_(session) {
    initialize();
  }

  ~LTPWebSocketAdapter() {
    shutdown();
  }

  /**
   * @brief 发送订单请求（下单）
   * @param request ccapi Request 对象
   * @param credential 认证信息
   * @param apiCallStartTime API调用开始时间（可选，用于延迟统计）
   * @param latencyCallback 延迟回调函数（可选，用于收集延迟数据）
   * @return 是否发送成功
   */
  bool sendCreateOrder(const Request& request,
                      const std::map<std::string, std::string>& credential,
                      const std::chrono::steady_clock::time_point& apiCallStartTime = std::chrono::steady_clock::time_point(),
                      std::function<void(long long)> latencyCallback = nullptr) {
    if (!publisher_) {
      CCAPI_LOGGER_ERROR("Publisher not initialized");
      return false;
    }

    try {
      // auto startTime = std::chrono::steady_clock::now();
      std::string ltpJson = std::move(convertRequestToLTPJson(request, credential, "order.place"));
      // auto endTime = std::chrono::steady_clock::now();
      // auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
      // std::cout << "convertRequestToLTPJson Latency: " << (latencyNs) << " ns" << std::endl;

      auto publishStartTime = std::chrono::steady_clock::now();

      if (apiCallStartTime.time_since_epoch().count() > 0) {
        auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(publishStartTime - apiCallStartTime).count();
        auto latencyUs = latencyNs / 1000;

        if (latencyCallback) {
          latencyCallback(latencyUs);
        }

        std::cout << "[Create Order] LTP adapter latency: " << latencyUs << " us" << std::endl;
      }

      publish_order(publisher_, ltpJson.c_str(), static_cast<unsigned int>(ltpJson.length()));
      CCAPI_LOGGER_DEBUG("Sent create order via LTP: " + ltpJson);
      return true;
    } catch (const std::exception& e) {
      CCAPI_LOGGER_ERROR("Failed to send create order: " + std::string(e.what()));
      return false;
    }
  }

  /**
   * @brief 发送撤单请求
   * @param request ccapi Request 对象
   * @param credential 认证信息
   * @param apiCallStartTime API调用开始时间（可选，用于延迟统计）
   * @param latencyCallback 延迟回调函数（可选，用于收集延迟数据）
   * @return 是否发送成功
   */
  bool sendCancelOrder(const Request& request,
                      const std::map<std::string, std::string>& credential,
                      const std::chrono::steady_clock::time_point& apiCallStartTime = std::chrono::steady_clock::time_point(),
                      std::function<void(long long)> latencyCallback = nullptr) {
    if (!publisher_) {
      CCAPI_LOGGER_ERROR("Publisher not initialized");
      return false;
    }

    try {
      std::string ltpJson = std::move(convertRequestToLTPJson(request, credential, "order.cancel"));
      auto publishStartTime = std::chrono::steady_clock::now();

      if (apiCallStartTime.time_since_epoch().count() > 0) {
        auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(publishStartTime - apiCallStartTime).count();
        auto latencyUs = latencyNs / 1000;

        if (latencyCallback) {
          latencyCallback(latencyUs);
        }

        std::cout << "[Cancel Order] LTP adapter latency: " << latencyUs << " us" << std::endl;
      }

      publish_order(publisher_, ltpJson.c_str(), static_cast<unsigned int>(ltpJson.length()));

      CCAPI_LOGGER_DEBUG("Sent cancel order via LTP: " + ltpJson);
      return true;
    } catch (const std::exception& e) {
      CCAPI_LOGGER_ERROR("Failed to send cancel order: " + std::string(e.what()));
      return false;
    }
  }

  /**
   * @brief 启动响应接收线程
   */
  void startReceiving() {
    if (running_) {
      return;
    }

    running_ = true;

    // 启动接收线程
    receiveThread_ = std::thread([this]() {
      this->receiveLoop();
    });

    // TODO: 新的接受线程需要绑核
  }

  /**
   * @brief 停止接收
   */
  void stopReceiving() {
    running_ = false;
    if (receiveThread_.joinable()) {
      receiveThread_.join();
    }
  }

  /**
   * @brief 检查是否已初始化
   */
  bool isInitialized() const {
    return publisher_ != nullptr && receiver_ != nullptr;
  }

 private:
  std::string orderPubTopic_;
  std::string orderSubTopic_;
  std::atomic<bool> running_;
  struct UltraOrderPublisher* publisher_;
  struct UltraOrderReceiver* receiver_;
  std::thread receiveThread_;
  Session* session_;  // Session 指针，用于事件分发
  std::mutex mutex_;

  /**
   * @brief 初始化发布者和接收者
   */
  void initialize() {
    publisher_ = init_order_publisher(orderPubTopic_.c_str(), static_cast<unsigned int>(orderPubTopic_.length()));
    if (!publisher_) {
      CCAPI_LOGGER_ERROR("Failed to initialize order publisher");
      return;
    }

    receiver_ = init_order_receiver(orderSubTopic_.c_str(), static_cast<unsigned int>(orderSubTopic_.length()));
    if (!receiver_) {
      CCAPI_LOGGER_ERROR("Failed to initialize order receiver");
      return;
    }

    CCAPI_LOGGER_INFO("LTP WebSocket adapter initialized successfully");
  }

  void shutdown() {
    stopReceiving();
    // 如果 FFI 接口提供了清理函数，应该在这里调用
  }

  /**
   * @brief 将 ccapi Request 转换为 LTP JSON 格式
   * @param request ccapi Request 对象
   * @param credential 认证信息
   * @param method 方法名（"order.place" 或 "order.cancel"）
   * @return LTP JSON 字符串
   */
  std::string convertRequestToLTPJson(const Request& request,
                                      const std::map<std::string, std::string>& credential,
                                      const std::string& method) {
    // 获取客户端订单ID（correlationId）
    std::string clientOrderId = request.getCorrelationId();
    if (clientOrderId.empty()) {
      // 如果没有 correlationId，生成一个
      std::ostringstream oss;
      oss << "ccapi_" << std::chrono::steady_clock::now().time_since_epoch().count();
      clientOrderId = oss.str();
    }

    std::string apiKey;
    std::string apiSecret;

    for (const auto& kv : credential) {
      const std::string& key = kv.first;
      const std::string& value = kv.second;

      if (apiKey.empty() && (key.find("API_KEY") != std::string::npos ||
                             key == "apiKey" ||
                             key.find("api_key") != std::string::npos)) {
        apiKey = value;
      }

      if (apiSecret.empty() && (key.find("API_SECRET") != std::string::npos ||
                                key == "apiSecret" ||
                                key.find("api_secret") != std::string::npos)) {
        apiSecret = value;
      }

      if (!apiKey.empty() && !apiSecret.empty()) break;
    }

    if (apiKey.empty() || apiSecret.empty()) {
      const auto& requestCredential = request.getCredential();
      for (const auto& kv : requestCredential) {
        const std::string& key = kv.first;
        const std::string& value = kv.second;

        if (apiKey.empty() && (key.find("API_KEY") != std::string::npos ||
                               key == "apiKey" ||
                               key.find("api_key") != std::string::npos)) {
          apiKey = value;
        }

        if (apiSecret.empty() && (key.find("API_SECRET") != std::string::npos ||
                                  key == "apiSecret" ||
                                  key.find("api_secret") != std::string::npos)) {
          apiSecret = value;
        }

        if (!apiKey.empty() && !apiSecret.empty()) break;
      }
    }

    const std::map<std::string, std::string>* paramMap = nullptr;
    const auto& paramList = request.getParamList();
    if (!paramList.empty()) {
      paramMap = &paramList[0];
    }

    std::ostringstream queryStream;
    std::ostringstream resultStream;

    int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (method == "order.place") {
      if (apiKey.empty() || apiSecret.empty()) {
        CCAPI_LOGGER_ERROR("API Key or Secret not found in credential");
        throw std::runtime_error("API Key or Secret not found in credential");
      }

      std::string symbol = request.getInstrument();
      std::string side = paramMap ? getParamValue(*paramMap, CCAPI_EM_ORDER_SIDE, "BUY") : "BUY";
      std::string type = paramMap ? getParamValue(*paramMap, CCAPI_EM_ORDER_TYPE, "LIMIT") : "LIMIT";
      std::string quantity = paramMap ? getParamValue(*paramMap, CCAPI_EM_ORDER_QUANTITY, "") : "";
      std::string price = paramMap ? getParamValue(*paramMap, CCAPI_EM_ORDER_LIMIT_PRICE, "") : "";
      std::string timeInForce = paramMap ? getParamValue(*paramMap, "timeInForce", "GTC") : "GTC";
      std::string newClientOrderId = paramMap ? getParamValue(*paramMap, CCAPI_EM_CLIENT_ORDER_ID, clientOrderId) : clientOrderId;

      if (price.empty() || quantity.empty()) {
        throw std::runtime_error("Price or quantity is empty. price=" + price + ", quantity=" + quantity);
      }

      // auto startTime = std::chrono::steady_clock::now();
      queryStream << "apiKey=" << apiKey
                  << "&newClientOrderId=" << newClientOrderId
                  << "&price=" << price
                  << "&quantity=" << quantity
                  << "&side=" << side
                  << "&symbol=" << symbol
                  << "&timeInForce=" << timeInForce
                  << "&timestamp=" << timestamp
                  << "&type=" << type;

      std::string queryString = queryStream.str();
      std::string signature = generateHMACSHA256(queryString, apiSecret);

      // auto endTime = std::chrono::steady_clock::now();
      // auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
      // std::cout << "Latency_3: " << (latencyNs) << " ns" << std::endl;

      resultStream << "{\"id\":\"" << clientOrderId
                   << "\",\"method\":\"" << method
                   << "\",\"params\":{\"apiKey\":\"" << apiKey
                   << "\",\"newClientOrderId\":\"" << newClientOrderId
                   << "\",\"symbol\":\"" << symbol
                   << "\",\"price\":" << price
                   << ",\"quantity\":" << quantity
                   << ",\"side\":\"" << side
                   << "\",\"timeInForce\":\"" << timeInForce
                   << "\",\"timestamp\":" << timestamp
                   << ",\"type\":\"" << type
                   << "\",\"signature\":\"" << signature << "\"}}";

      // CCAPI_LOGGER_DEBUG("LTP queryString: " + queryString);
      // CCAPI_LOGGER_DEBUG("LTP signature: " + signature);

      return resultStream.str();

    } else if (method == "order.cancel") {
      if (apiKey.empty() || apiSecret.empty()) {
        throw std::runtime_error("API Key or Secret not found in credential");
      }

      std::string symbol = request.getInstrument();
      std::string orderId = paramMap ? getParamValue(*paramMap, CCAPI_EM_ORDER_ID, "") : "";
      std::string origClientOrderId = paramMap ? getParamValue(*paramMap, CCAPI_EM_CLIENT_ORDER_ID, "") : "";

      if (orderId.empty() && origClientOrderId.empty()) {
        throw std::runtime_error("Both orderId and origClientOrderId are empty");
      }

      queryStream << "apiKey=" << apiKey;
      if (!orderId.empty()) {
        queryStream << "&orderId=" << orderId;
      }
      if (!origClientOrderId.empty()) {
        queryStream << "&origClientOrderId=" << origClientOrderId;
      }
      queryStream << "&symbol=" << symbol
                  << "&timestamp=" << timestamp;

      std::string queryString = queryStream.str();
      std::string signature = generateHMACSHA256(queryString, apiSecret);

      resultStream << "{\"id\":\"" << clientOrderId
                   << "\",\"method\":\"" << method
                   << "\",\"params\":{\"apiKey\":\"" << apiKey << "\"";
      if (!orderId.empty()) {
        resultStream << ",\"orderId\":\"" << orderId << "\"";
      }

      if (!origClientOrderId.empty()) {
        resultStream << ",\"origClientOrderId\":\"" << origClientOrderId << "\"";
      }

      resultStream << ",\"symbol\":\"" << symbol
                   << "\",\"timestamp\":" << timestamp
                   << ",\"signature\":\"" << signature << "\"}}";

      return resultStream.str();
    }

    return "";
  }

  /**
   * @brief 从参数映射中获取值
   */
  std::string getParamValue(const std::map<std::string, std::string>& paramMap,
                           const std::string& key,
                           const std::string& defaultValue = "") {
    auto it = paramMap.find(key);
    if (it != paramMap.end()) {
      return it->second;
    }
    return defaultValue;
  }

  /**
   * @brief 生成 HMAC SHA256 签名
   */
  std::string generateHMACSHA256(const std::string& queryString, const std::string& secret) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int digestLen;

    HMAC(EVP_sha256(),
         secret.c_str(), static_cast<int>(secret.length()),
         (unsigned char*)queryString.c_str(), static_cast<int>(queryString.length()),
         digest, &digestLen);

    static const char hexTable[] = "0123456789abcdef";
    char signatureBuf[SHA256_DIGEST_LENGTH * 2];

    for (unsigned int i = 0; i < digestLen; i++) {
      signatureBuf[i * 2] = hexTable[digest[i] >> 4];
      signatureBuf[i * 2 + 1] = hexTable[digest[i] & 0x0F];
    }

    return std::string(signatureBuf, digestLen * 2);
  }

  /**
   * @brief 接收循环（在独立线程中运行）
   */
  void receiveLoop() {
    unsigned char buffer[8192];

    while (running_) {
      if (receiver_) {
        unsigned int size = receive_order(receiver_, (char*)buffer);
        if (size > 0) {
          std::string response((char*)buffer, size);
          CCAPI_LOGGER_DEBUG("Received LTP response: " + response);

          // 将 LTP 响应转换为 ccapi Event
          Event event = convertLTPResponseToEvent(response);

          // 分发事件：通过 Session 分发（支持 EventHandler 模式）
          if (session_) {
            // 使用 Session 的 onEvent 方法分发事件（会触发 EventHandler）
            // onEvent 签名：onEvent(Event& event, Queue<Event>* eventQueue)
            // 传入 nullptr 作为 eventQueue，这样会使用 EventHandler 模式
            session_->onEvent(event, nullptr);
          } else {
            // 如果没有 Session，记录警告
            // 注意：这种情况下事件不会被分发
            CCAPI_LOGGER_ERROR("LTP adapter: No session provided, events will not be dispatched");
          }
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  bool hasError(const std::string& json) {
    return json.find("\"error\":") != std::string::npos ||
           json.find("\"Error\":") != std::string::npos ||
           json.find("\"ERROR\":") != std::string::npos;
  }

  std::string extractJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    size_t start = pos + searchKey.length();
    while (start < json.length() && (json[start] == ' ' || json[start] == '\"')) start++;

    size_t end = start;
    while (end < json.length() && json[end] != '\"' && json[end] != ',' &&
           json[end] != '}' && json[end] != ']' && json[end] != '\n') {
      end++;
    }

    if (end > start) {
      std::string value = json.substr(start, end - start);
      if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
      }
      return value;
    }
    return "";
  }

  std::string extractErrorMessage(const std::string& json) {
    std::string errorMsg;

    // Try to find "error" field
    size_t errorPos = json.find("\"error\":");
    if (errorPos == std::string::npos) {
      errorPos = json.find("\"Error\":");
    }
    if (errorPos == std::string::npos) {
      errorPos = json.find("\"ERROR\":");
    }

    if (errorPos != std::string::npos) {
      // Try to extract error message from nested object
      // Look for "msg" or "message" field in error object
      size_t msgPos = json.find("\"msg\":", errorPos);
      if (msgPos == std::string::npos) {
        msgPos = json.find("\"message\":", errorPos);
      }
      if (msgPos == std::string::npos) {
        msgPos = json.find("\"code\":", errorPos);
      }

      if (msgPos != std::string::npos) {
        if (json.find("\"code\":", errorPos) != std::string::npos &&
            json.find("\"code\":", errorPos) == msgPos) {
          errorMsg = extractJsonValue(json, "code");
          if (!errorMsg.empty()) {
            return "Error code: " + errorMsg;
          }
        } else {
          errorMsg = extractJsonValue(json, (json.find("\"message\":", errorPos) == msgPos) ? "message" : "msg");
          if (!errorMsg.empty()) {
            return errorMsg;
          }
        }
      }

      // If no message found, try to extract error code
      std::string errorCode = extractJsonValue(json, "code");
      if (!errorCode.empty()) {
        return "Error code: " + errorCode;
      }

      // If still no message, try to extract the whole error object
      size_t start = json.find("{", errorPos);
      if (start != std::string::npos) {
        int braceCount = 0;
        size_t end = start;
        for (; end < json.length(); end++) {
          if (json[end] == '{') braceCount++;
          if (json[end] == '}') {
            braceCount--;
            if (braceCount == 0) {
              errorMsg = json.substr(start, end - start + 1);
              break;
            }
          }
        }
      }
    }

    return errorMsg.empty() ? "Unknown error" : errorMsg;
  }

  /**
   * @brief 将 LTP 响应转换为 ccapi Event
   */
  Event convertLTPResponseToEvent(const std::string& ltpResponse) {
    Event event;
    event.setType(Event::Type::RESPONSE);

    Message message;
    message.setTimeReceived(UtilTime::now());
    message.setTime(UtilTime::now());

    // 检查是否是错误响应
    if (hasError(ltpResponse)) {
      // 错误响应
      message.setType(Message::Type::RESPONSE_ERROR);
      Element element;

      // 提取错误消息
      std::string errorMsg = extractErrorMessage(ltpResponse);
      element.insert(CCAPI_ERROR_MESSAGE, errorMsg);

      // 尝试提取错误码
      std::string errorCode = extractJsonValue(ltpResponse, "code");
      if (!errorCode.empty()) {
        element.insert(CCAPI_HTTP_STATUS_CODE, errorCode);
      } else {
        element.insert(CCAPI_HTTP_STATUS_CODE, "-1");
      }

      // 保存客户端订单ID（如果有）
      std::string clientOrderId = extractJsonValue(ltpResponse, "id");
      if (!clientOrderId.empty()) {
        element.insert(CCAPI_EM_CLIENT_ORDER_ID, clientOrderId);
      }

      message.setElementList({element});
    } else {
      // 成功响应
      Element element;

      // 提取客户端订单ID
      std::string clientOrderId = extractJsonValue(ltpResponse, "id");
      if (!clientOrderId.empty()) {
        element.insert(CCAPI_EM_CLIENT_ORDER_ID, clientOrderId);
      }

      // 检查是否是下单响应
      std::string orderId = extractJsonValue(ltpResponse, "orderId");
      if (!orderId.empty()) {
        // 下单成功响应
        message.setType(Message::Type::CREATE_ORDER);
        element.insert(CCAPI_EM_ORDER_ID, orderId);

        // 尝试从 result 对象中提取 orderId（如果存在）
        size_t resultPos = ltpResponse.find("\"result\":");
        if (resultPos != std::string::npos) {
          std::string resultSection = ltpResponse.substr(resultPos);
          std::string resultOrderId = extractJsonValue(resultSection, "orderId");
          if (!resultOrderId.empty()) {
            element.insert(CCAPI_EM_ORDER_ID, resultOrderId);
          }
        }

        // 提取其他订单信息
        std::string symbol = extractJsonValue(ltpResponse, "symbol");
        if (!symbol.empty()) {
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, symbol);
        }

        std::string status = extractJsonValue(ltpResponse, "status");
        if (!status.empty()) {
          element.insert(CCAPI_EM_ORDER_STATUS, status);
        }
      }
      // 检查是否是撤单响应
      else if (ltpResponse.find("CANCELED") != std::string::npos ||
               ltpResponse.find("\"status\":\"CANCELED\"") != std::string::npos) {
        // 撤单成功响应
        message.setType(Message::Type::CANCEL_ORDER);

        // 提取订单ID（撤单响应可能包含 orderId）
        std::string cancelOrderId = extractJsonValue(ltpResponse, "orderId");
        if (!cancelOrderId.empty()) {
          element.insert(CCAPI_EM_ORDER_ID, cancelOrderId);
        }

        // 提取状态
        element.insert(CCAPI_EM_ORDER_STATUS, "CANCELED");

        // 提取交易对
        std::string symbol = extractJsonValue(ltpResponse, "symbol");
        if (!symbol.empty()) {
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, symbol);
        }
      } else {
        // 未知响应类型，设置为通用响应
        message.setType(Message::Type::RESPONSE_ERROR);
        element.insert(CCAPI_ERROR_MESSAGE, "Unknown response format");
      }

      message.setElementList({element});
    }

    event.setMessageList({message});
    return event;
  }
};

} /* namespace ccapi */

#endif  // INCLUDE_CCAPI_CPP_CCAPI_LTP_WEBSOCKET_ADAPTER_H_

