#ifndef INCLUDE_LTP_TRADING_SERVICE_H_
#define INCLUDE_LTP_TRADING_SERVICE_H_

#include "ccapi_cpp/ccapi_ltp_trading_interface.h"
#include "ccapi_cpp/ccapi_session.h"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_subscription.h"
#include <map>
#include <string>
#include <iostream>
#include <chrono>

namespace ltp {

// 引入ccapi命名空间的必要类型
using ccapi::Session;
using ccapi::Request;
using ccapi::Event;
using ccapi::Message;
using ccapi::Element;
using ccapi::Subscription;
using ccapi::Queue;
using ccapi::UtilTime;

/**
 * 交易所协议能力配置
 */
struct ExchangeCapabilities {
  bool supportsFix = false;        // 是否支持FIX协议
  bool supportsWebsocket = false;  // 是否支持WebSocket协议
  bool supportsRest = true;        // 是否支持REST协议（默认都支持）
};

/**
 * 统一交易服务类
 *
 * 这个类封装了ccapi的Session，提供统一的交易接口
 * 客户可以通过这个类在不同交易所进行交易，而无需关心各交易所的参数差异
 *
 * 协议选择策略：
 * - 同步接口(createOrder等): 始终使用REST协议
 * - 异步接口(createOrderAsync等): 根据交易所能力配置，按优先级选择：FIX > WebSocket > REST
 */
class LTPTradingService {
 public:
  /**
   * 构造函数
   * @param session ccapi的Session对象指针
   */
  explicit LTPTradingService(Session* session)
    : session_(session), enableLatencyStats_(false) {
    initializeDefaultCapabilities();
  }

  /**
   * 析构函数
   */
  virtual ~LTPTradingService() {}

  /**
   * 设置交易所的协议能力
   * @param exchange 交易所
   * @param capabilities 协议能力配置
   *
   * 示例：
   * ExchangeCapabilities caps;
   * caps.supportsFix = true;
   * caps.supportsWebsocket = true;
   * caps.supportsRest = true;
   * service.setExchangeCapabilities(LTPExchange::BINANCE, caps);
   */
  void setExchangeCapabilities(LTPExchange exchange, const ExchangeCapabilities& capabilities) {
    std::string exchangeStr = ltp::exchangeToString(exchange);
    exchangeCapabilities_[exchangeStr] = capabilities;
  }

  /**
   * 启用/禁用延迟统计
   * @param enable true表示启用，false表示禁用
   */
  void setEnableLatencyStats(bool enable) {
    enableLatencyStats_ = enable;
  }

  /**
   * 获取延迟统计状态
   * @return true表示已启用，false表示已禁用
   */
  bool isLatencyStatsEnabled() const {
    return enableLatencyStats_;
  }

  /**
   * 获取交易所的协议能力
   * @param exchange 交易所
   * @return 协议能力配置
   */
  ExchangeCapabilities getExchangeCapabilities(LTPExchange exchange) const {
    std::string exchangeStr = ltp::exchangeToString(exchange);
    auto it = exchangeCapabilities_.find(exchangeStr);
    if (it != exchangeCapabilities_.end()) {
      return it->second;
    }
    // 默认只支持REST
    ExchangeCapabilities defaultCaps;
    defaultCaps.supportsRest = true;
    return defaultCaps;
  }

  // ====================================================================
  // 同步接口 - 使用REST协议，通过Queue<Event>接收响应
  // ====================================================================

  /**
   * 创建订单（同步）
   * @param request 统一的创建订单请求
   * @param credential 认证信息（API Key等）
   * @return 统一的响应结果
   */
  LTPResponse createOrder(const LTPCreateOrderRequest& request,
                              const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToCreateOrderRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  /**
   * 取消订单（同步）
   * @param request 统一的取消订单请求
   * @param credential 认证信息
   * @return 统一的响应结果
   */
  LTPResponse cancelOrder(const LTPCancelOrderRequest& request,
                              const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToCancelOrderRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  /**
   * 取消所有订单（同步）
   * @param request 统一的取消所有订单请求
   * @param credential 认证信息
   * @return 统一的响应结果
   */
  LTPResponse cancelAllOrders(const LTPCancelAllOrdersRequest& request,
                                  const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToCancelAllOrdersRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  /**
   * 查询订单（同步）
   * @param request 统一的查询订单请求
   * @param credential 认证信息
   * @return 统一的响应结果
   */
  LTPResponse getOrder(const LTPGetOrderRequest& request,
                          const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToGetOrderRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  /**
   * 查询开放订单（同步）
   * @param request 统一的查询开放订单请求
   * @param credential 认证信息
   * @return 统一的响应结果
   */
  LTPResponse getOpenOrders(const LTPGetOpenOrdersRequest& request,
                                const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToGetOpenOrdersRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  /**
   * 查询账户余额（同步）
   * @param request 统一的查询账户余额请求
   * @param credential 认证信息
   * @return 统一的响应结果
   */
  LTPResponse getAccountBalances(const LTPGetAccountBalancesRequest& request,
                                     const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToGetAccountBalancesRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  /**
   * 查询持仓（同步）
   * @param request 统一的查询持仓请求
   * @param credential 认证信息
   * @return 统一的响应结果
   */
  LTPResponse getAccountPositions(const LTPGetAccountPositionsRequest& request,
                                      const std::map<std::string, std::string>& credential = {}) {
    Request ccapiRequest = convertToGetAccountPositionsRequest(request, credential);
    return executeRequestSync(ccapiRequest);
  }

  // ====================================================================
  // 异步接口 - 根据交易所能力自动选择最优协议（FIX > WebSocket > REST）
  // ====================================================================

  /**
   * 创建订单（异步）
   * 根据交易所能力配置，按优先级选择协议：FIX > WebSocket > REST
   * @param request 统一的创建订单请求
   * @param credential 认证信息
   * @param correlationId 关联ID（用于匹配请求和响应）
   */
  void createOrderAsync(const LTPCreateOrderRequest& request,
                       const std::map<std::string, std::string>& credential = {},
                       const std::string& correlationId = "") {
    // 记录API调用开始时间（纳秒级）
    auto apiCallStartTime = std::chrono::steady_clock::now();

    // 零拷贝优化：直接构造最终Request，避免中间转换
    std::string exchange = ltp::exchangeToString(request.exchange);

    // 检查交易所能力，选择最优快速路径
    auto it = exchangeCapabilities_.find(exchange);
    if (it != exchangeCapabilities_.end()) {
      // 优先级1: FIX快速路径（最低延迟）
      if (it->second.supportsFix) {
        createOrderAsyncFixFastPath(request, credential, correlationId, exchange, apiCallStartTime);
        return;
      }
      // 优先级2: WebSocket快速路径
      if (it->second.supportsWebsocket) {
        createOrderAsyncWebSocketFastPath(request, credential, correlationId, exchange, apiCallStartTime);
        return;
      }
    }

    // 降级到标准路径（REST或首次连接）
    Request ccapiRequest = convertToCreateOrderRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }
    sendRequestWithBestProtocol(exchange, ccapiRequest, credential, apiCallStartTime);
  }

  /**
   * 取消订单（异步）
   * 根据交易所能力配置，按优先级选择协议：FIX > WebSocket > REST
   */
  void cancelOrderAsync(const LTPCancelOrderRequest& request,
                       const std::map<std::string, std::string>& credential = {},
                       const std::string& correlationId = "") {
    // 记录API调用开始时间（纳秒级）
    auto apiCallStartTime = std::chrono::steady_clock::now();

    // 零拷贝优化：直接构造最终Request，避免中间转换
    std::string exchange = ltp::exchangeToString(request.exchange);

    // 检查交易所能力，选择最优快速路径
    auto it = exchangeCapabilities_.find(exchange);
    if (it != exchangeCapabilities_.end()) {
      // 优先级1: FIX快速路径（最低延迟）
      if (it->second.supportsFix) {
        cancelOrderAsyncFixFastPath(request, credential, correlationId, exchange, apiCallStartTime);
        return;
      }
      // 优先级2: WebSocket快速路径
      if (it->second.supportsWebsocket) {
        cancelOrderAsyncWebSocketFastPath(request, credential, correlationId, exchange, apiCallStartTime);
        return;
      }
    }

    // 降级到标准路径（REST或首次连接）
    Request ccapiRequest = convertToCancelOrderRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }
    sendRequestWithBestProtocol(exchange, ccapiRequest, credential, apiCallStartTime);
  }

  /**
   * 取消所有订单（异步）
   */
  void cancelAllOrdersAsync(const LTPCancelAllOrdersRequest& request,
                           const std::map<std::string, std::string>& credential = {},
                           const std::string& correlationId = "") {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest = convertToCancelAllOrdersRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }

    sendRequestWithBestProtocol(exchange, ccapiRequest, credential);
  }

  /**
   * 查询订单（异步）
   * 注意：查询操作强制使用REST协议，因为大多数交易所的WebSocket不支持查询操作
   */
  void getOrderAsync(const LTPGetOrderRequest& request,
                    const std::map<std::string, std::string>& credential = {},
                    const std::string& correlationId = "") {
    Request ccapiRequest = convertToGetOrderRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }

    // 查询操作强制使用REST
    session_->sendRequest(ccapiRequest);
  }

  /**
   * 查询开放订单（异步）
   * 注意：查询操作强制使用REST协议，因为大多数交易所的WebSocket不支持查询操作
   */
  void getOpenOrdersAsync(const LTPGetOpenOrdersRequest& request,
                         const std::map<std::string, std::string>& credential = {},
                         const std::string& correlationId = "") {
    Request ccapiRequest = convertToGetOpenOrdersRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }

    // 查询操作强制使用REST
    session_->sendRequest(ccapiRequest);
  }

  /**
   * 查询账户余额（异步）
   * 注意：查询操作强制使用REST协议，因为大多数交易所的WebSocket不支持查询操作
   */
  void getAccountBalancesAsync(const LTPGetAccountBalancesRequest& request,
                               const std::map<std::string, std::string>& credential = {},
                               const std::string& correlationId = "") {
    Request ccapiRequest = convertToGetAccountBalancesRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }

    // 查询操作强制使用REST
    session_->sendRequest(ccapiRequest);
  }

