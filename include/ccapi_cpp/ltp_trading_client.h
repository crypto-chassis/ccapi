#ifndef INCLUDE_LTP_TRADING_CLIENT_H_
#define INCLUDE_LTP_TRADING_CLIENT_H_

#include "ltp_trading_interface.h"
#include <functional>
#include <memory>

namespace ltp {

namespace internal {
  class LTPClientImpl;
}

using LTPEventCallback = std::function<void(const LTPResponse&)>;

struct LTPClientOptions {
  bool enableDebugLog = false;              // Enable debug logging
  bool enableLatencyStats = false;          // Enable latency statistics
  int connectionTimeoutSeconds = 30;        // Connection timeout (seconds)

  LTPClientOptions() = default;
};

class LTPClient {
public:
  explicit LTPClient(LTPEventCallback callback,
                     const LTPClientOptions& options = LTPClientOptions());

  ~LTPClient();

  LTPClient(const LTPClient&) = delete;
  LTPClient& operator=(const LTPClient&) = delete;

  /**
   * Start the client
   * Must be called before using any trading functions
   */
  void start();

  /**
   * Stop the client
   * Closes all connections and cleans up resources
   */
  void stop();

  /**
   * Check if the client is running
   */
  bool isRunning() const;

  // ====================================================================
  // Synchronous Trading Interface
  // ====================================================================

  /**
   * Create order (synchronous)
   */
  LTPResponse createOrder(const LTPCreateOrderRequest& request,
                          const std::map<std::string, std::string>& credential);

  /**
   * Cancel order (synchronous)
   */
  LTPResponse cancelOrder(const LTPCancelOrderRequest& request,
                          const std::map<std::string, std::string>& credential);

  /**
   * Cancel all orders (synchronous)
   */
  LTPResponse cancelAllOrders(const LTPCancelAllOrdersRequest& request,
                              const std::map<std::string, std::string>& credential);

  /**
   * Get order (synchronous)
   */
  LTPResponse getOrder(const LTPGetOrderRequest& request,
                       const std::map<std::string, std::string>& credential);

  /**
   * Get open orders (synchronous)
   */
  LTPResponse getOpenOrders(const LTPGetOpenOrdersRequest& request,
                            const std::map<std::string, std::string>& credential);

  /**
   * Get account balances (synchronous)
   */
  LTPResponse getAccountBalances(const LTPGetAccountBalancesRequest& request,
                                 const std::map<std::string, std::string>& credential);

  /**
   * Get account positions (synchronous)
   */
  LTPResponse getAccountPositions(const LTPGetAccountPositionsRequest& request,
                                  const std::map<std::string, std::string>& credential);

  // ====================================================================
  // Asynchronous Trading Interface - Auto-selects optimal protocol (FIX > WebSocket > REST)
  // ====================================================================

  /**
   * Create order (asynchronous)
   * Result returned via callback function
   */
  void createOrderAsync(const LTPCreateOrderRequest& request,
                        const std::map<std::string, std::string>& credential,
                        const std::string& correlationId = "");

  /**
   * Cancel order (asynchronous)
   */
  void cancelOrderAsync(const LTPCancelOrderRequest& request,
                        const std::map<std::string, std::string>& credential,
                        const std::string& correlationId = "");

  /**
   * Cancel all orders (asynchronous)
   */
  void cancelAllOrdersAsync(const LTPCancelAllOrdersRequest& request,
                            const std::map<std::string, std::string>& credential,
                            const std::string& correlationId = "");

  /**
   * Get order (asynchronous)
   */
  void getOrderAsync(const LTPGetOrderRequest& request,
                     const std::map<std::string, std::string>& credential,
                     const std::string& correlationId = "");

  /**
   * Get open orders (asynchronous)
   */
  void getOpenOrdersAsync(const LTPGetOpenOrdersRequest& request,
                          const std::map<std::string, std::string>& credential,
                          const std::string& correlationId = "");

  /**
   * Get account balances (asynchronous)
   */
  void getAccountBalancesAsync(const LTPGetAccountBalancesRequest& request,
                               const std::map<std::string, std::string>& credential,
                               const std::string& correlationId = "");

  /**
   * Get account positions (asynchronous)
   */
  void getAccountPositionsAsync(const LTPGetAccountPositionsRequest& request,
                                const std::map<std::string, std::string>& credential,
                                const std::string& correlationId = "");

  // ====================================================================
  // Subscription Interface - Real-time order updates, trades, etc.
  // ====================================================================

  /**
   * Subscribe to order status updates
   * Receive real-time order status changes (new, partially filled, filled, canceled, etc.)
   */
  void subscribeOrderUpdates(LTPExchange exchange,
                             const std::string& symbol,
                             const std::map<std::string, std::string>& credential,
                             const std::string& correlationId = "");

  /**
   * Subscribe to private trade updates
   */
  void subscribePrivateTrades(LTPExchange exchange,
                              const std::string& symbol,
                              const std::map<std::string, std::string>& credential,
                              const std::string& correlationId = "");

  /**
   * Subscribe to account balance updates
   */
  void subscribeBalanceUpdates(LTPExchange exchange,
                               const std::map<std::string, std::string>& credential,
                               const std::string& correlationId = "");

  /**
   * Subscribe to position updates
   */
  void subscribePositionUpdates(LTPExchange exchange,
                                const std::string& symbol,
                                const std::map<std::string, std::string>& credential,
                                const std::string& correlationId = "");

private:
  std::unique_ptr<internal::LTPClientImpl> impl_;
};

} // namespace ltp

#endif  // INCLUDE_LTP_TRADING_CLIENT_H_