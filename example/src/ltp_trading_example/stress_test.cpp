#include "ccapi_cpp/ccapi_session.h"
#include "ccapi_cpp/ccapi_ltp_trading_interface.h"
#include "ccapi_cpp/ccapi_ltp_trading_service.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <mutex>
#include <algorithm>

using namespace ccapi;
using namespace ltp;

/**
 * 币安U本位合约统一接口压测程序
 *
 * 测试目标：
 * - 使用统一交易接口进行高频下单/撤单压测
 * - 测量WebSocket下单和撤单的延迟
 * - 统计P50, P90, P95, P99延迟
 *
 * 测试方案：
 * 1. 建立WebSocket连接并授权
 * 2. 循环执行：下单 → 等待响应 → 撤单 → 等待响应
 * 3. 记录每次操作的延迟
 * 4. 输出统计结果
 */

// 延迟统计 (微秒)
std::vector<long long> createLatencies;
std::vector<long long> cancelLatencies;
std::mutex latencyMutex;

// 计算分位数
double calculatePercentile(std::vector<long long> data, double percentile) {
  if (data.empty()) return 0.0;
  std::sort(data.begin(), data.end());
  size_t index = static_cast<size_t>(std::ceil(percentile / 100.0 * data.size())) - 1;
  if (index >= data.size()) index = data.size() - 1;
  return static_cast<double>(data[index]);
}

// 打印分位数统计
void printPercentiles(const std::string& type, const std::vector<long long>& latencies) {
  if (latencies.empty()) {
    std::cout << type << " - 无数据" << std::endl;
    return;
  }

  std::vector<long long> sortedData = latencies;
  std::sort(sortedData.begin(), sortedData.end());

  double p50 = calculatePercentile(sortedData, 50);
  double p90 = calculatePercentile(sortedData, 90);
  double p95 = calculatePercentile(sortedData, 95);
  double p99 = calculatePercentile(sortedData, 99);

  long long sum = 0;
  for (auto latency : sortedData) {
    sum += latency;
  }
  double avg = static_cast<double>(sum) / sortedData.size();

  std::cout << "\n" << type << " 延迟统计 (样本数: " << sortedData.size() << "):" << std::endl;
  std::cout << "  平均值: " << avg << " μs (" << (avg / 1000.0) << " ms)" << std::endl;
  std::cout << "  P50: " << p50 << " μs (" << (p50 / 1000.0) << " ms)" << std::endl;
  std::cout << "  P90: " << p90 << " μs (" << (p90 / 1000.0) << " ms)" << std::endl;
  std::cout << "  P95: " << p95 << " μs (" << (p95 / 1000.0) << " ms)" << std::endl;
  std::cout << "  P99: " << p99 << " μs (" << (p99 / 1000.0) << " ms)" << std::endl;
  std::cout << "  最小值: " << sortedData.front() << " μs (" << (sortedData.front() / 1000.0) << " ms)" << std::endl;
  std::cout << "  最大值: " << sortedData.back() << " μs (" << (sortedData.back() / 1000.0) << " ms)" << std::endl;
}

