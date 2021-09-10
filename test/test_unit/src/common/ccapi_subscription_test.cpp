#include "ccapi_cpp/ccapi_subscription.h"

#include "gtest/gtest.h"
namespace ccapi {
TEST(SubscriptionTest, marketDepth) {
  Subscription subscription("", "", CCAPI_MARKET_DEPTH);
  EXPECT_EQ(subscription.getServiceName(), CCAPI_MARKET_DATA);
}
TEST(SubscriptionTest, trade) {
  Subscription subscription("", "", CCAPI_TRADE);
  EXPECT_EQ(subscription.getServiceName(), CCAPI_MARKET_DATA);
}
TEST(SubscriptionTest, privateTrade) {
  Subscription subscription("", "", CCAPI_EM_PRIVATE_TRADE);
  EXPECT_EQ(subscription.getServiceName(), CCAPI_EXECUTION_MANAGEMENT);
}
TEST(SubscriptionTest, orderUpdate) {
  Subscription subscription("", "", CCAPI_EM_ORDER_UPDATE);
  EXPECT_EQ(subscription.getServiceName(), CCAPI_EXECUTION_MANAGEMENT);
}
} /* namespace ccapi */