  /**
   * 查询持仓（异步）
   * 注意：查询操作强制使用REST协议，因为大多数交易所的WebSocket不支持查询操作
   */
  void getAccountPositionsAsync(const LTPGetAccountPositionsRequest& request,
                                const std::map<std::string, std::string>& credential = {},
                                const std::string& correlationId = "") {
    Request ccapiRequest = convertToGetAccountPositionsRequest(request, credential);
    if (!correlationId.empty()) {
      ccapiRequest.setCorrelationId(correlationId);
    }

    // 查询操作强制使用REST
    session_->sendRequest(ccapiRequest);
  }

  // ====================================================================
  // 订阅接口 - 订阅实时订单状态更新
  // ====================================================================

  /**
   * 订阅订单状态更新
   * 通过WebSocket实时接收订单状态变化（新建、部分成交、完全成交、撤销等）
   *
   * @param exchange 交易所
   * @param symbol 交易对（空字符串表示订阅所有交易对）
   * @param credential 认证信息
   * @param correlationId 关联ID（用于标识此订阅）
   *
   * 示例：
   * // 订阅OKX所有交易对的订单更新
   * service.subscribeOrderUpdates(LTPExchange::OKX, "", credential, "okx-orders");
   *
   * // 订阅币安U本位合约特定交易对的订单更新
   * service.subscribeOrderUpdates(LTPExchange::BINANCE_USDS_FUTURES, "BTCUSDT", credential, "binance-btc");
   */
  void subscribeOrderUpdates(LTPExchange exchange,
                            const std::string& symbol,
                            const std::map<std::string, std::string>& credential,
                            const std::string& correlationId = "") {
    std::string exchangeStr = ltp::exchangeToString(exchange);
    std::string corrId = correlationId.empty()
                        ? "order_updates_" + exchangeStr + "_" + std::to_string(UtilTime::now().time_since_epoch().count())
                        : correlationId;

    // 准备订阅的credential
    std::map<std::string, std::string> subCredential = prepareSubscriptionCredential(exchangeStr, credential);

    // 创建订阅
    Subscription subscription(
      exchangeStr,                    // 交易所
      symbol,                         // 交易对（空字符串表示所有）
      CCAPI_EM_ORDER_UPDATE,         // 订单状态更新
      "",                             // 选项
      corrId,                         // 关联ID
      subCredential                   // 认证信息
    );

    session_->subscribe(subscription);
  }

  /**
   * 订阅私有成交更新
   * 实时接收订单的成交信息
   *
   * @param exchange 交易所
   * @param symbol 交易对（空字符串表示订阅所有交易对）
   * @param credential 认证信息
   * @param correlationId 关联ID
   */
  void subscribePrivateTrades(LTPExchange exchange,
                             const std::string& symbol,
                             const std::map<std::string, std::string>& credential,
                             const std::string& correlationId = "") {
    std::string exchangeStr = ltp::exchangeToString(exchange);
    std::string corrId = correlationId.empty()
                        ? "private_trades_" + exchangeStr + "_" + std::to_string(UtilTime::now().time_since_epoch().count())
                        : correlationId;

    std::map<std::string, std::string> subCredential = prepareSubscriptionCredential(exchangeStr, credential);

    Subscription subscription(
      exchangeStr,
      symbol,
      CCAPI_EM_PRIVATE_TRADE,        // 私有成交
      "",
      corrId,
      subCredential
    );

    session_->subscribe(subscription);
  }

  /**
   * 订阅账户余额更新
   * 实时接收账户余额变化
   *
   * @param exchange 交易所
   * @param credential 认证信息
   * @param correlationId 关联ID
   */
  void subscribeBalanceUpdates(LTPExchange exchange,
                              const std::map<std::string, std::string>& credential,
                              const std::string& correlationId = "") {
    std::string exchangeStr = ltp::exchangeToString(exchange);
    std::string corrId = correlationId.empty()
                        ? "balance_updates_" + exchangeStr + "_" + std::to_string(UtilTime::now().time_since_epoch().count())
                        : correlationId;

    std::map<std::string, std::string> subCredential = prepareSubscriptionCredential(exchangeStr, credential);

    Subscription subscription(
      exchangeStr,
      "",                            // 余额订阅不需要指定交易对
      CCAPI_EM_BALANCE_UPDATE,       // 余额更新
      "",
      corrId,
      subCredential
    );

    session_->subscribe(subscription);
  }

  /**
   * 订阅持仓更新
   * 实时接收持仓变化（仅适用于合约交易）
   *
   * @param exchange 交易所
   * @param symbol 交易对（空字符串表示订阅所有交易对）
   * @param credential 认证信息
   * @param correlationId 关联ID
   */
  void subscribePositionUpdates(LTPExchange exchange,
                               const std::string& symbol,
                               const std::map<std::string, std::string>& credential,
                               const std::string& correlationId = "") {
    std::string exchangeStr = ltp::exchangeToString(exchange);
    std::string corrId = correlationId.empty()
                        ? "position_updates_" + exchangeStr + "_" + std::to_string(UtilTime::now().time_since_epoch().count())
                        : correlationId;

    std::map<std::string, std::string> subCredential = prepareSubscriptionCredential(exchangeStr, credential);

    Subscription subscription(
      exchangeStr,
      symbol,
      CCAPI_EM_POSITION_UPDATE,      // 持仓更新
      "",
      corrId,
      subCredential
    );

    session_->subscribe(subscription);
  }

  // ====================================================================
  // 辅助方法 - 将ccapi的Event转换为LTPResponse
  // ====================================================================