// 压测事件处理器
class StressTestHandler : public EventHandler {
 public:
  void processEvent(const Event& event, Session* session) override {
    // 转换为统一响应
    LTPResponse response = LTPTradingService::convertEventToResponse(event);

    // 处理会话状态
    if (response.eventType == LTPEventType::SESSION_STATUS) {
      if (response.messageType == LTPMessageType::SESSION_CONNECTION_UP) {
        std::cout << "✅ WebSocket连接已建立" << std::endl;
        isConnected = true;
      }
    }
    // 处理授权状态
    else if (response.eventType == LTPEventType::AUTHORIZATION_STATUS) {
      if (response.messageType == LTPMessageType::AUTHORIZATION_SUCCESS) {
        std::cout << "✅ WebSocket授权成功!" << std::endl;
        isAuthorized = true;
      } else if (response.messageType == LTPMessageType::AUTHORIZATION_FAILURE) {
        std::cout << "❌ WebSocket授权失败: " << response.errorMessage << std::endl;
      }
    }
    // 处理订阅状态
    else if (response.eventType == LTPEventType::SUBSCRIPTION_STATUS) {
      if (response.messageType == LTPMessageType::SUBSCRIPTION_STARTED) {
        std::cout << "✅ 订阅已启动" << std::endl;
        isSubscribed = true;
      }
    }
    // 处理交易响应
    else if (response.eventType == LTPEventType::RESPONSE) {
      if (response.messageType == LTPMessageType::CREATE_ORDER) {
        auto now = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            now - lastCreateOrderTime).count();

        std::lock_guard<std::mutex> lock(latencyMutex);
        createLatencies.push_back(latency);

        if (response.success && !response.orderInfo.orderId.empty()) {
          lastOrderId = response.orderInfo.orderId;
          // 立即撤单
          cancelOrder(session);
        } else {
          std::cout << "❌ 下单失败: " << response.errorMessage << std::endl;
          completedOrders++;
        }
      }
      else if (response.messageType == LTPMessageType::CANCEL_ORDER) {
        auto now = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            now - lastCancelOrderTime).count();

        std::lock_guard<std::mutex> lock(latencyMutex);
        cancelLatencies.push_back(latency);

        completedOrders++;

        // 显示进度
        if (completedOrders % 10 == 0 || completedOrders == totalOrders) {
          std::cout << "进度: " << completedOrders << "/" << totalOrders << std::endl;
        }

        if (completedOrders < totalOrders) {
          // 继续下一轮测试
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          createOrder(session);
        } else {
          // 测试完成,打印统计
          std::cout << "\n========== 测试完成 ==========" << std::endl;
          printPercentiles("下单", createLatencies);
          printPercentiles("撤单", cancelLatencies);
          std::cout << "============================\n" << std::endl;
        }
      }
      else if (response.messageType == LTPMessageType::RESPONSE_ERROR) {
        std::cout << "❌ 请求失败: " << response.errorMessage << std::endl;
        completedOrders++;
      }
    }
  }

  void createOrder(Session* session) {
    LTPCreateOrderRequest request;
    request.exchange = LTPExchange::BINANCE_USDS_FUTURES;
    request.symbol = "USDCUSDT";
    request.side = LTPOrderSide::BUY;
    request.type = LTPOrderType::LIMIT;
    request.quantity = "10";
    request.price = "0.990";  // 低于市价，不会成交
    request.timeInForce = LTPTimeInForce::GTC;

    lastCreateOrderTime = std::chrono::steady_clock::now();
    tradingService->createOrderAsync(request, credential, "stress-test-create");
  }

  void cancelOrder(Session* session) {
    LTPCancelOrderRequest request;
    request.exchange = LTPExchange::BINANCE_USDS_FUTURES;
    request.symbol = "USDCUSDT";
    request.orderId = lastOrderId;

    lastCancelOrderTime = std::chrono::steady_clock::now();
    tradingService->cancelOrderAsync(request, credential, "stress-test-cancel");
  }

  LTPTradingService* tradingService = nullptr;
  std::map<std::string, std::string> credential;
  bool isConnected = false;
  bool isAuthorized = false;
  bool isSubscribed = false;
  std::string lastOrderId;
  std::chrono::steady_clock::time_point lastCreateOrderTime;
  std::chrono::steady_clock::time_point lastCancelOrderTime;
  int completedOrders = 0;
  int totalOrders = 100;  // 默认测试100轮
};

