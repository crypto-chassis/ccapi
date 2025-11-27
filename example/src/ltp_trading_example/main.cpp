#include "ccapi_cpp/ccapi_session.h"
#include "ccapi_cpp/ccapi_ltp_trading_interface.h"
#include "ccapi_cpp/ccapi_ltp_trading_service.h"
#include <iostream>
#include <chrono>

using namespace ccapi;  // ccapi的基础类型：Session, EventHandler等
using namespace ltp;    // LTP的交易接口类型

class ConnectivityTestHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* session) override {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // 转换为统一响应
    LTPResponse response = LTPTradingService::convertEventToResponse(event);

    std::cout << "\n[" << timestamp << "] ========== 收到事件 ==========" << std::endl;
    std::cout << "事件类型: " << response.eventTypeString << std::endl;
    std::cout << "消息类型: " << response.messageTypeString << std::endl;

    // 打印原始事件（用于调试）
    if (response.eventType == LTPEventType::SUBSCRIPTION_DATA ||
        response.eventType == LTPEventType::RESPONSE) {
      std::cout << "\n📋 原始事件内容:" << std::endl;
      std::cout << event.toPrettyString(2, 0) << std::endl;
    }

    // 根据事件类型处理
    if (response.eventType == LTPEventType::RESPONSE) {
      // 交易响应
      if (response.success) {
        std::cout << "✅ 操作成功!" << std::endl;

        // 处理账户余额响应
        if (!response.balances.empty()) {
          std::cout << "📊 账户余额:" << std::endl;
          for (const auto& balance : response.balances) {
            std::cout << "   资产: " << balance.asset << std::endl;
            std::cout << "   可用余额: " << balance.availableBalance << std::endl;
            std::cout << "   总余额: " << balance.totalBalance << std::endl;
            if (!balance.frozenBalance.empty()) {
              std::cout << "   冻结余额: " << balance.frozenBalance << std::endl;
            }
            std::cout << "   ---" << std::endl;
          }

          // 打印原始交易所响应
          if (!response.rawResponse.empty()) {
            std::cout << "\n📄 原始交易所响应:" << std::endl;
            std::cout << response.rawResponse << std::endl;
          }
        }
        // 处理持仓响应
        else if (!response.positions.empty()) {
          std::cout << "📊 持仓信息 (数量: " << response.positions.size() << "):" << std::endl;
          for (const auto& position : response.positions) {
            std::cout << "   交易对: " << position.symbol << std::endl;
            std::cout << "   持仓方向: " << position.positionSide << std::endl;
            std::cout << "   持仓数量: " << position.positionAmount << std::endl;
            std::cout << "   开仓均价: " << position.entryPrice << std::endl;
            if (!position.unrealizedProfit.empty()) {
              std::cout << "   未实现盈亏: " << position.unrealizedProfit << std::endl;
            }
            if (!position.leverage.empty()) {
              std::cout << "   杠杆倍数: " << position.leverage << std::endl;
            }
            if (!position.marginType.empty()) {
              std::cout << "   保证金模式: " << position.marginType << std::endl;
            }
            std::cout << "   ---" << std::endl;
          }

          // 打印原始交易所响应
          if (!response.rawResponse.empty()) {
            std::cout << "\n📄 原始交易所响应:" << std::endl;
            std::cout << response.rawResponse << std::endl;
          }
        }
        // 处理开放订单响应
        else if (!response.orders.empty()) {
          std::cout << "📊 开放订单 (数量: " << response.orders.size() << "):" << std::endl;
          for (const auto& order : response.orders) {
            std::cout << "   订单ID: " << order.orderId << std::endl;
            std::cout << "   客户端订单ID: " << order.clientOrderId << std::endl;
            std::cout << "   交易对: " << order.symbol << std::endl;
            std::cout << "   方向: " << (order.side == LTPOrderSide::BUY ? "BUY" : "SELL") << std::endl;
            std::cout << "   类型: ";
            switch (order.type) {
              case LTPOrderType::LIMIT:
                std::cout << "LIMIT" << std::endl;
                break;
              case LTPOrderType::MARKET:
                std::cout << "MARKET" << std::endl;
                break;
              default:
                std::cout << "OTHER" << std::endl;
            }
            std::cout << "   价格: " << order.price << std::endl;
            std::cout << "   数量: " << order.quantity << std::endl;
            std::cout << "   已成交数量: " << order.executedQty << std::endl;
            std::cout << "   订单状态: ";
            switch (order.status) {
              case LTPOrderStatus::NEW:
                std::cout << "NEW (新建)" << std::endl;
                break;
              case LTPOrderStatus::PARTIALLY_FILLED:
                std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
                break;
              case LTPOrderStatus::FILLED:
                std::cout << "FILLED (完全成交)" << std::endl;
                break;
              case LTPOrderStatus::CANCELED:
                std::cout << "CANCELED (已取消)" << std::endl;
                break;
              default:
                std::cout << "UNKNOWN" << std::endl;
            }
            std::cout << "   ---" << std::endl;
          }

          // 打印原始交易所响应
          if (!response.rawResponse.empty()) {
            std::cout << "\n📄 原始交易所响应:" << std::endl;
            std::cout << response.rawResponse << std::endl;
          }
        }
        // 处理订单响应
        else if (!response.orderInfo.orderId.empty()) {
          std::cout << "订单ID: " << response.orderInfo.orderId << std::endl;
          std::cout << "客户端订单ID: " << response.orderInfo.clientOrderId << std::endl;

          // 只有当交易对不为空时才显示
          if (!response.orderInfo.symbol.empty()) {
            std::cout << "交易对: " << response.orderInfo.symbol << std::endl;
          }

          // 只有当状态不是UNKNOWN时才显示订单状态
          // OKX WebSocket下单/撤单响应不包含状态字段，这是正常的
          if (response.orderInfo.status != LTPOrderStatus::UNKNOWN) {
            std::cout << "订单状态: ";
            switch (response.orderInfo.status) {
              case LTPOrderStatus::NEW:
                std::cout << "NEW (新建)" << std::endl;
                break;
              case LTPOrderStatus::FILLED:
                std::cout << "FILLED (完全成交)" << std::endl;
                break;
              case LTPOrderStatus::PARTIALLY_FILLED:
                std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
                break;
              case LTPOrderStatus::CANCELED:
                std::cout << "CANCELED (已取消)" << std::endl;
                break;
              default:
                break;
            }
          }

          // 保存订单ID供撤单使用
          lastOrderId = response.orderInfo.orderId;
          lastClientOrderId = response.orderInfo.clientOrderId;
        }
      } else {
        std::cout << "❌ 操作失败!" << std::endl;
        std::cout << "错误码: " << response.errorCode << std::endl;
        std::cout << "错误信息: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == LTPEventType::SESSION_STATUS) {
      // 会话状态事件
      if (response.messageType == LTPMessageType::SESSION_CONNECTION_UP) {
        std::cout << "✅ WebSocket连接已建立" << std::endl;
      } else if (response.messageType == LTPMessageType::SESSION_CONNECTION_DOWN) {
        std::cout << "⚠️  WebSocket连接已断开" << std::endl;
      } else {
        std::cout << "ℹ️  会话状态: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == LTPEventType::AUTHORIZATION_STATUS) {
      // 授权状态事件
      if (response.messageType == LTPMessageType::AUTHORIZATION_SUCCESS) {
        std::cout << "✅ WebSocket授权成功" << std::endl;
      } else if (response.messageType == LTPMessageType::AUTHORIZATION_FAILURE) {
        std::cout << "❌ WebSocket授权失败: " << response.errorMessage << std::endl;
      } else {
        std::cout << "ℹ️  授权状态: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == LTPEventType::SUBSCRIPTION_STATUS) {
      // 订阅状态事件
      if (response.messageType == LTPMessageType::SUBSCRIPTION_STARTED) {
        std::cout << "✅ 订阅已启动" << std::endl;
      } else if (response.messageType == LTPMessageType::SUBSCRIPTION_FAILURE) {
        std::cout << "❌ 订阅失败: " << response.errorMessage << std::endl;
      } else if (response.messageType == LTPMessageType::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE) {
        std::cout << "⚠️  订阅失败（连接问题）: " << response.errorMessage << std::endl;
      } else {
        std::cout << "ℹ️  订阅状态: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == LTPEventType::SUBSCRIPTION_DATA) {
      // 订阅数据（实时推送的订单更新、成交等）
      std::cout << "📊 收到订阅数据推送" << std::endl;
      if (!response.orderInfo.orderId.empty()) {
        std::cout << "订单ID: " << response.orderInfo.orderId << std::endl;
        std::cout << "客户端订单ID: " << response.orderInfo.clientOrderId << std::endl;
        std::cout << "交易对: " << response.orderInfo.symbol << std::endl;

        // ⭐ 这里可以获取到完整的订单状态！
        std::cout << "⭐ 订单状态: ";
        switch (response.orderInfo.status) {
          case LTPOrderStatus::NEW:
            std::cout << "NEW (新建)" << std::endl;
            break;
          case LTPOrderStatus::FILLED:
            std::cout << "FILLED (完全成交)" << std::endl;
            break;
          case LTPOrderStatus::PARTIALLY_FILLED:
            std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
            break;
          case LTPOrderStatus::CANCELED:
            std::cout << "CANCELED (已取消)" << std::endl;
            break;
          default:
            // 打印原始状态值以便调试
            std::cout << "UNKNOWN";
            if (!response.orderInfo.extraInfo.empty()) {
              auto it = response.orderInfo.extraInfo.find("ORDER_STATUS");
              if (it != response.orderInfo.extraInfo.end()) {
                std::cout << " (原始值: " << it->second << ")";
              }
            }
            std::cout << std::endl;
        }

        // 显示价格和数量信息
        if (!response.orderInfo.price.empty()) {
          std::cout << "价格: " << response.orderInfo.price << std::endl;
        }
        if (!response.orderInfo.quantity.empty()) {
          std::cout << "数量: " << response.orderInfo.quantity << std::endl;
        }
        if (!response.orderInfo.executedQty.empty()) {
          std::cout << "已成交数量: " << response.orderInfo.executedQty << std::endl;
        }
      }
    }
    else if (response.eventType == LTPEventType::REQUEST_STATUS) {
      // 请求状态事件
      std::cout << "ℹ️  请求状态: " << response.errorMessage << std::endl;
    }
    else if (response.eventType == LTPEventType::FIX || response.eventType == LTPEventType::FIX_STATUS) {
      // FIX协议事件
      std::cout << "ℹ️  FIX事件: " << response.errorMessage << std::endl;
    }
    else if (response.eventType == LTPEventType::HEARTBEAT) {
      // 心跳事件（通常不需要打印，避免刷屏）
      // std::cout << "💓 心跳" << std::endl;
    }
    else {
      // 其他事件
      std::cout << "ℹ️  " << (response.success ? "✅" : "❌") << " " << response.errorMessage << std::endl;
    }

    std::cout << "========================================\n" << std::endl;
  }

  std::string lastOrderId;
  std::string lastClientOrderId;
};

int main(int argc, char** argv) {
  // 启用ccapi日志以查看实际发送的消息
  std::cout << "启用ccapi调试日志..." << std::endl;

  std::cout << "\n========================================" << std::endl;
  std::cout << "统一交易接口连通性测试" << std::endl;
  std::cout << "========================================\n" << std::endl;

  // 1. 创建Session（启用日志）
  SessionOptions sessionOptions;
  sessionOptions.enableCheckPingPongWebsocketApplicationLevel = false;  // 减少日志噪音
  SessionConfigs sessionConfigs;
  ConnectivityTestHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);

  // 2. 创建统一交易服务
  // 选项1: 使用默认的 ccapi WebSocket（boost.beast）
  // LTPTradingService tradingService(&session);

  // 选项2: 使用 LTP 适配器替代 WebSocket（推荐用于低延迟场景）
  // 参数说明：
  //   - useLTPAdapter: true 表示使用 LTP 适配器，false 表示使用默认 WebSocket
  //   - orderPubTopic: LTP 订单发布主题（发送订单请求），默认 "bf0_order_sub"
  //   - orderSubTopic: LTP 订单订阅主题（接收订单响应），默认 "bf0_order_pub"
  bool useLTPAdapter = true;  // 设置为 true 启用 LTP 适配器
  std::string orderPubTopic = "bf0_order_sub";  // 根据您的实际配置修改
  std::string orderSubTopic = "bf0_order_pub";  // 根据您的实际配置修改

  LTPTradingService tradingService(&session, useLTPAdapter, orderPubTopic, orderSubTopic);

  if (useLTPAdapter) {
    std::cout << "✅ 已启用 LTP WebSocket 适配器" << std::endl;
    std::cout << "   发布主题: " << orderPubTopic << std::endl;
    std::cout << "   订阅主题: " << orderSubTopic << std::endl;
  } else {
    std::cout << "ℹ️  使用默认 ccapi WebSocket (boost.beast)" << std::endl;
  }

  // 3. 准备认证信息
  std::map<std::string, std::string> credential;

  // ====================================================================
  // 测试1: 币安统一账户（Portfolio Margin）U本位合约 - REST协议
  // ====================================================================
  if (0)
  {
    std::cout << "测试1: 币安统一账户 U本位合约 (REST)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    const char* pmApiKey = "ZU62l6Zk1sNMnCbTc329jUsvDlfbWz2KWC8fmlSMcjdLMqfninAxEtmfRJ41E2Um";
    const char* pmApiSecret = "jd80wDi7E02j0art7GYrI2af7iEiwE3SQzpqk7fVzmbTmcqZxo0O05eoThlyLMpH";

    if (pmApiKey && pmApiSecret) {
      std::cout << "API Key: " << std::string(pmApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: REST (统一账户只支持REST)" << std::endl;
      std::cout << "交易对: USDCUSDT" << std::endl;
      std::cout << "方向: BUY" << std::endl;
      std::cout << "类型: LIMIT" << std::endl;
      std::cout << "数量: 10" << std::endl;
      std::cout << "价格: 0.92 USDT (远离市价)" << std::endl;

      credential.clear();
      credential[CCAPI_BINANCE_PORTFOLIO_MARGIN_API_KEY] = pmApiKey;
      credential[CCAPI_BINANCE_PORTFOLIO_MARGIN_API_SECRET] = pmApiSecret;

      LTPCreateOrderRequest request;
      request.exchange = LTPExchange::BINANCE_PORTFOLIO_MARGIN;
      request.symbol = "USDCUSDT";
      request.side = LTPOrderSide::BUY;
      request.type = LTPOrderType::LIMIT;
      request.quantity = "10";
      request.price = "0.92";
      request.timeInForce = LTPTimeInForce::GTC;

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      request.clientOrderId = "test_pm_" + std::to_string(timestamp);

      std::cout << "\n[步骤1] 发送下单请求..." << std::endl;
      tradingService.createOrderAsync(request, credential, "pm-create-order");
      std::cout << "请求已发送，等待响应...\n" << std::endl;

      // 等待下单响应
      std::this_thread::sleep_for(std::chrono::seconds(3));

      // 撤单测试
      if (!eventHandler.lastOrderId.empty()) {
        std::cout << "\n[步骤2] 发送撤单请求..." << std::endl;
        std::cout << "撤销订单ID: " << eventHandler.lastOrderId << std::endl;

        LTPCancelOrderRequest cancelRequest;
        cancelRequest.exchange = LTPExchange::BINANCE_PORTFOLIO_MARGIN;
        cancelRequest.symbol = "USDCUSDT";
        cancelRequest.orderId = eventHandler.lastOrderId;

        tradingService.cancelOrderAsync(cancelRequest, credential, "pm-cancel-order");
        std::cout << "撤单请求已发送，等待响应...\n" << std::endl;

        // 等待撤单响应
        std::this_thread::sleep_for(std::chrono::seconds(3));
      } else {
        std::cout << "\n⚠️  跳过撤单: 未获取到订单ID\n" << std::endl;
      }
    } else {
      std::cout << "⚠️  跳过测试1: 未设置环境变量 BINANCE_PM_API_KEY 和 BINANCE_PM_API_SECRET\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试2: 币安经典账户 U本位合约 - WebSocket协议
  // ====================================================================
  if (0)
  {
    std::cout << "\n测试2: 币安经典账户 U本位合约 (WebSocket)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    const char* futuresApiKey = "hplus70npfafWR87cFC9V0mgfitu3t9oDxfg5kDCVy9OhUfI5VCJiQ17uFnRKCYB";
    const char* futuresApiSecret = "ztBWlo3PrHbFDxCv11kdRzCBF4v4EgkLwQeDUfx5pmwau9ax3cTkgJBKNCFOlOxa";

    if (futuresApiKey && futuresApiSecret) {
      std::cout << "API Key: " << std::string(futuresApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: WebSocket (经典账户支持WebSocket)" << std::endl;
      std::cout << "交易对: USDCUSDT" << std::endl;
      std::cout << "方向: BUY" << std::endl;
      std::cout << "类型: LIMIT" << std::endl;
      std::cout << "数量: 10 USDC" << std::endl;
      std::cout << "价格: 0.9 USDT (远离市价)" << std::endl;

      credential.clear();
      credential[CCAPI_BINANCE_USDS_FUTURES_API_KEY] = futuresApiKey;
      credential[CCAPI_BINANCE_USDS_FUTURES_API_SECRET] = futuresApiSecret;

      LTPCreateOrderRequest request;
      request.exchange = LTPExchange::BINANCE_USDS_FUTURES;
      request.symbol = "USDCUSDT";
      request.side = LTPOrderSide::BUY;
      request.type = LTPOrderType::LIMIT;
      request.quantity = "10";
      request.price = "0.9";
      request.timeInForce = LTPTimeInForce::GTC;

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      request.clientOrderId = "test_futures_" + std::to_string(timestamp);

      // 清空上一个测试的订单ID
      eventHandler.lastOrderId.clear();
      eventHandler.lastClientOrderId.clear();

      std::cout << "\n[步骤1] 发送下单请求..." << std::endl;
      std::cout << "注意: WebSocket首次连接需要时间建立和授权..." << std::endl;
      tradingService.createOrderAsync(request, credential, "futures-create-order");
      std::cout << "请求已发送，等待响应...\n" << std::endl;

      // 等待下单响应（WebSocket首次连接需要更长时间）
      std::this_thread::sleep_for(std::chrono::seconds(5));

      // 撤单测试
      if (!eventHandler.lastOrderId.empty()) {
        std::cout << "\n[步骤2] 发送撤单请求..." << std::endl;
        std::cout << "撤销订单ID: " << eventHandler.lastOrderId << std::endl;

        LTPCancelOrderRequest cancelRequest;
        cancelRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;
        cancelRequest.symbol = "USDCUSDT";
        cancelRequest.orderId = eventHandler.lastOrderId;

        tradingService.cancelOrderAsync(cancelRequest, credential, "futures-cancel-order");
        std::cout << "撤单请求已发送，等待响应...\n" << std::endl;

        // 等待撤单响应
        std::this_thread::sleep_for(std::chrono::seconds(3));
      } else {
        std::cout << "\n⚠️  跳过撤单: 未获取到订单ID\n" << std::endl;
      }
    } else {
      std::cout << "⚠️  跳过测试2: 未设置环境变量 BINANCE_FUTURES_API_KEY 和 BINANCE_FUTURES_API_SECRET\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试3: OKX现货 - REST API 下单 + 撤单
  // ====================================================================
  if (0)
  {
    std::cout << "\n测试3: OKX - REST API" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* okxApiKey = "9a746984-40e6-492b-bdb6-5024877ecf72";
    const char* okxApiSecret = "AA2AA1ABC5F7B08A52C342F719F4F209";
    const char* okxApiPassphrase = "%ug8wYpFABJM%A0A";

    if (okxApiKey && okxApiSecret && okxApiPassphrase) {
      std::cout << "API Key: " << std::string(okxApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: REST" << std::endl;
      std::cout << "交易对: USDC-USDT (现货)" << std::endl;
      std::cout << "方向: BUY" << std::endl;
      std::cout << "类型: LIMIT" << std::endl;
      std::cout << "数量: 10 USDC" << std::endl;
      std::cout << "价格: 0.9 USDT (远离市价)\n" << std::endl;

      credential.clear();
      credential[CCAPI_OKX_API_KEY] = okxApiKey;
      credential[CCAPI_OKX_API_SECRET] = okxApiSecret;
      credential[CCAPI_OKX_API_PASSPHRASE] = okxApiPassphrase;

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      LTPCreateOrderRequest request;
      request.exchange = LTPExchange::OKX;
      request.symbol = "USDC-USDT";
      request.side = LTPOrderSide::BUY;
      request.type = LTPOrderType::LIMIT;
      request.quantity = "10";
      request.price = "0.9";
      request.clientOrderId = "rest" + std::to_string(timestamp % 10000000000);
      // tdMode会由统一接口自动添加, 默认cross（保证金模式,全仓）. 客户根据自身需要更新该参数

      std::cout << "[步骤1] REST下单..." << std::endl;
      LTPResponse createResponse = tradingService.createOrder(request, credential);

      if (createResponse.success) {
        std::cout << "✅ REST下单成功!" << std::endl;
        std::cout << "   订单ID: " << createResponse.orderInfo.orderId << std::endl;
        std::cout << "   客户端订单ID: " << createResponse.orderInfo.clientOrderId << std::endl;

        // 等待一下，确保订单已经在交易所系统中
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // REST撤单
        std::cout << "\n[步骤2] REST撤单..." << std::endl;
        LTPCancelOrderRequest cancelRequest;
        cancelRequest.exchange = LTPExchange::OKX;
        cancelRequest.symbol = "USDC-USDT";
        cancelRequest.orderId = createResponse.orderInfo.orderId;

        LTPResponse cancelResponse = tradingService.cancelOrder(cancelRequest, credential);

        if (cancelResponse.success) {
          std::cout << "✅ REST撤单成功!" << std::endl;
          std::cout << "   订单ID: " << cancelResponse.orderInfo.orderId << std::endl;
        } else {
          std::cout << "❌ REST撤单失败: " << cancelResponse.errorMessage << std::endl;
        }
      } else {
        std::cout << "❌ REST下单失败: " << createResponse.errorMessage << std::endl;
      }
    } else {
      std::cout << "⚠️  跳过测试3: 未设置OKX API凭证\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试4: OKX现货 - WebSocket API 下单 + 撤单
  // ====================================================================
  if (0)
  {
    std::cout << "\n测试4: OKX - WebSocket API" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* okxApiKey = "9a746984-40e6-492b-bdb6-5024877ecf72";
    const char* okxApiSecret = "AA2AA1ABC5F7B08A52C342F719F4F209";
    const char* okxApiPassphrase = "%ug8wYpFABJM%A0A";

    if (okxApiKey && okxApiSecret && okxApiPassphrase) {
      std::cout << "API Key: " << std::string(okxApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: WebSocket" << std::endl;
      std::cout << "交易对: USDC-USDT (现货)" << std::endl;
      std::cout << "方向: BUY" << std::endl;
      std::cout << "类型: LIMIT" << std::endl;
      std::cout << "数量: 10 USDC" << std::endl;
      std::cout << "价格: 0.9 USDT (远离市价)\n" << std::endl;

      credential.clear();
      credential[CCAPI_OKX_API_KEY] = okxApiKey;
      credential[CCAPI_OKX_API_SECRET] = okxApiSecret;
      credential[CCAPI_OKX_API_PASSPHRASE] = okxApiPassphrase;

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      LTPCreateOrderRequest request;
      request.exchange = LTPExchange::OKX;
      request.symbol = "USDC-USDT";
      request.side = LTPOrderSide::BUY;
      request.type = LTPOrderType::LIMIT;
      request.quantity = "10";
      request.price = "0.9";
      request.clientOrderId = "ws" + std::to_string(timestamp % 10000000000);

      // tdMode会由统一接口自动添加，默认为"cross"（保证金模式，全仓）
      // 客户可根据自身需要通过extraParams修改该参数：
      // request.extraParams["tdMode"] = "cash";      // 现货模式（非保证金）
      // request.extraParams["tdMode"] = "cross";     // 全仓保证金模式（默认）
      // request.extraParams["tdMode"] = "isolated";  // 逐仓保证金模式

      // ordType 默认是limit的
      // 客户可以根据自身需要通过extraParams修改该参数：
      // request.extraParams["ordType"] = "market";  // 市价单
      // request.extraParams["ordType"] = "limit";   // 限价单

      // 清空订单ID
      eventHandler.lastOrderId.clear();
      eventHandler.lastClientOrderId.clear();

      std::cout << "[步骤1] WebSocket下单..." << std::endl;
      std::cout << "注意: WebSocket首次连接需要时间建立和授权..." << std::endl;
      tradingService.createOrderAsync(request, credential, "okx-ws-create");

      // 等待WebSocket连接、授权和下单响应
      std::this_thread::sleep_for(std::chrono::seconds(6));

      // WebSocket撤单
      if (!eventHandler.lastOrderId.empty()) {
        std::cout << "\n[步骤2] WebSocket撤单..." << std::endl;
        std::cout << "撤销订单ID: " << eventHandler.lastOrderId << std::endl;

        LTPCancelOrderRequest cancelRequest;
        cancelRequest.exchange = LTPExchange::OKX;
        cancelRequest.symbol = "USDC-USDT";
        cancelRequest.orderId = eventHandler.lastOrderId;

        tradingService.cancelOrderAsync(cancelRequest, credential, "okx-ws-cancel");

        // 等待撤单响应
        std::this_thread::sleep_for(std::chrono::seconds(3));
      } else {
        std::cout << "\n⚠️  跳过WebSocket撤单: 未获取到订单ID" << std::endl;
        std::cout << "可能原因: WebSocket连接未建立或下单失败\n" << std::endl;
      }
    } else {
      std::cout << "⚠️  跳过测试4: 未设置OKX API凭证\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试5: OKX永续合约 - WebSocket API 下单 + 撤单
  // ====================================================================
  if (0)
  {
    std::cout << "\n测试5: OKX永续合约 - WebSocket API" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* okxApiKey = "9a746984-40e6-492b-bdb6-5024877ecf72";
    const char* okxApiSecret = "AA2AA1ABC5F7B08A52C342F719F4F209";
    const char* okxApiPassphrase = "%ug8wYpFABJM%A0A";

    if (okxApiKey && okxApiSecret && okxApiPassphrase) {
      std::cout << "API Key: " << std::string(okxApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: WebSocket" << std::endl;
      std::cout << "交易对: USDC-USDT-SWAP (USDC永续合约)" << std::endl;
      std::cout << "方向: BUY" << std::endl;
      std::cout << "类型: LIMIT" << std::endl;
      std::cout << "数量: 1 张合约" << std::endl;
      std::cout << "价格: 0.9 USDT (远离市价)\n" << std::endl;

      credential.clear();
      credential[CCAPI_OKX_API_KEY] = okxApiKey;
      credential[CCAPI_OKX_API_SECRET] = okxApiSecret;
      credential[CCAPI_OKX_API_PASSPHRASE] = okxApiPassphrase;

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      LTPCreateOrderRequest request;
      request.exchange = LTPExchange::OKX;
      request.symbol = "USDC-USDT-SWAP";  // USDC永续合约
      request.side = LTPOrderSide::BUY;
      request.type = LTPOrderType::LIMIT;
      request.quantity = "1";  // 合约张数
      request.price = "0.9";
      request.clientOrderId = "swap" + std::to_string(timestamp % 10000000000);

      // 合约交易：tdMode会自动设置为"cross"（全仓）
      // 如需逐仓模式，可使用：
      // request.extraParams["tdMode"] = "isolated";

      // 清空订单ID
      eventHandler.lastOrderId.clear();
      eventHandler.lastClientOrderId.clear();

      std::cout << "[步骤1] WebSocket下单..." << std::endl;
      std::cout << "注意: WebSocket首次连接需要时间建立和授权..." << std::endl;
      tradingService.createOrderAsync(request, credential, "okx-swap-create");

      // 等待WebSocket连接、授权和下单响应
      std::this_thread::sleep_for(std::chrono::seconds(6));

      // WebSocket撤单
      if (!eventHandler.lastOrderId.empty()) {
        std::cout << "\n[步骤2] WebSocket撤单..." << std::endl;
        std::cout << "撤销订单ID: " << eventHandler.lastOrderId << std::endl;

        LTPCancelOrderRequest cancelRequest;
        cancelRequest.exchange = LTPExchange::OKX;
        cancelRequest.symbol = "USDC-USDT-SWAP";
        cancelRequest.orderId = eventHandler.lastOrderId;

        tradingService.cancelOrderAsync(cancelRequest, credential, "okx-swap-cancel");

        // 等待撤单响应
        std::this_thread::sleep_for(std::chrono::seconds(3));
      } else {
        std::cout << "\n⚠️  跳过WebSocket撤单: 未获取到订单ID" << std::endl;
        std::cout << "可能原因: WebSocket连接未建立或下单失败\n" << std::endl;
      }
    } else {
      std::cout << "⚠️  跳过测试5: 未设置OKX API凭证\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试6: OKX订单状态订阅 - 实时接收订单状态更新
  // ====================================================================
  if (0)
  {
    std::cout << "\n测试6: OKX订单状态订阅" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* okxApiKey = "9a746984-40e6-492b-bdb6-5024877ecf72";
    const char* okxApiSecret = "AA2AA1ABC5F7B08A52C342F719F4F209";
    const char* okxApiPassphrase = "%ug8wYpFABJM%A0A";

    if (okxApiKey && okxApiSecret && okxApiPassphrase) {
      std::cout << "API Key: " << std::string(okxApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "\n说明:" << std::endl;
      std::cout << "  - 订阅OKX的orders频道" << std::endl;
      std::cout << "  - 实时接收所有订单的状态更新" << std::endl;
      std::cout << "  - 包括：新建、部分成交、完全成交、撤销等状态" << std::endl;
      std::cout << "\n⭐ 这样就能获取到完整的订单状态了！\n" << std::endl;

      credential.clear();
      credential[CCAPI_OKX_API_KEY] = okxApiKey;
      credential[CCAPI_OKX_API_SECRET] = okxApiSecret;
      credential[CCAPI_OKX_API_PASSPHRASE] = okxApiPassphrase;

      // 创建订阅 - 订阅所有交易对的订单更新
      Subscription subscription(
        CCAPI_EXCHANGE_NAME_OKX,           // 交易所
        "",                                 // 空字符串表示订阅所有交易对
        CCAPI_EM_ORDER_UPDATE,             // 订单状态更新
        "",                                 // 选项（空）
        "okx-orders-subscription",          // 关联ID
        credential                          // 认证信息
      );

      // 如果只想订阅特定交易对，可以这样：
      // Subscription subscription(
      //   CCAPI_EXCHANGE_NAME_OKX,
      //   "BTC-USDT-SWAP",                 // 指定交易对
      //   CCAPI_EM_ORDER_UPDATE,
      //   "",
      //   "okx-btc-orders",
      //   credential
      // );

      std::cout << "正在连接OKX WebSocket并订阅orders频道..." << std::endl;
      session.subscribe(subscription);

      std::cout << "✅ 订阅请求已发送" << std::endl;
      std::cout << "\n现在你可以:" << std::endl;
      std::cout << "  1. 在另一个终端运行测试5进行下单" << std::endl;
      std::cout << "  2. 或者在OKX网页/APP上手动下单" << std::endl;
      std::cout << "  3. 你将在这里看到订单状态的实时更新！\n" << std::endl;
      std::cout << "等待订单更新... (程序将运行60秒)\n" << std::endl;

      // 保持程序运行，接收订单更新
      std::this_thread::sleep_for(std::chrono::seconds(60));

      std::cout << "\n订阅测试结束" << std::endl;
    } else {
      std::cout << "⚠️  跳过测试6: 未设置OKX API凭证\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试7: 币安U本位合约 - 通过WebSocket订阅获取订单状态
  // ====================================================================
  if (1)
  {
    std::cout << "\n测试7: 币安U本位合约 - WebSocket订阅订单状态" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* futuresApiKey = "hplus70npfafWR87cFC9V0mgfitu3t9oDxfg5kDCVy9OhUfI5VCJiQ17uFnRKCYB";
    const char* futuresApiSecret = "ztBWlo3PrHbFDxCv11kdRzCBF4v4EgkLwQeDUfx5pmwau9ax3cTkgJBKNCFOlOxa";

    if (futuresApiKey && futuresApiSecret) {
      std::cout << "API Key: " << std::string(futuresApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: WebSocket订阅" << std::endl;
      std::cout << "交易对: USDCUSDT" << std::endl;
      std::cout << "\n测试流程: 订阅订单更新 → 异步下单 → 从订阅获取状态 → 查询订单 → 撤单\n" << std::endl;

      credential.clear();
      credential[CCAPI_BINANCE_USDS_FUTURES_API_KEY] = futuresApiKey;
      credential[CCAPI_BINANCE_USDS_FUTURES_API_SECRET] = futuresApiSecret;

      // 清空订单ID
      eventHandler.lastOrderId.clear();
      eventHandler.lastClientOrderId.clear();

      // 步骤1: 订阅订单状态更新
      std::cout << "[步骤1] 订阅订单状态更新..." << std::endl;
      tradingService.subscribeOrderUpdates(
        LTPExchange::BINANCE_USDS_FUTURES,
        "USDCUSDT",  // 订阅USDCUSDT交易对
        credential,
        "binance-futures-orders"
      );

      std::cout << "✅ 订阅请求已发送，等待连接建立..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(3));

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      // 步骤2: 异步下单
      std::cout << "\n[步骤2] 异步下单..." << std::endl;
      LTPCreateOrderRequest createRequest;
      createRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;
      createRequest.symbol = "USDCUSDT";
      createRequest.side = LTPOrderSide::BUY;
      createRequest.type = LTPOrderType::LIMIT;
      createRequest.quantity = "10";
      createRequest.price = "0.9";
      createRequest.timeInForce = LTPTimeInForce::GTC;
      createRequest.clientOrderId = "sub_test_" + std::to_string(timestamp);

      tradingService.createOrderAsync(createRequest, credential, "binance-futures-create");
      std::cout << "✅ 异步下单请求已发送" << std::endl;

      // 步骤3: 等待从WebSocket订阅频道接收订单状态更新
      std::cout << "\n[步骤3] 等待从WebSocket订阅频道接收订单状态..." << std::endl;
      std::cout << "（订单状态将通过 EventHandler 的 SUBSCRIPTION_DATA 事件推送）" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(3));

      // 步骤4: 查询订单状态（确认订单详情）
      if (!eventHandler.lastOrderId.empty()) {
        std::cout << "\n[步骤4] 查询订单状态（确认订单详情）..." << std::endl;
        LTPGetOrderRequest getRequest;
        getRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;
        getRequest.symbol = "USDCUSDT";
        getRequest.orderId = eventHandler.lastOrderId;

        // 使用异步接口（自动使用REST） - 响应会通过EventHandler回调接收
        tradingService.getOrderAsync(getRequest, credential, "binance-futures-get-order");
        std::cout << "✅ 异步查询请求已发送（REST），等待响应..." << std::endl;
        std::cout << "（查询结果将通过EventHandler回调显示）" << std::endl;

        // 等待查询响应
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 步骤5: 查询openOrder
        std::cout << "\n[步骤5] 查询开放订单..." << std::endl;
        LTPGetOpenOrdersRequest openOrdersRequest;
        openOrdersRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;
        openOrdersRequest.symbol = "USDCUSDT";  // 查询USDCUSDT的开放订单

        tradingService.getOpenOrdersAsync(openOrdersRequest, credential, "binance-futures-get-open-orders");
        std::cout << "✅ 异步查询请求已发送（REST），等待响应..." << std::endl;
        std::cout << "（开放订单列表将通过EventHandler回调显示）" << std::endl;

        // 等待openOrder查询响应
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // 步骤6: 撤单
        std::cout << "\n[步骤6] 撤单..." << std::endl;
        LTPCancelOrderRequest cancelRequest;
        cancelRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;
        cancelRequest.symbol = "USDCUSDT";
        cancelRequest.orderId = eventHandler.lastOrderId;

        tradingService.cancelOrderAsync(cancelRequest, credential, "binance-futures-cancel");
        std::cout << "✅ 异步撤单请求已发送" << std::endl;

        // 等待撤单响应和订阅推送
        std::cout << "\n等待撤单响应和订阅推送..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
      } else {
        std::cout << "\n⚠️  未从订阅频道获取到订单ID，跳过查询和撤单" << std::endl;
      }

      // 步骤7: 查询账户余额
      std::cout << "\n[步骤7] 查询账户余额..." << std::endl;
      LTPGetAccountBalancesRequest balanceRequest;
      balanceRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;

      // 使用异步接口（自动使用REST） - 响应会通过EventHandler回调接收
      tradingService.getAccountBalancesAsync(balanceRequest, credential, "binance-futures-get-balances");
      std::cout << "✅ 异步查询请求已发送（REST），等待响应..." << std::endl;
      std::cout << "（余额信息将通过EventHandler回调显示）" << std::endl;

      // 等待余额查询响应
      std::this_thread::sleep_for(std::chrono::seconds(3));

      // 步骤8: 查询持仓
      std::cout << "\n[步骤8] 查询持仓..." << std::endl;
      LTPGetAccountPositionsRequest positionsRequest;
      positionsRequest.exchange = LTPExchange::BINANCE_USDS_FUTURES;
      positionsRequest.symbol = "USDCUSDT";  // 查询USDCUSDT的持仓

      tradingService.getAccountPositionsAsync(positionsRequest, credential, "binance-futures-get-positions");
      std::cout << "✅ 异步查询请求已发送（REST），等待响应..." << std::endl;
      std::cout << "（持仓信息将通过EventHandler回调显示）" << std::endl;

      // 等待持仓查询响应
      std::this_thread::sleep_for(std::chrono::seconds(3));
    } else {
      std::cout << "⚠️  跳过测试7: 未设置币安合约API凭证\n" << std::endl;
    }
  }

  // ====================================================================
  // 测试8: OKX - 通过WebSocket订阅获取订单状态
  // ====================================================================
  if (0)
  {
    std::cout << "\n测试8: OKX - WebSocket订阅订单状态" << std::endl;
    std::cout << "========================================" << std::endl;

    const char* okxApiKey = "9a746984-40e6-492b-bdb6-5024877ecf72";
    const char* okxApiSecret = "AA2AA1ABC5F7B08A52C342F719F4F209";
    const char* okxApiPassphrase = "%ug8wYpFABJM%A0A";

    if (okxApiKey && okxApiSecret && okxApiPassphrase) {
      std::cout << "API Key: " << std::string(okxApiKey).substr(0, 8) << "..." << std::endl;
      std::cout << "协议: WebSocket订阅" << std::endl;
      std::cout << "交易对: USDC-USDT (现货)" << std::endl;
      std::cout << "\n测试流程: 订阅订单更新 → 异步下单 → 从订阅获取状态 → 查询订单 → 撤单\n" << std::endl;

      credential.clear();
      credential[CCAPI_OKX_API_KEY] = okxApiKey;
      credential[CCAPI_OKX_API_SECRET] = okxApiSecret;
      credential[CCAPI_OKX_API_PASSPHRASE] = okxApiPassphrase;

      // 清空订单ID
      eventHandler.lastOrderId.clear();
      eventHandler.lastClientOrderId.clear();

      // 步骤1: 订阅订单状态更新
      std::cout << "[步骤1] 订阅订单状态更新..." << std::endl;
      tradingService.subscribeOrderUpdates(
        LTPExchange::OKX,
        "USDC-USDT",  // 订阅USDC-USDT交易对
        credential,
        "okx-orders"
      );

      std::cout << "✅ 订阅请求已发送，等待连接建立..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(3));

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      // 步骤2: 异步下单
      std::cout << "\n[步骤2] 异步下单..." << std::endl;
      LTPCreateOrderRequest createRequest;
      createRequest.exchange = LTPExchange::OKX;
      createRequest.symbol = "USDC-USDT";
      createRequest.side = LTPOrderSide::BUY;
      createRequest.type = LTPOrderType::LIMIT;
      createRequest.quantity = "10";
      createRequest.price = "0.9";
      createRequest.clientOrderId = "query" + std::to_string(timestamp % 10000000000);

      tradingService.createOrderAsync(createRequest, credential, "okx-create");
      std::cout << "✅ 异步下单请求已发送" << std::endl;

      // 步骤3: 等待从WebSocket订阅频道接收订单状态更新
      std::cout << "\n[步骤3] 等待从WebSocket订阅频道接收订单状态..." << std::endl;
      std::cout << "（订单状态将通过EventHandler的 SUBSCRIPTION_DATA 事件推送）" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(3));

      // 步骤4: 查询订单状态（确认订单详情）
      if (!eventHandler.lastOrderId.empty()) {
        std::cout << "\n[步骤4] 查询订单状态（确认订单详情）..." << std::endl;
        LTPGetOrderRequest getRequest;
        getRequest.exchange = LTPExchange::OKX;
        getRequest.symbol = "USDC-USDT";
        getRequest.orderId = eventHandler.lastOrderId;

        LTPResponse getResponse = tradingService.getOrder(getRequest, credential);

        if (getResponse.success) {
          std::cout << "✅ 查询成功!" << std::endl;
          std::cout << "   订单ID: " << getResponse.orderInfo.orderId << std::endl;
          std::cout << "   客户端订单ID: " << getResponse.orderInfo.clientOrderId << std::endl;
          std::cout << "   交易对: " << getResponse.orderInfo.symbol << std::endl;
          std::cout << "   方向: " << (getResponse.orderInfo.side == LTPOrderSide::BUY ? "BUY" : "SELL") << std::endl;
          std::cout << "   类型: " << (getResponse.orderInfo.type == LTPOrderType::LIMIT ? "LIMIT" : "MARKET") << std::endl;
          std::cout << "   价格: " << getResponse.orderInfo.price << std::endl;
          std::cout << "   数量: " << getResponse.orderInfo.quantity << std::endl;
          std::cout << "   已成交数量: " << getResponse.orderInfo.executedQty << std::endl;

          std::cout << "   ⭐ 订单状态: ";
          switch (getResponse.orderInfo.status) {
            case LTPOrderStatus::NEW:
              std::cout << "NEW (新建)" << std::endl;
              break;
            case LTPOrderStatus::FILLED:
              std::cout << "FILLED (完全成交)" << std::endl;
              break;
            case LTPOrderStatus::PARTIALLY_FILLED:
              std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
              break;
            case LTPOrderStatus::CANCELED:
              std::cout << "CANCELED (已取消)" << std::endl;
              break;
            default:
              std::cout << "UNKNOWN" << std::endl;
          }

          // 显示额外信息（调试用）
          if (!getResponse.orderInfo.extraInfo.empty()) {
            std::cout << "\n   额外信息:" << std::endl;
            for (const auto& kv : getResponse.orderInfo.extraInfo) {
              std::cout << "     " << kv.first << ": " << kv.second << std::endl;
            }
          }

          // 步骤5: 查询开放订单
          std::cout << "\n[步骤5] 查询开放订单..." << std::endl;
      LTPGetOpenOrdersRequest openOrdersRequest;
      openOrdersRequest.exchange = LTPExchange::OKX;
      openOrdersRequest.symbol = "USDC-USDT";  // 查询USDC-USDT的开放订单

      LTPResponse openOrdersResponse = tradingService.getOpenOrders(openOrdersRequest, credential);

      if (openOrdersResponse.success) {
        std::cout << "✅ 查询成功! 开放订单数量: " << openOrdersResponse.orders.size() << std::endl;
        for (const auto& order : openOrdersResponse.orders) {
          std::cout << "   订单ID: " << order.orderId << std::endl;
          std::cout << "   客户端订单ID: " << order.clientOrderId << std::endl;
          std::cout << "   交易对: " << order.symbol << std::endl;
          std::cout << "   方向: " << (order.side == LTPOrderSide::BUY ? "BUY" : "SELL") << std::endl;
          std::cout << "   价格: " << order.price << std::endl;
          std::cout << "   数量: " << order.quantity << std::endl;
          std::cout << "   已成交数量: " << order.executedQty << std::endl;
          std::cout << "   ---" << std::endl;
        }
          } else {
            std::cout << "❌ 查询失败: " << openOrdersResponse.errorMessage << std::endl;
          }

          // 步骤6: 撤单
          std::cout << "\n[步骤6] 撤单..." << std::endl;
          LTPCancelOrderRequest cancelRequest;
          cancelRequest.exchange = LTPExchange::OKX;
          cancelRequest.symbol = "USDC-USDT";
          cancelRequest.orderId = eventHandler.lastOrderId;

          tradingService.cancelOrderAsync(cancelRequest, credential, "okx-cancel");
          std::cout << "✅ 异步撤单请求已发送" << std::endl;

          // 等待撤单响应和订阅推送
          std::cout << "\n等待撤单响应和订阅推送..." << std::endl;
          std::this_thread::sleep_for(std::chrono::seconds(3));
        } else {
          std::cout << "❌ 查询失败: " << getResponse.errorMessage << std::endl;
        }
      } else {
        std::cout << "\n⚠️  未从订阅频道获取到订单ID，跳过查询和撤单" << std::endl;
      }

      // 步骤7: 查询账户余额
      std::cout << "\n[步骤7] 查询账户余额..." << std::endl;
      LTPGetAccountBalancesRequest balanceRequest;
      balanceRequest.exchange = LTPExchange::OKX;

      LTPResponse balanceResponse = tradingService.getAccountBalances(balanceRequest, credential);

      if (balanceResponse.success) {
        std::cout << "✅ 查询成功! 账户余额:" << std::endl;
        for (const auto& balance : balanceResponse.balances) {
          std::cout << "   资产: " << balance.asset << std::endl;
          std::cout << "   可用余额: " << balance.availableBalance << std::endl;
          std::cout << "   总余额: " << balance.totalBalance << std::endl;
          if (!balance.frozenBalance.empty()) {
            std::cout << "   冻结余额: " << balance.frozenBalance << std::endl;
          }
          std::cout << "   ---" << std::endl;
        }

        // 打印原始交易所响应
        if (!balanceResponse.rawResponse.empty()) {
          std::cout << "\n📄 原始交易所响应:" << std::endl;
          std::cout << balanceResponse.rawResponse << std::endl;
        }
      } else {
        std::cout << "❌ 查询失败: " << balanceResponse.errorMessage << std::endl;
      }

      // 步骤8: 查询持仓
      std::cout << "\n[步骤8] 查询持仓..." << std::endl;
      LTPGetAccountPositionsRequest positionsRequest;
      positionsRequest.exchange = LTPExchange::OKX;
      positionsRequest.symbol = "USDC-USDT-SWAP";  // 查询USDC-USDT-SWAP的持仓

      LTPResponse positionsResponse = tradingService.getAccountPositions(positionsRequest, credential);

      if (positionsResponse.success) {
        std::cout << "✅ 查询成功! 持仓数量: " << positionsResponse.positions.size() << std::endl;
        for (const auto& position : positionsResponse.positions) {
          std::cout << "   交易对: " << position.symbol << std::endl;
          std::cout << "   持仓方向: " << position.positionSide << std::endl;
          std::cout << "   持仓数量: " << position.positionAmount << std::endl;
          std::cout << "   开仓均价: " << position.entryPrice << std::endl;
          if (!position.unrealizedProfit.empty()) {
            std::cout << "   未实现盈亏: " << position.unrealizedProfit << std::endl;
          }
          if (!position.leverage.empty()) {
            std::cout << "   杠杆倍数: " << position.leverage << std::endl;
          }
          std::cout << "   ---" << std::endl;
        }
      } else {
        std::cout << "❌ 查询失败: " << positionsResponse.errorMessage << std::endl;
      }
    } else {
      std::cout << "⚠️  跳过测试8: 未设置OKX API凭证\n" << std::endl;
    }
  }

  std::cout << "\n========================================" << std::endl;
  std::cout << "测试完成!" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "\n1. 测试概览:" << std::endl;
  std::cout << "   - 测试1-2: 币安统一账户和经典账户（已禁用）" << std::endl;
  std::cout << "   - 测试3: OKX现货 REST API 下单+撤单" << std::endl;
  std::cout << "   - 测试4: OKX现货 WebSocket API 下单+撤单" << std::endl;
  std::cout << "   - 测试5: OKX永续合约 WebSocket API 下单+撤单" << std::endl;
  std::cout << "   - 测试6: OKX订单状态订阅（实时接收订单状态更新）" << std::endl;
  std::cout << "   - ⭐ 测试7: 币安U本位合约 - WebSocket订阅+查询订单状态（已启用）" << std::endl;
  std::cout << "   - ⭐ 测试8: OKX - WebSocket订阅+查询订单状态（已禁用）" << std::endl;
  std::cout << "\n2. 测试流程:" << std::endl;
  std::cout << "   - 测试1-6: 下单 → 撤单" << std::endl;
  std::cout << "   - ⭐ 测试7-8（新流程）:" << std::endl;
  std::cout << "     1. 订阅WebSocket订单更新频道" << std::endl;
  std::cout << "     2. 异步下单" << std::endl;
  std::cout << "     3. 从WebSocket订阅频道实时接收订单状态" << std::endl;
  std::cout << "     4. 发起订单查询请求（确认订单详情）" << std::endl;
  std::cout << "     5. 查到订单状态后撤单" << std::endl;
  std::cout << "   - 价格设置远离市价，避免实际成交" << std::endl;
  std::cout << "\n3. OKX交易对类型:" << std::endl;
  std::cout << "   - 现货: USDC-USDT (测试3、4、8)" << std::endl;
  std::cout << "   - 永续合约: USDC-USDT-SWAP (测试5)" << std::endl;
  std::cout << "   - tdMode自动根据交易对类型设置" << std::endl;
  std::cout << "\n4. OKX tdMode配置:" << std::endl;
  std::cout << "   - 默认: 'cross' (全仓保证金模式)" << std::endl;
  std::cout << "   - 可选: 'cash' (现货模式), 'isolated' (逐仓模式)" << std::endl;
  std::cout << "   - 通过 request.extraParams[\"tdMode\"] 自定义" << std::endl;
  std::cout << "\n5. 获取订单状态的三种方式:" << std::endl;
  std::cout << "   ⭐ 方式1: WebSocket订阅 + 查询（推荐，见测试7-8）" << std::endl;
  std::cout << "     - 订阅订单更新频道实时接收状态变化" << std::endl;
  std::cout << "     - 使用getOrder()查询完整订单详情" << std::endl;
  std::cout << "     - 优点: 实时性好，信息完整" << std::endl;
  std::cout << "   方式2: 仅WebSocket订阅（见测试6）" << std::endl;
  std::cout << "     - 订阅orders频道实时接收状态更新" << std::endl;
  std::cout << "     - 优点: 实时推送，无需轮询" << std::endl;
  std::cout << "   方式3: 仅REST API查询" << std::endl;
  std::cout << "     - 使用getOrder()主动查询" << std::endl;
  std::cout << "     - 优点: 简单直接" << std::endl;
  std::cout << "\n6. 查询订单状态API:" << std::endl;
  std::cout << "   - 币安U本位合约: GET /fapi/v1/order" << std::endl;
  std::cout << "   - OKX: GET /api/v5/trade/order" << std::endl;
  std::cout << "   - 统一接口: tradingService.getOrder(request, credential)" << std::endl;
  std::cout << "   - 支持通过orderId或clientOrderId查询" << std::endl;
  std::cout << "\n7. WebSocket订阅API:" << std::endl;
  std::cout << "   - 统一接口: tradingService.subscribeOrderUpdates()" << std::endl;
  std::cout << "   - 自动处理不同交易所的订阅参数差异" << std::endl;
  std::cout << "   - 订单状态通过EventHandler的SUBSCRIPTION_DATA事件推送" << std::endl;
  std::cout << "\n8. API凭证:" << std::endl;
  std::cout << "   - OKX: OKX_API_KEY, OKX_API_SECRET, OKX_API_PASSPHRASE" << std::endl;
  std::cout << "   - 币安: BINANCE_PM_API_KEY, BINANCE_FUTURES_API_KEY (可选)" << std::endl;
  std::cout << "\n注意事项:" << std::endl;
  std::cout << "   - ✅ 统一接口通过 response.success 判断操作是否成功" << std::endl;
  std::cout << "   - OKX WebSocket下单/撤单响应不包含订单状态（这是OKX API设计）" << std::endl;
  std::cout << "   - 异步操作（createOrderAsync/cancelOrderAsync）返回void" << std::endl;
  std::cout << "   - 异步操作的结果通过EventHandler回调接收" << std::endl;

  // 正确关闭Session
  std::cout << "\n正在关闭连接..." << std::endl;
  session.stop();
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "程序正常退出\n" << std::endl;

  return 0;
}