  /**
   * 将ccapi的Event转换为LTPResponse
   * @param event ccapi的事件对象
   * @return 统一的响应结果
   */
  static LTPResponse convertEventToResponse(const Event& event) {
    LTPResponse response;

    // 设置事件类型
    if (event.getType() == Event::Type::RESPONSE) {
      response.eventType = LTPEventType::RESPONSE;
      response.eventTypeString = "RESPONSE";
    } else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      response.eventType = LTPEventType::SUBSCRIPTION_DATA;
      response.eventTypeString = "SUBSCRIPTION_DATA";
    } else if (event.getType() == Event::Type::SESSION_STATUS) {
      response.eventType = LTPEventType::SESSION_STATUS;
      response.eventTypeString = "SESSION_STATUS";
    } else if (event.getType() == Event::Type::AUTHORIZATION_STATUS) {
      response.eventType = LTPEventType::AUTHORIZATION_STATUS;
      response.eventTypeString = "AUTHORIZATION_STATUS";
    } else if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
      response.eventType = LTPEventType::SUBSCRIPTION_STATUS;
      response.eventTypeString = "SUBSCRIPTION_STATUS";
    } else if (event.getType() == Event::Type::REQUEST_STATUS) {
      response.eventType = LTPEventType::REQUEST_STATUS;
      response.eventTypeString = "REQUEST_STATUS";
    } else if (event.getType() == Event::Type::FIX) {
      response.eventType = LTPEventType::FIX;
      response.eventTypeString = "FIX";
    } else if (event.getType() == Event::Type::FIX_STATUS) {
      response.eventType = LTPEventType::FIX_STATUS;
      response.eventTypeString = "FIX_STATUS";
    } else if (event.getType() == Event::Type::HEARTBEAT) {
      response.eventType = LTPEventType::HEARTBEAT;
      response.eventTypeString = "HEARTBEAT";
    } else {
      response.eventType = LTPEventType::OTHER;
      response.eventTypeString = "OTHER";
    }

