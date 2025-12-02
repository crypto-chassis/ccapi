#include "ltp_trading_client.h"
#include "ltp_trading_client_impl.h"

namespace ltp {

LTPClient::LTPClient(LTPEventCallback callback, const LTPClientOptions& options)
  : impl_(std::make_unique<internal::LTPClientImpl>(callback, options)) {
}

LTPClient::~LTPClient() = default;

void LTPClient::start() {
  impl_->start();
}

void LTPClient::stop() {
  impl_->stop();
}

bool LTPClient::isRunning() const {
  return impl_->isRunning();
}

LTPResponse LTPClient::createOrder(const LTPCreateOrderRequest& request,
                                   const std::map<std::string, std::string>& credential) {
  return impl_->createOrder(request, credential);
}

LTPResponse LTPClient::cancelOrder(const LTPCancelOrderRequest& request,
                                   const std::map<std::string, std::string>& credential) {
  return impl_->cancelOrder(request, credential);
}

LTPResponse LTPClient::cancelAllOrders(const LTPCancelAllOrdersRequest& request,
                                       const std::map<std::string, std::string>& credential) {
  return impl_->cancelAllOrders(request, credential);
}

LTPResponse LTPClient::getOrder(const LTPGetOrderRequest& request,
                                const std::map<std::string, std::string>& credential) {
  return impl_->getOrder(request, credential);
}

LTPResponse LTPClient::getOpenOrders(const LTPGetOpenOrdersRequest& request,
                                     const std::map<std::string, std::string>& credential) {
  return impl_->getOpenOrders(request, credential);
}

LTPResponse LTPClient::getAccountBalances(const LTPGetAccountBalancesRequest& request,
                                          const std::map<std::string, std::string>& credential) {
  return impl_->getAccountBalances(request, credential);
}

LTPResponse LTPClient::getAccountPositions(const LTPGetAccountPositionsRequest& request,
                                           const std::map<std::string, std::string>& credential) {
  return impl_->getAccountPositions(request, credential);
}

void LTPClient::createOrderAsync(const LTPCreateOrderRequest& request,
                                 const std::map<std::string, std::string>& credential,
                                 const std::string& correlationId) {
  impl_->createOrderAsync(request, credential, correlationId);
}

void LTPClient::cancelOrderAsync(const LTPCancelOrderRequest& request,
                                 const std::map<std::string, std::string>& credential,
                                 const std::string& correlationId) {
  impl_->cancelOrderAsync(request, credential, correlationId);
}

void LTPClient::cancelAllOrdersAsync(const LTPCancelAllOrdersRequest& request,
                                     const std::map<std::string, std::string>& credential,
                                     const std::string& correlationId) {
  impl_->cancelAllOrdersAsync(request, credential, correlationId);
}

void LTPClient::getOrderAsync(const LTPGetOrderRequest& request,
                              const std::map<std::string, std::string>& credential,
                              const std::string& correlationId) {
  impl_->getOrderAsync(request, credential, correlationId);
}

void LTPClient::getOpenOrdersAsync(const LTPGetOpenOrdersRequest& request,
                                   const std::map<std::string, std::string>& credential,
                                   const std::string& correlationId) {
  impl_->getOpenOrdersAsync(request, credential, correlationId);
}

void LTPClient::getAccountBalancesAsync(const LTPGetAccountBalancesRequest& request,
                                        const std::map<std::string, std::string>& credential,
                                        const std::string& correlationId) {
  impl_->getAccountBalancesAsync(request, credential, correlationId);
}

void LTPClient::getAccountPositionsAsync(const LTPGetAccountPositionsRequest& request,
                                         const std::map<std::string, std::string>& credential,
                                         const std::string& correlationId) {
  impl_->getAccountPositionsAsync(request, credential, correlationId);
}

void LTPClient::subscribeOrderUpdates(LTPExchange exchange,
                                      const std::string& symbol,
                                      const std::map<std::string, std::string>& credential,
                                      const std::string& correlationId) {
  impl_->subscribeOrderUpdates(exchange, symbol, credential, correlationId);
}

void LTPClient::subscribePrivateTrades(LTPExchange exchange,
                                       const std::string& symbol,
                                       const std::map<std::string, std::string>& credential,
                                       const std::string& correlationId) {
  impl_->subscribePrivateTrades(exchange, symbol, credential, correlationId);
}

void LTPClient::subscribeBalanceUpdates(LTPExchange exchange,
                                        const std::map<std::string, std::string>& credential,
                                        const std::string& correlationId) {
  impl_->subscribeBalanceUpdates(exchange, credential, correlationId);
}

void LTPClient::subscribePositionUpdates(LTPExchange exchange,
                                         const std::string& symbol,
                                         const std::map<std::string, std::string>& credential,
                                         const std::string& correlationId) {
  impl_->subscribePositionUpdates(exchange, symbol, credential, correlationId);
}

} // namespace ltp