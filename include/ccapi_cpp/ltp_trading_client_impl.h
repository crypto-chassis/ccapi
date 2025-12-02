#ifndef INCLUDE_LTP_TRADING_CLIENT_IMPL_H_
#define INCLUDE_LTP_TRADING_CLIENT_IMPL_H_

#include "ltp_trading_client.h"
#include "ccapi_ltp_trading_service.h"
#include "ccapi_cpp/ccapi_session.h"

namespace ltp {
namespace internal {

class LTPClientImpl : public ccapi::EventHandler {
public:
  LTPClientImpl(LTPEventCallback callback, const LTPClientOptions& options)
    : callback_(callback),
      options_(options),
      session_(nullptr),
      tradingService_(nullptr),
      isRunning_(false) {}

  ~LTPClientImpl() {
    stop();
  }

  void start() {
    if (isRunning_) {
      return;
    }

    ccapi::SessionOptions sessionOptions;
    sessionOptions.enableCheckPingPongWebsocketApplicationLevel = false;

    ccapi::SessionConfigs sessionConfigs;

    session_ = std::make_unique<ccapi::Session>(sessionOptions, sessionConfigs, this);

    tradingService_ = std::make_unique<LTPTradingService>(session_.get());
    tradingService_->setEnableLatencyStats(options_.enableLatencyStats);

    isRunning_ = true;
  }

  void stop() {
    if (!isRunning_) {
      return;
    }

    if (session_) {
      session_->stop();
    }

    tradingService_.reset();
    session_.reset();
    isRunning_ = false;
  }

  bool isRunning() const {
    return isRunning_;
  }

  void processEvent(const ccapi::Event& event, ccapi::Session* session) override {
    LTPResponse response = LTPTradingService::convertEventToResponse(event);

    if (callback_) {
      callback_(response);
    }
  }

  LTPResponse createOrder(const LTPCreateOrderRequest& request,
                          const std::map<std::string, std::string>& credential) {
    return tradingService_->createOrder(request, credential);
  }

  LTPResponse cancelOrder(const LTPCancelOrderRequest& request,
                          const std::map<std::string, std::string>& credential) {
    return tradingService_->cancelOrder(request, credential);
  }

  LTPResponse cancelAllOrders(const LTPCancelAllOrdersRequest& request,
                              const std::map<std::string, std::string>& credential) {
    return tradingService_->cancelAllOrders(request, credential);
  }

  LTPResponse getOrder(const LTPGetOrderRequest& request,
                       const std::map<std::string, std::string>& credential) {
    return tradingService_->getOrder(request, credential);
  }

  LTPResponse getOpenOrders(const LTPGetOpenOrdersRequest& request,
                            const std::map<std::string, std::string>& credential) {
    return tradingService_->getOpenOrders(request, credential);
  }

  LTPResponse getAccountBalances(const LTPGetAccountBalancesRequest& request,
                                 const std::map<std::string, std::string>& credential) {
    return tradingService_->getAccountBalances(request, credential);
  }

  LTPResponse getAccountPositions(const LTPGetAccountPositionsRequest& request,
                                  const std::map<std::string, std::string>& credential) {
    return tradingService_->getAccountPositions(request, credential);
  }

  void createOrderAsync(const LTPCreateOrderRequest& request,
                        const std::map<std::string, std::string>& credential,
                        const std::string& correlationId) {
    tradingService_->createOrderAsync(request, credential, correlationId);
  }

  void cancelOrderAsync(const LTPCancelOrderRequest& request,
                        const std::map<std::string, std::string>& credential,
                        const std::string& correlationId) {
    tradingService_->cancelOrderAsync(request, credential, correlationId);
  }

  void cancelAllOrdersAsync(const LTPCancelAllOrdersRequest& request,
                            const std::map<std::string, std::string>& credential,
                            const std::string& correlationId) {
    tradingService_->cancelAllOrdersAsync(request, credential, correlationId);
  }

  void getOrderAsync(const LTPGetOrderRequest& request,
                     const std::map<std::string, std::string>& credential,
                     const std::string& correlationId) {
    tradingService_->getOrderAsync(request, credential, correlationId);
  }

  void getOpenOrdersAsync(const LTPGetOpenOrdersRequest& request,
                          const std::map<std::string, std::string>& credential,
                          const std::string& correlationId) {
    tradingService_->getOpenOrdersAsync(request, credential, correlationId);
  }

  void getAccountBalancesAsync(const LTPGetAccountBalancesRequest& request,
                               const std::map<std::string, std::string>& credential,
                               const std::string& correlationId) {
    tradingService_->getAccountBalancesAsync(request, credential, correlationId);
  }

  void getAccountPositionsAsync(const LTPGetAccountPositionsRequest& request,
                                const std::map<std::string, std::string>& credential,
                                const std::string& correlationId) {
    tradingService_->getAccountPositionsAsync(request, credential, correlationId);
  }

  void subscribeOrderUpdates(LTPExchange exchange,
                             const std::string& symbol,
                             const std::map<std::string, std::string>& credential,
                             const std::string& correlationId) {
    tradingService_->subscribeOrderUpdates(exchange, symbol, credential, correlationId);
  }

  void subscribePrivateTrades(LTPExchange exchange,
                              const std::string& symbol,
                              const std::map<std::string, std::string>& credential,
                              const std::string& correlationId) {
    tradingService_->subscribePrivateTrades(exchange, symbol, credential, correlationId);
  }

  void subscribeBalanceUpdates(LTPExchange exchange,
                               const std::map<std::string, std::string>& credential,
                               const std::string& correlationId) {
    tradingService_->subscribeBalanceUpdates(exchange, credential, correlationId);
  }

  void subscribePositionUpdates(LTPExchange exchange,
                                const std::string& symbol,
                                const std::map<std::string, std::string>& credential,
                                const std::string& correlationId) {
    tradingService_->subscribePositionUpdates(exchange, symbol, credential, correlationId);
  }

private:
  LTPEventCallback callback_;
  LTPClientOptions options_;
  std::unique_ptr<ccapi::Session> session_;
  std::unique_ptr<LTPTradingService> tradingService_;
  bool isRunning_;
};

} // namespace internal
} // namespace ltp

#endif  // INCLUDE_LTP_TRADING_CLIENT_IMPL_H_