int main(int argc, char** argv) {
  // 从命令行或环境变量获取API凭证
  std::string apiKey;
  std::string apiSecret;

  if (argc >= 3) {
    apiKey = argv[1];
    apiSecret = argv[2];
  } else {
    apiKey = UtilSystem::getEnvAsString("BINANCE_USDS_FUTURES_API_KEY");
    apiSecret = UtilSystem::getEnvAsString("BINANCE_USDS_FUTURES_API_SECRET");
  }

  if (apiKey.empty() || apiSecret.empty()) {
    std::cerr << "错误: 未提供 API 凭证" << std::endl;
    std::cerr << "\n使用方法:" << std::endl;
    std::cerr << "  方式1 - 命令行参数:" << std::endl;
    std::cerr << "    " << argv[0] << " <api_key> <api_secret> [test_count]" << std::endl;
    std::cerr << "  方式2 - 环境变量:" << std::endl;
    std::cerr << "    export BINANCE_USDS_FUTURES_API_KEY='your_key'" << std::endl;
    std::cerr << "    export BINANCE_USDS_FUTURES_API_SECRET='your_secret'" << std::endl;
    return EXIT_FAILURE;
  }

  // 获取测试次数
  int testCount = 100;  // 默认测试100次
  if (argc >= 4) {
    testCount = std::atoi(argv[3]);
    if (testCount <= 0) testCount = 100;
  }

  std::cout << "===========================================================" << std::endl;
  std::cout << "币安U本位合约统一接口压测程序" << std::endl;
  std::cout << "===========================================================" << std::endl;
  std::cout << "交易所: BINANCE_USDS_FUTURES" << std::endl;
  std::cout << "交易对: USDCUSDT (稳定币对,低风险)" << std::endl;
  std::cout << "测试参数: 买入10 USDC @ 0.990 USDT (不会成交)" << std::endl;
  std::cout << "测试轮数: " << testCount << std::endl;
  std::cout << "接口类型: 统一交易接口 (LTPTradingService)" << std::endl;
  std::cout << "协议: WebSocket (自动选择)" << std::endl;
  std::cout << "===========================================================\n" << std::endl;

  // 创建Session和统一交易服务
  SessionOptions sessionOptions;
  sessionOptions.enableCheckPingPongWebsocketApplicationLevel = false;
  SessionConfigs sessionConfigs;
  StressTestHandler eventHandler;
  Session session(sessionOptions, sessionConfigs, &eventHandler);

  LTPTradingService tradingService(&session);

  // 启用延迟统计
  tradingService.setEnableLatencyStats(true);

  eventHandler.tradingService = &tradingService;
  eventHandler.totalOrders = testCount;

  // 准备认证信息
  eventHandler.credential[CCAPI_BINANCE_USDS_FUTURES_API_KEY] = apiKey;
  eventHandler.credential[CCAPI_BINANCE_USDS_FUTURES_API_SECRET] = apiSecret;

  // 订阅WebSocket订单更新（这会自动建立WebSocket连接）
  std::cout << "正在建立WebSocket连接并订阅订单更新..." << std::endl;
  tradingService.subscribeOrderUpdates(
    LTPExchange::BINANCE_USDS_FUTURES,
    "USDCUSDT",
    eventHandler.credential,
    "stress-test-subscription"
  );

  std::cout << "✅ 订阅请求已发送，等待连接建立..." << std::endl;

  // 等待连接建立、授权完成和订阅启动
  // 参考main.cpp测试7的做法，简单等待3秒
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // 检查连接状态（可选，用于调试）
  if (!eventHandler.isConnected) {
    std::cout << "⚠️  警告: 未检测到WebSocket连接建立事件" << std::endl;
  }
  if (!eventHandler.isAuthorized) {
    std::cout << "⚠️  警告: 未检测到WebSocket授权成功事件" << std::endl;
  }
  if (!eventHandler.isSubscribed) {
    std::cout << "⚠️  警告: 未检测到订阅启动事件" << std::endl;
  }

  std::cout << "连接准备完成，开始压测..." << std::endl;

  // 开始压测
  std::cout << "\n开始压测...(测试过程中不显示每次结果以避免影响延迟)" << std::endl;
  eventHandler.createOrder(&session);

  // 等待所有测试完成
  while (eventHandler.completedOrders < eventHandler.totalOrders) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\n所有测试完成!" << std::endl;

  // 停止Session
  std::cout << "正在关闭连接..." << std::endl;
  session.stop();
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "程序正常退出" << std::endl;
  return EXIT_SUCCESS;
}