    // 处理消息
    const auto& messageList = event.getMessageList();
    if (!messageList.empty()) {
      const auto& message = messageList[0];

      // 设置消息类型
      if (message.getType() == Message::Type::CREATE_ORDER) {
        response.messageType = LTPMessageType::CREATE_ORDER;
        response.messageTypeString = "CREATE_ORDER";
      } else if (message.getType() == Message::Type::CANCEL_ORDER) {
        response.messageType = LTPMessageType::CANCEL_ORDER;
        response.messageTypeString = "CANCEL_ORDER";
      } else if (message.getType() == Message::Type::GET_ORDER) {
        response.messageType = LTPMessageType::GET_ORDER;
        response.messageTypeString = "GET_ORDER";
      } else if (message.getType() == Message::Type::GET_OPEN_ORDERS) {
        response.messageType = LTPMessageType::GET_OPEN_ORDERS;
        response.messageTypeString = "GET_OPEN_ORDERS";
      } else if (message.getType() == Message::Type::GET_ACCOUNT_BALANCES) {
        response.messageType = LTPMessageType::GET_ACCOUNT_BALANCES;
        response.messageTypeString = "GET_ACCOUNT_BALANCES";
      } else if (message.getType() == Message::Type::GET_ACCOUNT_POSITIONS) {
        response.messageType = LTPMessageType::GET_ACCOUNT_POSITIONS;
        response.messageTypeString = "GET_ACCOUNT_POSITIONS";
      } else if (message.getType() == Message::Type::RESPONSE_ERROR) {
        response.messageType = LTPMessageType::RESPONSE_ERROR;
        response.messageTypeString = "RESPONSE_ERROR";
      } else if (message.getType() == Message::Type::SESSION_CONNECTION_UP) {
        response.messageType = LTPMessageType::SESSION_CONNECTION_UP;
        response.messageTypeString = "SESSION_CONNECTION_UP";
      } else if (message.getType() == Message::Type::SESSION_CONNECTION_DOWN) {
        response.messageType = LTPMessageType::SESSION_CONNECTION_DOWN;
        response.messageTypeString = "SESSION_CONNECTION_DOWN";
      } else if (message.getType() == Message::Type::AUTHORIZATION_SUCCESS) {
        response.messageType = LTPMessageType::AUTHORIZATION_SUCCESS;
        response.messageTypeString = "AUTHORIZATION_SUCCESS";
      } else if (message.getType() == Message::Type::AUTHORIZATION_FAILURE) {
        response.messageType = LTPMessageType::AUTHORIZATION_FAILURE;
        response.messageTypeString = "AUTHORIZATION_FAILURE";
      } else if (message.getType() == Message::Type::SUBSCRIPTION_STARTED) {
        response.messageType = LTPMessageType::SUBSCRIPTION_STARTED;
        response.messageTypeString = "SUBSCRIPTION_STARTED";
      } else if (message.getType() == Message::Type::SUBSCRIPTION_FAILURE) {
        response.messageType = LTPMessageType::SUBSCRIPTION_FAILURE;
        response.messageTypeString = "SUBSCRIPTION_FAILURE";
      } else if (message.getType() == Message::Type::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE) {
        response.messageType = LTPMessageType::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE;
        response.messageTypeString = "SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE";
      } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE) {
        response.messageType = LTPMessageType::ORDER_UPDATE;
        response.messageTypeString = "ORDER_UPDATE";
      } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE) {
        response.messageType = LTPMessageType::PRIVATE_TRADE;
        response.messageTypeString = "PRIVATE_TRADE";
      } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE) {
        response.messageType = LTPMessageType::BALANCE_UPDATE;
        response.messageTypeString = "BALANCE_UPDATE";
      } else if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_POSITION_UPDATE) {
        response.messageType = LTPMessageType::POSITION_UPDATE;
        response.messageTypeString = "POSITION_UPDATE";
      } else {
        response.messageType = LTPMessageType::OTHER;
        response.messageTypeString = "OTHER";
      }

      // 处理RESPONSE类型的事件（实际的交易响应）
      if (event.getType() == Event::Type::RESPONSE) {
        // 检查是否有错误
        if (message.getType() == Message::Type::RESPONSE_ERROR) {
          response.success = false;
          const auto& elementList = message.getElementList();
          if (!elementList.empty()) {
            const auto& element = elementList[0];
            response.errorMessage = element.getValue(CCAPI_ERROR_MESSAGE);
            response.errorCode = element.getValue(CCAPI_HTTP_STATUS_CODE);
          }
        } else {
          response.success = true;

          // 根据消息类型解析不同的响应
          const auto& elementList = message.getElementList();
          if (!elementList.empty()) {
            // 生成原始响应（使用ccapi的toString函数）
            response.rawResponse = ccapi::toString(elementList);

            // 账户余额响应
            if (message.getType() == Message::Type::GET_ACCOUNT_BALANCES) {
              for (const auto& element : elementList) {
                response.balances.push_back(convertElementToBalanceInfo(element));
              }
            }
            // 持仓响应
            else if (message.getType() == Message::Type::GET_ACCOUNT_POSITIONS) {
              for (const auto& element : elementList) {
                response.positions.push_back(convertElementToPositionInfo(element));
              }
            }
            // 订单响应
            else {
              // 单个订单响应
              if (elementList.size() == 1) {
                response.orderInfo = convertElementToOrderInfo(elementList[0]);
              } else {
                // 多个订单响应
                for (const auto& element : elementList) {
                  response.orders.push_back(convertElementToOrderInfo(element));
                }
              }
            }
          }
        }
      }
      // 处理状态事件
      else if (event.getType() == Event::Type::SESSION_STATUS) {
        // SESSION_STATUS事件
        if (message.getType() == Message::Type::SESSION_CONNECTION_UP) {
          response.success = true;
          response.errorMessage = "WebSocket connection established";
        } else if (message.getType() == Message::Type::SESSION_CONNECTION_DOWN) {
          response.success = false;
          response.errorMessage = "WebSocket connection closed";
        } else {
          response.success = true;
          response.errorMessage = "Session status event";
        }
      }
      else if (event.getType() == Event::Type::AUTHORIZATION_STATUS) {
        // AUTHORIZATION_STATUS事件
        if (message.getType() == Message::Type::AUTHORIZATION_SUCCESS) {
          response.success = true;
          response.errorMessage = "Authorization successful";
        } else if (message.getType() == Message::Type::AUTHORIZATION_FAILURE) {
          response.success = false;
          const auto& elementList = message.getElementList();
          if (!elementList.empty()) {
            const auto& element = elementList[0];
            response.errorMessage = element.getValue(CCAPI_ERROR_MESSAGE);
          } else {
            response.errorMessage = "Authorization failed";
          }
        } else {
          response.success = true;
          response.errorMessage = "Authorization status event";
        }
      }
      else if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
        // SUBSCRIPTION_STATUS事件
        if (message.getType() == Message::Type::SUBSCRIPTION_STARTED) {
          response.success = true;
          response.errorMessage = "Subscription started";
        } else if (message.getType() == Message::Type::SUBSCRIPTION_FAILURE) {
          response.success = false;
          const auto& elementList = message.getElementList();
          if (!elementList.empty()) {
            const auto& element = elementList[0];
            response.errorMessage = element.getValue(CCAPI_ERROR_MESSAGE);
          } else {
            response.errorMessage = "Subscription failed";
          }
        } else if (message.getType() == Message::Type::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE) {
          response.success = false;
          response.errorMessage = "Subscription failed due to connection failure (will auto-retry)";
        } else {
          response.success = true;
          response.errorMessage = "Subscription status event";
        }
      }
      // 处理SUBSCRIPTION_DATA事件（实时推送的订单更新、成交等）
      else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
        response.success = true;
        response.errorMessage = "Subscription data received";

        // 解析订单信息（如果有）
        const auto& elementList = message.getElementList();
        if (!elementList.empty()) {
          // 单个订单响应
          if (elementList.size() == 1) {
            response.orderInfo = convertElementToOrderInfo(elementList[0]);
          } else {
            // 多个订单响应
            for (const auto& element : elementList) {
              response.orders.push_back(convertElementToOrderInfo(element));
            }
          }
        }
      }
      // 处理REQUEST_STATUS事件
      else if (event.getType() == Event::Type::REQUEST_STATUS) {
        response.success = true;
        response.errorMessage = "Request status event";
      }
      // 处理FIX相关事件
      else if (event.getType() == Event::Type::FIX) {
        response.success = true;
        response.errorMessage = "FIX message received";
      }
      else if (event.getType() == Event::Type::FIX_STATUS) {
        response.success = true;
        response.errorMessage = "FIX status event";
      }
      // 处理HEARTBEAT事件
      else if (event.getType() == Event::Type::HEARTBEAT) {
        response.success = true;
        response.errorMessage = "Heartbeat received";
      }
      // 其他未知类型的事件
      else {
        response.success = false;
        response.errorMessage = "Unknown event type";
      }
    }

    return response;
  }

 private:
  Session* session_;
  std::map<std::string, ExchangeCapabilities> exchangeCapabilities_;
  std::map<std::string, std::string> websocketSubscriptionCorrelationIds_;  // 交易所 -> WebSocket订阅correlationId
  std::map<std::string, std::string> fixSubscriptionCorrelationIds_;        // 交易所 -> FIX订阅correlationId
  bool enableLatencyStats_;  // 是否启用延迟统计

  /**
   * FIX快速路径 - 零拷贝优化（最低延迟）
   * FIX协议是专为金融交易设计的二进制协议，延迟比WebSocket更低
   * 直接构造最终的Request对象，避免convertToCreateOrderRequest的开销
   */
  inline void createOrderAsyncFixFastPath(const LTPCreateOrderRequest& request,
                                         const std::map<std::string, std::string>& credential,
                                         const std::string& correlationId,
                                         const std::string& exchange,
                                         const std::chrono::steady_clock::time_point& apiCallStartTime) {
    // 获取FIX correlationId（已缓存，快速）
    auto fixIt = fixSubscriptionCorrelationIds_.find(exchange);
    if (fixIt == fixSubscriptionCorrelationIds_.end()) {
      // 首次调用，需要建立FIX连接（降级到标准路径）
      Request ccapiRequest = convertToCreateOrderRequest(request, credential);
      if (!correlationId.empty()) {
        ccapiRequest.setCorrelationId(correlationId);
      }
      sendRequestWithBestProtocol(exchange, ccapiRequest, credential, apiCallStartTime);
      return;
    }

    const std::string& fixCorrelationId = fixIt->second;

    // 直接构造Request - 零拷贝优化
    Request fixRequest(Request::Operation::CREATE_ORDER, exchange, request.symbol,
                      correlationId.empty() ? "" : correlationId,
                      std::map<std::string, std::string>{});  // FIX不需要credential

    // 直接构造参数map - 极致优化版本
    std::map<std::string, std::string> param;

    // 订单方向（必需）
    param.emplace(CCAPI_EM_ORDER_SIDE, ltp::orderSideToString(request.side));

    // 订单类型（必需）
    if (exchange != CCAPI_EXCHANGE_NAME_OKX) {
      param.emplace(CCAPI_EM_ORDER_TYPE, ltp::orderTypeToString(request.type));
    }

    // 客户端订单ID
    if (!request.clientOrderId.empty()) {
      param.emplace(CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId);
    }

    // 价格
    if (!request.price.empty()) {
      param.emplace(CCAPI_EM_ORDER_LIMIT_PRICE, request.price);
    }

    // 数量
    if (!request.quantity.empty()) {
      param.emplace(CCAPI_EM_ORDER_QUANTITY, request.quantity);
    }
    if (!request.quoteOrderQty.empty()) {
      param.emplace("quoteOrderQty", request.quoteOrderQty);
    }

    // 时间有效性
    if (request.timeInForce != LTPTimeInForce::UNKNOWN &&
        request.type != LTPOrderType::MARKET &&
        exchange != CCAPI_EXCHANGE_NAME_OKX) {
      param.emplace("timeInForce", ltp::timeInForceToString(request.timeInForce));
    }

    // 高级选项
    if (request.reduceOnly) {
      param.emplace("reduceOnly", "true");
    }
    if (request.postOnly) {
      param.emplace("postOnly", "true");
    }
    if (!request.leverage.empty()) {
      param.emplace(CCAPI_EM_ORDER_LEVERAGE, request.leverage);
    }
    if (!request.marginMode.empty()) {
      param.emplace(CCAPI_MARGIN_MODE, request.marginMode);
    }
    if (!request.positionSide.empty()) {
      param.emplace("positionSide", request.positionSide);
    }
    if (!request.stopPrice.empty()) {
      param.emplace("stopPrice", request.stopPrice);
    }

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.emplace(kv.first, kv.second);
    }

    // 使用move避免拷贝
    fixRequest.appendParam(std::move(param));

    // 记录延迟
    if (enableLatencyStats_ && apiCallStartTime.time_since_epoch().count() > 0) {
      auto sendTime = std::chrono::steady_clock::now();
      auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(sendTime - apiCallStartTime).count();
      std::cout << "[LTPTradingService] createOrderAsync to sendRequestByFix latency: "
                << latencyNs << " ns (" << (latencyNs / 1000.0) << " us)" << std::endl;
    }

    // 直接发送
    session_->sendRequestByFix(fixCorrelationId, fixRequest);
  }

  /**
   * WebSocket快速路径 - 零拷贝优化
   * 直接构造最终的Request对象，避免convertToCreateOrderRequest的开销
   */
  inline void createOrderAsyncWebSocketFastPath(const LTPCreateOrderRequest& request,
                                               const std::map<std::string, std::string>& credential,
                                               const std::string& correlationId,
                                               const std::string& exchange,
                                               const std::chrono::steady_clock::time_point& apiCallStartTime) {
    // 获取WebSocket correlationId（已缓存，快速）
    auto wsIt = websocketSubscriptionCorrelationIds_.find(exchange);
    if (wsIt == websocketSubscriptionCorrelationIds_.end()) {
      // 首次调用，需要建立连接（降级到标准路径）
      Request ccapiRequest = convertToCreateOrderRequest(request, credential);
      if (!correlationId.empty()) {
        ccapiRequest.setCorrelationId(correlationId);
      }
      sendRequestWithBestProtocol(exchange, ccapiRequest, credential, apiCallStartTime);
      return;
    }

    const std::string& wsCorrelationId = wsIt->second;

    // 直接构造Request - 零拷贝优化
    // 注意：WebSocket不需要credential
    Request wsRequest(Request::Operation::CREATE_ORDER, exchange, request.symbol,
                     correlationId.empty() ? "" : correlationId,
                     std::map<std::string, std::string>{});  // 空credential

    // 直接构造参数map - 极致优化版本
    // 使用emplace + move语义减少拷贝
    std::map<std::string, std::string> param;

    // 订单方向（必需） - 直接emplace，避免临时对象
    param.emplace(CCAPI_EM_ORDER_SIDE, ltp::orderSideToString(request.side));

    // 订单类型（必需，OKX除外）
    if (exchange != CCAPI_EXCHANGE_NAME_OKX) {
      param.emplace(CCAPI_EM_ORDER_TYPE, ltp::orderTypeToString(request.type));
    }

    // 客户端订单ID
    if (!request.clientOrderId.empty()) {
      param.emplace(CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId);
    }

    // 杠杆
    if (!request.leverage.empty()) {
      param.emplace(CCAPI_EM_ORDER_LEVERAGE, request.leverage);
    }

    // 保证金模式
    if (!request.marginMode.empty()) {
      param.emplace(CCAPI_MARGIN_MODE, request.marginMode);
    }

    // 价格
    if (!request.price.empty()) {
      param.emplace(CCAPI_EM_ORDER_LIMIT_PRICE, request.price);
    }

    // 持仓方向
    if (!request.positionSide.empty()) {
      param.emplace("positionSide", request.positionSide);
    }

    // postOnly标志
    if (request.postOnly) {
      param.emplace("postOnly", "true");
    }

    // 数量
    if (!request.quantity.empty()) {
      param.emplace(CCAPI_EM_ORDER_QUANTITY, request.quantity);
    }
    if (!request.quoteOrderQty.empty()) {
      param.emplace("quoteOrderQty", request.quoteOrderQty);
    }

    // reduceOnly标志
    if (request.reduceOnly) {
      param.emplace("reduceOnly", "true");
    }

    // 止损价格
    if (!request.stopPrice.empty()) {
      param.emplace("stopPrice", request.stopPrice);
    }

    // OKX特殊处理：tdMode
    if (exchange == CCAPI_EXCHANGE_NAME_OKX) {
      auto tdModeIt = request.extraParams.find("tdMode");
      if (tdModeIt == request.extraParams.end()) {
        param.emplace("tdMode", "cross");
      }
    }

    // 时间有效性
    if (request.timeInForce != LTPTimeInForce::UNKNOWN &&
        request.type != LTPOrderType::MARKET &&
        exchange != CCAPI_EXCHANGE_NAME_OKX) {
      param.emplace("timeInForce", ltp::timeInForceToString(request.timeInForce));
    }

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.emplace(kv.first, kv.second);
    }

    // 使用move避免拷贝
    wsRequest.appendParam(std::move(param));

    // 记录延迟
    if (enableLatencyStats_ && apiCallStartTime.time_since_epoch().count() > 0) {
      auto sendTime = std::chrono::steady_clock::now();
      auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(sendTime - apiCallStartTime).count();
      std::cout << "[LTPTradingService] createOrderAsync to sendRequestByWebsocket latency: "
                << latencyNs << " ns (" << (latencyNs / 1000.0) << " us)" << std::endl;
    }

    // 直接发送
    session_->sendRequestByWebsocket(wsCorrelationId, wsRequest);
  }

  /**
   * FIX快速路径 - 撤单零拷贝优化（最低延迟）
   */
  inline void cancelOrderAsyncFixFastPath(const LTPCancelOrderRequest& request,
                                         const std::map<std::string, std::string>& credential,
                                         const std::string& correlationId,
                                         const std::string& exchange,
                                         const std::chrono::steady_clock::time_point& apiCallStartTime) {
    // 获取FIX correlationId（已缓存，快速）
    auto fixIt = fixSubscriptionCorrelationIds_.find(exchange);
    if (fixIt == fixSubscriptionCorrelationIds_.end()) {
      // 首次调用，需要建立FIX连接（降级到标准路径）
      Request ccapiRequest = convertToCancelOrderRequest(request, credential);
      if (!correlationId.empty()) {
        ccapiRequest.setCorrelationId(correlationId);
      }
      sendRequestWithBestProtocol(exchange, ccapiRequest, credential, apiCallStartTime);
      return;
    }

    const std::string& fixCorrelationId = fixIt->second;

    // 直接构造Request - 零拷贝优化
    Request fixRequest(Request::Operation::CANCEL_ORDER, exchange, request.symbol,
                      correlationId.empty() ? "" : correlationId,
                      std::map<std::string, std::string>{});  // FIX不需要credential

    // 直接构造参数map - 极致优化版本
    std::map<std::string, std::string> param;

    // 订单ID或客户端订单ID（至少需要一个）
    if (!request.orderId.empty()) {
      param.emplace(CCAPI_EM_ORDER_ID, request.orderId);
    }
    if (!request.clientOrderId.empty()) {
      param.emplace(CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId);
    }

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.emplace(kv.first, kv.second);
    }

    // 使用move避免拷贝
    fixRequest.appendParam(std::move(param));

    // 记录延迟
    if (enableLatencyStats_ && apiCallStartTime.time_since_epoch().count() > 0) {
      auto sendTime = std::chrono::steady_clock::now();
      auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(sendTime - apiCallStartTime).count();
      std::cout << "[LTPTradingService] cancelOrderAsync to sendRequestByFix latency: "
                << latencyNs << " ns (" << (latencyNs / 1000.0) << " us)" << std::endl;
    }

    // 直接发送
    session_->sendRequestByFix(fixCorrelationId, fixRequest);
  }

  /**
   * WebSocket快速路径 - 撤单零拷贝优化
   */
  inline void cancelOrderAsyncWebSocketFastPath(const LTPCancelOrderRequest& request,
                                               const std::map<std::string, std::string>& credential,
                                               const std::string& correlationId,
                                               const std::string& exchange,
                                               const std::chrono::steady_clock::time_point& apiCallStartTime) {
    // 获取WebSocket correlationId（已缓存，快速）
    auto wsIt = websocketSubscriptionCorrelationIds_.find(exchange);
    if (wsIt == websocketSubscriptionCorrelationIds_.end()) {
      // 首次调用，需要建立连接（降级到标准路径）
      Request ccapiRequest = convertToCancelOrderRequest(request, credential);
      if (!correlationId.empty()) {
        ccapiRequest.setCorrelationId(correlationId);
      }
      sendRequestWithBestProtocol(exchange, ccapiRequest, credential, apiCallStartTime);
      return;
    }

    const std::string& wsCorrelationId = wsIt->second;

    // 直接构造Request - 零拷贝优化
    Request wsRequest(Request::Operation::CANCEL_ORDER, exchange, request.symbol,
                     correlationId.empty() ? "" : correlationId,
                     std::map<std::string, std::string>{});  // 空credential

    // 直接构造参数map - 极致优化版本
    std::map<std::string, std::string> param;

    // 订单ID或客户端订单ID（至少需要一个）
    if (!request.orderId.empty()) {
      param.emplace(CCAPI_EM_ORDER_ID, request.orderId);
    }
    if (!request.clientOrderId.empty()) {
      param.emplace(CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId);
    }

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.emplace(kv.first, kv.second);
    }

    // 使用move避免拷贝
    wsRequest.appendParam(std::move(param));

    // 记录延迟
    if (enableLatencyStats_ && apiCallStartTime.time_since_epoch().count() > 0) {
      auto sendTime = std::chrono::steady_clock::now();
      auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(sendTime - apiCallStartTime).count();
      std::cout << "[LTPTradingService] cancelOrderAsync to sendRequestByWebsocket latency: "
                << latencyNs << " ns (" << (latencyNs / 1000.0) << " us)" << std::endl;
    }

    // 直接发送
    session_->sendRequestByWebsocket(wsCorrelationId, wsRequest);
  }

  /**
   * 初始化默认的交易所能力配置
   * 用户可以通过setExchangeCapabilities覆盖这些默认配置
   */
  void initializeDefaultCapabilities() {
    // 币安现货/杠杆（经典账户） - 支持FIX, WebSocket, REST
    ExchangeCapabilities binanceCaps;
    binanceCaps.supportsFix = true;
    binanceCaps.supportsWebsocket = true;
    binanceCaps.supportsRest = true;
    exchangeCapabilities_[CCAPI_EXCHANGE_NAME_BINANCE] = binanceCaps;

    // 币安合约（经典账户） - 支持WebSocket, REST
    ExchangeCapabilities binanceFuturesCaps;
    binanceFuturesCaps.supportsFix = false;
    binanceFuturesCaps.supportsWebsocket = true;
    binanceFuturesCaps.supportsRest = true;
    exchangeCapabilities_[CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES] = binanceFuturesCaps;
    exchangeCapabilities_[CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES] = binanceFuturesCaps;

    // 币安统一账户 - 只支持REST
    ExchangeCapabilities binancePortfolioCaps;
    binancePortfolioCaps.supportsFix = false;
    binancePortfolioCaps.supportsWebsocket = false;
    binancePortfolioCaps.supportsRest = true;
    exchangeCapabilities_[CCAPI_EXCHANGE_NAME_BINANCE_PORTFOLIO_MARGIN] = binancePortfolioCaps;

    // OKX - 支持WebSocket, REST
    ExchangeCapabilities okxCaps;
    okxCaps.supportsFix = false;
    okxCaps.supportsWebsocket = true;
    okxCaps.supportsRest = true;
    exchangeCapabilities_[CCAPI_EXCHANGE_NAME_OKX] = okxCaps;

    // Bybit - 支持WebSocket, REST
    ExchangeCapabilities bybitCaps;
    bybitCaps.supportsFix = false;
    bybitCaps.supportsWebsocket = true;
    bybitCaps.supportsRest = true;
    exchangeCapabilities_[CCAPI_EXCHANGE_NAME_BYBIT] = bybitCaps;
  }

  /**
   * 根据交易所能力选择最优协议发送请求
   * 优先级：FIX > WebSocket > REST
   */
  void sendRequestWithBestProtocol(const std::string& exchange,
                                   Request& request,
                                   const std::map<std::string, std::string>& credential,
                                   const std::chrono::steady_clock::time_point& apiCallStartTime = std::chrono::steady_clock::time_point()) {
    auto it = exchangeCapabilities_.find(exchange);
    ExchangeCapabilities caps;
    if (it != exchangeCapabilities_.end()) {
      caps = it->second;
    } else {
      // 默认只支持REST
      caps.supportsRest = true;
    }

    // 优先级1: FIX协议
    if (caps.supportsFix) {
      std::string correlationId = ensureFixSubscription(exchange, credential);
      session_->sendRequestByFix(correlationId, request);
      return;
    }

    // 优先级2: WebSocket协议
    // WebSocket请求不需要在Request中携带credential，因为credential已经在Subscription中提供
    if (caps.supportsWebsocket) {
      std::string correlationId = ensureWebsocketSubscription(exchange, credential);

      // 清除Request中的credential（WebSocket不需要）
      Request wsRequest = request;
      wsRequest.setCredential({});

      // 记录实际发送时间并计算延迟（仅在启用延迟统计时）
      if (enableLatencyStats_ && apiCallStartTime.time_since_epoch().count() > 0) {
        auto sendTime = std::chrono::steady_clock::now();
        auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(sendTime - apiCallStartTime).count();
        std::cout << "[LTPTradingService] createOrderAsync to sendRequestByWebsocket latency: "
                  << latencyNs << " ns (" << (latencyNs / 1000.0) << " us)" << std::endl;
      }

      session_->sendRequestByWebsocket(correlationId, wsRequest);
      return;
    }

    // 优先级3: REST协议（默认）
    session_->sendRequest(request);
  }

  /**
   * 确保WebSocket订阅存在，如果不存在则创建
   * @return WebSocket订阅的correlationId
   */
  std::string ensureWebsocketSubscription(const std::string& exchange,
                                         const std::map<std::string, std::string>& credential) {
    // 检查是否已经有订阅
    auto it = websocketSubscriptionCorrelationIds_.find(exchange);
    if (it != websocketSubscriptionCorrelationIds_.end()) {
      return it->second;
    }

    // 创建新的WebSocket订阅
    std::string correlationId = "ws_" + exchange + "_" + std::to_string(UtilTime::now().time_since_epoch().count());

    // 为WebSocket订阅准备正确的credential
    // 币安交易所需要使用专门的WebSocket API Key宏
    std::map<std::string, std::string> wsCredential = credential;

    // 币安现货/杠杆（经典账户）
    if (exchange == CCAPI_EXCHANGE_NAME_BINANCE) {
      auto apiKeyIt = credential.find(CCAPI_BINANCE_API_KEY);
      auto apiSecretIt = credential.find(CCAPI_BINANCE_API_SECRET);

      if (apiKeyIt != credential.end() && apiSecretIt != credential.end()) {
        wsCredential.clear();
        wsCredential[CCAPI_BINANCE_WEBSOCKET_ORDER_ENTRY_API_KEY] = apiKeyIt->second;
        wsCredential[CCAPI_BINANCE_API_SECRET] = apiSecretIt->second;
      }
    }
    // 币安U本位合约（经典账户）
    else if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES) {
      auto apiKeyIt = credential.find(CCAPI_BINANCE_USDS_FUTURES_API_KEY);
      auto apiSecretIt = credential.find(CCAPI_BINANCE_USDS_FUTURES_API_SECRET);

      if (apiKeyIt != credential.end() && apiSecretIt != credential.end()) {
        wsCredential.clear();
        wsCredential[CCAPI_BINANCE_USDS_FUTURES_WEBSOCKET_ORDER_ENTRY_API_KEY] = apiKeyIt->second;
        wsCredential[CCAPI_BINANCE_USDS_FUTURES_API_SECRET] = apiSecretIt->second;
      }
    }
    // 币安币本位合约（经典账户）
    else if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES) {
      auto apiKeyIt = credential.find(CCAPI_BINANCE_COIN_FUTURES_API_KEY);
      auto apiSecretIt = credential.find(CCAPI_BINANCE_COIN_FUTURES_API_SECRET);

      if (apiKeyIt != credential.end() && apiSecretIt != credential.end()) {
        wsCredential.clear();
        wsCredential[CCAPI_BINANCE_COIN_FUTURES_WEBSOCKET_ORDER_ENTRY_API_KEY] = apiKeyIt->second;
        wsCredential[CCAPI_BINANCE_COIN_FUTURES_API_SECRET] = apiSecretIt->second;
      }
    }
    // 币安US - 如果也支持WebSocket，需要类似处理
    // 注意：需要确认币安US是否有专门的WebSocket API Key宏

    // Subscription构造函数: (exchange, instrument, field, options, correlationId, credential, proxyUrl)
    Subscription subscription(exchange, "", CCAPI_EM_WEBSOCKET_ORDER_ENTRY, "", correlationId, wsCredential, "");

    session_->subscribe(subscription);

    // 等待订阅被Session处理（给Session时间启用交易所和建立连接）
    // 这是首次连接，需要时间建立WebSocket连接和完成授权
    std::this_thread::sleep_for(std::chrono::seconds(3));

    websocketSubscriptionCorrelationIds_[exchange] = correlationId;
    return correlationId;
  }

  /**
   * 确保FIX订阅存在，如果不存在则创建
   * @return FIX订阅的correlationId
   */
  std::string ensureFixSubscription(const std::string& exchange,
                                   const std::map<std::string, std::string>& credential) {
    // 检查是否已经有订阅
    auto it = fixSubscriptionCorrelationIds_.find(exchange);
    if (it != fixSubscriptionCorrelationIds_.end()) {
      return it->second;
    }

    // 创建新的FIX订阅
    std::string correlationId = "fix_" + exchange + "_" + std::to_string(UtilTime::now().time_since_epoch().count());

    // Subscription构造函数: (exchange, instrument, field, options, correlationId, credential, proxyUrl)
    Subscription subscription(exchange, "", CCAPI_FIX, "", correlationId, credential, "");

    session_->subscribe(subscription);

    // 等待订阅被Session处理（给Session时间启用交易所和建立连接）
    std::this_thread::sleep_for(std::chrono::seconds(3));

    fixSubscriptionCorrelationIds_[exchange] = correlationId;
    return correlationId;
  }

  /**
   * 为订阅准备正确的credential
   * 某些交易所（如币安）的WebSocket订阅需要使用特殊的API Key宏
   */
  std::map<std::string, std::string> prepareSubscriptionCredential(
      const std::string& exchange,
      const std::map<std::string, std::string>& credential) {

    std::map<std::string, std::string> subCredential = credential;

    // 币安现货/杠杆（经典账户）
    if (exchange == CCAPI_EXCHANGE_NAME_BINANCE) {
      auto apiKeyIt = credential.find(CCAPI_BINANCE_API_KEY);
      auto apiSecretIt = credential.find(CCAPI_BINANCE_API_SECRET);

      if (apiKeyIt != credential.end() && apiSecretIt != credential.end()) {
        // 订阅使用原始的API Key和Secret（不需要特殊的WebSocket Key）
        subCredential.clear();
        subCredential[CCAPI_BINANCE_API_KEY] = apiKeyIt->second;
        subCredential[CCAPI_BINANCE_API_SECRET] = apiSecretIt->second;
      }
    }
    // 币安U本位合约（经典账户）
    else if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_USDS_FUTURES) {
      auto apiKeyIt = credential.find(CCAPI_BINANCE_USDS_FUTURES_API_KEY);
      auto apiSecretIt = credential.find(CCAPI_BINANCE_USDS_FUTURES_API_SECRET);

      if (apiKeyIt != credential.end() && apiSecretIt != credential.end()) {
        subCredential.clear();
        subCredential[CCAPI_BINANCE_USDS_FUTURES_API_KEY] = apiKeyIt->second;
        subCredential[CCAPI_BINANCE_USDS_FUTURES_API_SECRET] = apiSecretIt->second;
      }
    }
    // 币安币本位合约（经典账户）
    else if (exchange == CCAPI_EXCHANGE_NAME_BINANCE_COIN_FUTURES) {
      auto apiKeyIt = credential.find(CCAPI_BINANCE_COIN_FUTURES_API_KEY);
      auto apiSecretIt = credential.find(CCAPI_BINANCE_COIN_FUTURES_API_SECRET);

      if (apiKeyIt != credential.end() && apiSecretIt != credential.end()) {
        subCredential.clear();
        subCredential[CCAPI_BINANCE_COIN_FUTURES_API_KEY] = apiKeyIt->second;
        subCredential[CCAPI_BINANCE_COIN_FUTURES_API_SECRET] = apiSecretIt->second;
      }
    }
    // OKX和其他交易所直接使用原始credential

    return subCredential;
  }

  // ====================================================================
  // 内部转换方法 - 将统一请求转换为ccapi Request
  // ====================================================================

  /**
   * 转换创建订单请求
   */
  Request convertToCreateOrderRequest(const LTPCreateOrderRequest& request,
                                      const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::CREATE_ORDER, exchange, request.symbol, "", credential);

    std::map<std::string, std::string> param;

    // 订单方向
    param.insert({CCAPI_EM_ORDER_SIDE, ltp::orderSideToString(request.side)});

    // 订单类型
    // OKX特殊处理：不传递CCAPI_EM_ORDER_TYPE，因为OKX服务会自动将其转换为ordType参数
    if (exchange != CCAPI_EXCHANGE_NAME_OKX) {
      std::string orderTypeStr = ltp::orderTypeToString(request.type);
      param.insert({CCAPI_EM_ORDER_TYPE, orderTypeStr});
    }
    // 注意：OKX的ordType会由ccapi_execution_management_service_okx.h自动添加

    // 数量
    if (!request.quantity.empty()) {
      param.insert({CCAPI_EM_ORDER_QUANTITY, request.quantity});
    }
    if (!request.quoteOrderQty.empty()) {
      param.insert({"quoteOrderQty", request.quoteOrderQty});
    }

    // 价格
    if (!request.price.empty()) {
      param.insert({CCAPI_EM_ORDER_LIMIT_PRICE, request.price});
    }
    if (!request.stopPrice.empty()) {
      param.insert({"stopPrice", request.stopPrice});
    }

    // 客户端订单ID
    if (!request.clientOrderId.empty()) {
      param.insert({CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId});
    }

    // 时间有效性
    // timeInForce只在非市价单时添加
    // 某些交易所和订单类型不支持timeInForce
    if (request.timeInForce != LTPTimeInForce::UNKNOWN && request.type != LTPOrderType::MARKET) {
      // OKX不支持timeInForce参数
      if (exchange != CCAPI_EXCHANGE_NAME_OKX) {
        param.insert({"timeInForce", ltp::timeInForceToString(request.timeInForce)});
      }
    }

    // OKX特殊处理：智能设置tdMode参数
    // tdMode是OKX必需的参数，表示交易模式：
    // - "cash": 现货模式（非保证金交易）
    // - "cross": 全仓保证金模式
    // - "isolated": 逐仓保证金模式
    if (exchange == CCAPI_EXCHANGE_NAME_OKX) {
      // 检查用户是否通过extraParams自定义了tdMode
      auto tdModeIt = request.extraParams.find("tdMode");
      if (tdModeIt == request.extraParams.end()) {
        // 用户未指定tdMode，使用智能判断
        std::string tdMode = "cross";  // 默认使用cross模式（最通用）

        // 根据交易对类型判断
        if (request.symbol.find("-SWAP") != std::string::npos ||
            request.symbol.find("-FUTURES") != std::string::npos) {
          // 永续合约或交割合约：使用cross模式
          tdMode = "cross";
        } else {
          // 现货交易对：理论上应该使用cash模式
          // 但某些账户配置（如统一账户）可能需要使用cross模式
          // 保守起见，默认使用cross模式
          // 用户可以通过 request.extraParams["tdMode"] = "cash" 覆盖
          tdMode = "cross";
        }

        param.insert({"tdMode", tdMode});
      }
      // 如果用户已通过extraParams指定tdMode，则会在后面的extraParams循环中添加
    }

    // 高级选项
    if (request.reduceOnly) {
      param.insert({"reduceOnly", "true"});
    }
    if (request.postOnly) {
      param.insert({"postOnly", "true"});
    }
    if (!request.leverage.empty()) {
      param.insert({CCAPI_EM_ORDER_LEVERAGE, request.leverage});
    }
    if (!request.marginMode.empty()) {
      param.insert({CCAPI_MARGIN_MODE, request.marginMode});
    }
    if (!request.positionSide.empty()) {
      param.insert({"positionSide", request.positionSide});
    }

    // 额外参数（最后添加，可以覆盖默认值）
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    ccapiRequest.appendParam(param);
    return ccapiRequest;
  }

  /**
   * 转换取消订单请求
   */
  Request convertToCancelOrderRequest(const LTPCancelOrderRequest& request,
                                     const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::CANCEL_ORDER, exchange, request.symbol, "", credential);

    std::map<std::string, std::string> param;

    if (!request.orderId.empty()) {
      param.insert({CCAPI_EM_ORDER_ID, request.orderId});
    }
    if (!request.clientOrderId.empty()) {
      param.insert({CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId});
    }

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    ccapiRequest.appendParam(param);
    return ccapiRequest;
  }

  /**
   * 转换取消所有订单请求
   */
  Request convertToCancelAllOrdersRequest(const LTPCancelAllOrdersRequest& request,
                                         const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::CANCEL_OPEN_ORDERS, exchange, request.symbol, "", credential);

    std::map<std::string, std::string> param;

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    if (!param.empty()) {
      ccapiRequest.appendParam(param);
    }

    return ccapiRequest;
  }

  /**
   * 转换查询订单请求
   */
  Request convertToGetOrderRequest(const LTPGetOrderRequest& request,
                                   const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::GET_ORDER, exchange, request.symbol, "", credential);

    std::map<std::string, std::string> param;

    if (!request.orderId.empty()) {
      param.insert({CCAPI_EM_ORDER_ID, request.orderId});
    }
    if (!request.clientOrderId.empty()) {
      param.insert({CCAPI_EM_CLIENT_ORDER_ID, request.clientOrderId});
    }

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    ccapiRequest.appendParam(param);
    return ccapiRequest;
  }

  /**
   * 转换查询开放订单请求
   */
  Request convertToGetOpenOrdersRequest(const LTPGetOpenOrdersRequest& request,
                                       const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::GET_OPEN_ORDERS, exchange, request.symbol, "", credential);

    std::map<std::string, std::string> param;

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    if (!param.empty()) {
      ccapiRequest.appendParam(param);
    }

    return ccapiRequest;
  }

  /**
   * 转换查询账户余额请求
   */
  Request convertToGetAccountBalancesRequest(const LTPGetAccountBalancesRequest& request,
                                            const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::GET_ACCOUNT_BALANCES, exchange, "", "", credential);

    std::map<std::string, std::string> param;

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    if (!param.empty()) {
      ccapiRequest.appendParam(param);
    }

    return ccapiRequest;
  }

  /**
   * 转换查询持仓请求
   */
  Request convertToGetAccountPositionsRequest(const LTPGetAccountPositionsRequest& request,
                                             const std::map<std::string, std::string>& credential) {
    std::string exchange = ltp::exchangeToString(request.exchange);
    Request ccapiRequest(Request::Operation::GET_ACCOUNT_POSITIONS, exchange, request.symbol, "", credential);

    std::map<std::string, std::string> param;

    // 额外参数
    for (const auto& kv : request.extraParams) {
      param.insert(kv);
    }

    if (!param.empty()) {
      ccapiRequest.appendParam(param);
    }

    return ccapiRequest;
  }

  /**
   * 执行同步请求
   */
  LTPResponse executeRequestSync(Request& request) {
    Queue<Event> eventQueue;
    session_->sendRequest(request, &eventQueue);

    std::vector<Event> eventList = eventQueue.purge();
    if (!eventList.empty()) {
      return convertEventToResponse(eventList[0]);
    }

    LTPResponse response;
    response.success = false;
    response.errorMessage = "No response received";
    return response;
  }

  /**
   * 将Element转换为LTPOrderInfo
   */
  static LTPOrderInfo convertElementToOrderInfo(const Element& element) {
    LTPOrderInfo orderInfo;

    orderInfo.orderId = element.getValue(CCAPI_EM_ORDER_ID);
    orderInfo.clientOrderId = element.getValue(CCAPI_EM_CLIENT_ORDER_ID);
    orderInfo.symbol = element.getValue(CCAPI_EM_ORDER_INSTRUMENT);

    // 订单方向
    std::string sideStr = element.getValue(CCAPI_EM_ORDER_SIDE);
    orderInfo.side = ltp::stringToOrderSide(sideStr);

    // 订单类型
    std::string typeStr = element.getValue(CCAPI_EM_ORDER_TYPE);
    orderInfo.type = ltp::stringToOrderType(typeStr);

    // 订单状态
    std::string statusStr = element.getValue(CCAPI_EM_ORDER_STATUS);
    orderInfo.status = ltp::stringToOrderStatus(statusStr);

    // 时间有效性
    std::string tifStr = element.getValue("timeInForce");
    if (!tifStr.empty()) {
      orderInfo.timeInForce = ltp::stringToTimeInForce(tifStr);
    }

    // 价格和数量
    orderInfo.price = element.getValue(CCAPI_EM_ORDER_LIMIT_PRICE);
    orderInfo.quantity = element.getValue(CCAPI_EM_ORDER_QUANTITY);
    orderInfo.executedQty = element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY);
    orderInfo.cumulativeQuoteQty = element.getValue(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUOTE_QUANTITY);
    orderInfo.avgPrice = element.getValue(CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE);

    // 触发价格
    orderInfo.stopPrice = element.getValue("stopPrice");

    // 时间信息
    orderInfo.createTime = element.getValue("createTime");
    orderInfo.updateTime = element.getValue("updateTime");

    // 费用信息
    orderInfo.commission = element.getValue(CCAPI_EM_ORDER_FEE_QUANTITY);
    orderInfo.commissionAsset = element.getValue(CCAPI_EM_ORDER_FEE_ASSET);

    // 将所有字段存入extraInfo以便调试
    for (const auto& kv : element.getNameValueMap()) {
      orderInfo.extraInfo.insert(kv);
    }

    return orderInfo;
  }

  /**
   * 将Element转换为LTPBalanceInfo
   */
  static LTPBalanceInfo convertElementToBalanceInfo(const Element& element) {
    LTPBalanceInfo balanceInfo;

    balanceInfo.asset = element.getValue(CCAPI_EM_ASSET);
    balanceInfo.availableBalance = element.getValue(CCAPI_EM_QUANTITY_AVAILABLE_FOR_TRADING);
    balanceInfo.totalBalance = element.getValue(CCAPI_EM_QUANTITY_TOTAL);

    // 计算冻结余额（如果有的话）
    if (!balanceInfo.totalBalance.empty() && !balanceInfo.availableBalance.empty()) {
      try {
        double total = std::stod(balanceInfo.totalBalance);
        double available = std::stod(balanceInfo.availableBalance);
        double frozen = total - available;
        if (frozen > 0) {
          balanceInfo.frozenBalance = std::to_string(frozen);
        }
      } catch (...) {
        // 如果转换失败，忽略
      }
    }

    // 将所有字段存入extraInfo以便调试
    for (const auto& kv : element.getNameValueMap()) {
      balanceInfo.extraInfo.insert(kv);
    }

    return balanceInfo;
  }

  /**
   * 将Element转换为LTPPositionInfo
   */
  static LTPPositionInfo convertElementToPositionInfo(const Element& element) {
    LTPPositionInfo positionInfo;

    positionInfo.symbol = element.getValue(CCAPI_INSTRUMENT);
    positionInfo.positionSide = element.getValue(CCAPI_EM_POSITION_SIDE);
    positionInfo.positionAmount = element.getValue(CCAPI_EM_POSITION_QUANTITY);
    positionInfo.entryPrice = element.getValue(CCAPI_EM_POSITION_ENTRY_PRICE);
    positionInfo.unrealizedProfit = element.getValue(CCAPI_EM_UNREALIZED_PNL);
    positionInfo.leverage = element.getValue(CCAPI_EM_POSITION_LEVERAGE);
    positionInfo.marginType = element.getValue(CCAPI_EM_POSITION_MARGIN_TYPE);

    // 将所有字段存入extraInfo以便调试
    for (const auto& kv : element.getNameValueMap()) {
      positionInfo.extraInfo.insert(kv);
    }

    return positionInfo;
    return positionInfo;
  }
};

} /* namespace ltp */

#endif  // INCLUDE_LTP_TRADING_SERVICE_H_


