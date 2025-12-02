#include "ccapi_cpp/ltp_trading_client.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "LTP交易客户端示例" << std::endl;
  std::cout << "========================================\n" << std::endl;

  std::string lastOrderId;

  ltp::LTPClient client([&lastOrderId](const ltp::LTPResponse& response) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();

    std::cout << "\n[" << timestamp << "] ========== 收到事件 ==========" << std::endl;
    std::cout << "事件类型: " << response.eventTypeString << std::endl;
    std::cout << "消息类型: " << response.messageTypeString << std::endl;

    // 根据事件类型处理
    if (response.eventType == ltp::LTPEventType::RESPONSE) {
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
            std::cout << "   方向: " << (order.side == ltp::LTPOrderSide::BUY ? "BUY" : "SELL") << std::endl;
            std::cout << "   类型: ";
            switch (order.type) {
              case ltp::LTPOrderType::LIMIT:
                std::cout << "LIMIT" << std::endl;
                break;
              case ltp::LTPOrderType::MARKET:
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
              case ltp::LTPOrderStatus::NEW:
                std::cout << "NEW (新建)" << std::endl;
                break;
              case ltp::LTPOrderStatus::PARTIALLY_FILLED:
                std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
                break;
              case ltp::LTPOrderStatus::FILLED:
                std::cout << "FILLED (完全成交)" << std::endl;
                break;
              case ltp::LTPOrderStatus::CANCELED:
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
          if (response.orderInfo.status != ltp::LTPOrderStatus::UNKNOWN) {
            std::cout << "订单状态: ";
            switch (response.orderInfo.status) {
              case ltp::LTPOrderStatus::NEW:
                std::cout << "NEW (新建)" << std::endl;
                break;
              case ltp::LTPOrderStatus::FILLED:
                std::cout << "FILLED (完全成交)" << std::endl;
                break;
              case ltp::LTPOrderStatus::PARTIALLY_FILLED:
                std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
                break;
              case ltp::LTPOrderStatus::CANCELED:
                std::cout << "CANCELED (已取消)" << std::endl;
                break;
              default:
                break;
            }
          }

          // 保存订单ID供撤单使用
          lastOrderId = response.orderInfo.orderId;
        }
      } else {
        std::cout << "❌ 操作失败!" << std::endl;
        std::cout << "错误码: " << response.errorCode << std::endl;
        std::cout << "错误信息: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == ltp::LTPEventType::SESSION_STATUS) {
      // 会话状态事件
      if (response.messageType == ltp::LTPMessageType::SESSION_CONNECTION_UP) {
        std::cout << "✅ WebSocket连接已建立" << std::endl;
      } else if (response.messageType == ltp::LTPMessageType::SESSION_CONNECTION_DOWN) {
        std::cout << "⚠️  WebSocket连接已断开" << std::endl;
      } else {
        std::cout << "ℹ️  会话状态: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == ltp::LTPEventType::AUTHORIZATION_STATUS) {
      // 授权状态事件
      if (response.messageType == ltp::LTPMessageType::AUTHORIZATION_SUCCESS) {
        std::cout << "✅ WebSocket授权成功" << std::endl;
      } else if (response.messageType == ltp::LTPMessageType::AUTHORIZATION_FAILURE) {
        std::cout << "❌ WebSocket授权失败: " << response.errorMessage << std::endl;
      } else {
        std::cout << "ℹ️  授权状态: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == ltp::LTPEventType::SUBSCRIPTION_STATUS) {
      // 订阅状态事件
      if (response.messageType == ltp::LTPMessageType::SUBSCRIPTION_STARTED) {
        std::cout << "✅ 订阅已启动" << std::endl;
      } else if (response.messageType == ltp::LTPMessageType::SUBSCRIPTION_FAILURE) {
        std::cout << "❌ 订阅失败: " << response.errorMessage << std::endl;
      } else if (response.messageType == ltp::LTPMessageType::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE) {
        std::cout << "⚠️  订阅失败（连接问题）: " << response.errorMessage << std::endl;
      } else {
        std::cout << "ℹ️  订阅状态: " << response.errorMessage << std::endl;
      }
    }
    else if (response.eventType == ltp::LTPEventType::SUBSCRIPTION_DATA) {
      // 订阅数据（实时推送的订单更新、成交等）
      std::cout << "📊 收到订阅数据推送" << std::endl;
      if (!response.orderInfo.orderId.empty()) {
        std::cout << "订单ID: " << response.orderInfo.orderId << std::endl;
        std::cout << "客户端订单ID: " << response.orderInfo.clientOrderId << std::endl;
        std::cout << "交易对: " << response.orderInfo.symbol << std::endl;

        // ⭐ 这里可以获取到完整的订单状态！
        std::cout << "⭐ 订单状态: ";
        switch (response.orderInfo.status) {
          case ltp::LTPOrderStatus::NEW:
            std::cout << "NEW (新建)" << std::endl;
            break;
          case ltp::LTPOrderStatus::FILLED:
            std::cout << "FILLED (完全成交)" << std::endl;
            break;
          case ltp::LTPOrderStatus::PARTIALLY_FILLED:
            std::cout << "PARTIALLY_FILLED (部分成交)" << std::endl;
            break;
          case ltp::LTPOrderStatus::CANCELED:
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
    else if (response.eventType == ltp::LTPEventType::REQUEST_STATUS) {
      // 请求状态事件
      std::cout << "ℹ️  请求状态: " << response.errorMessage << std::endl;
    }
    else if (response.eventType == ltp::LTPEventType::FIX || response.eventType == ltp::LTPEventType::FIX_STATUS) {
      // FIX协议事件
      std::cout << "ℹ️  FIX事件: " << response.errorMessage << std::endl;
    }
    else if (response.eventType == ltp::LTPEventType::HEARTBEAT) {
      // 心跳事件（通常不需要打印，避免刷屏）
      // std::cout << "💓 心跳" << std::endl;
    }
    else {
      // 其他事件
      std::cout << "ℹ️  " << (response.success ? "✅" : "❌") << " " << response.errorMessage << std::endl;
    }

    std::cout << "========================================\n" << std::endl;
  });

  std::cout << "启动LTP客户端..." << std::endl;
  client.start();
  std::cout << "✅ 客户端已启动\n" << std::endl;

  const char* apiKey = std::getenv("BINANCE_USDS_FUTURES_API_KEY");
  const char* apiSecret = std::getenv("BINANCE_USDS_FUTURES_API_SECRET");

  if (!apiKey || !apiSecret) {
    std::cerr << "错误: 请设置环境变量 BINANCE_USDS_FUTURES_API_KEY 和 BINANCE_USDS_FUTURES_API_SECRET" << std::endl;
    std::cerr << "示例: export BINANCE_USDS_FUTURES_API_KEY=your_api_key" << std::endl;
    std::cerr << "      export BINANCE_USDS_FUTURES_API_SECRET=your_api_secret" << std::endl;
    return 1;
  }

  std::map<std::string, std::string> credential;
  credential["BINANCE_USDS_FUTURES_API_KEY"] = apiKey;
  credential["BINANCE_USDS_FUTURES_API_SECRET"] = apiSecret;

  std::cout << "订阅订单更新..." << std::endl;
  client.subscribeOrderUpdates(
    ltp::LTPExchange::BINANCE_USDS_FUTURES,
    "USDCUSDT",
    credential,
    "order-updates"
  );
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "\n发送下单请求..." << std::endl;
  ltp::LTPCreateOrderRequest request;
  request.exchange = ltp::LTPExchange::BINANCE_USDS_FUTURES;
  request.symbol = "USDCUSDT";
  request.side = ltp::LTPOrderSide::BUY;
  request.type = ltp::LTPOrderType::LIMIT;
  request.quantity = "10";
  request.price = "0.9";
  request.timeInForce = ltp::LTPTimeInForce::GTC;

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()).count();
  request.clientOrderId = "ltp_" + std::to_string(timestamp);

  client.createOrderAsync(request, credential, "create-order");

  std::this_thread::sleep_for(std::chrono::seconds(3));

  if (!lastOrderId.empty()) {
    std::cout << "\n查询账户余额..." << std::endl;
    ltp::LTPGetAccountBalancesRequest balanceRequest;
    balanceRequest.exchange = ltp::LTPExchange::BINANCE_USDS_FUTURES;

    client.getAccountBalancesAsync(balanceRequest, credential, "get-balances");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n发送撤单请求..." << std::endl;
    ltp::LTPCancelOrderRequest cancelRequest;
    cancelRequest.exchange = ltp::LTPExchange::BINANCE_USDS_FUTURES;
    cancelRequest.symbol = "USDCUSDT";
    cancelRequest.orderId = lastOrderId;

    client.cancelOrderAsync(cancelRequest, credential, "cancel-order");
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  std::cout << "\n停止LTP客户端..." << std::endl;
  client.stop();
  std::cout << "✅ 客户端已停止" << std::endl;

  std::cout << "\n========================================" << std::endl;
  std::cout << "示例完成!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
