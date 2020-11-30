#ifdef ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef ENABLE_EXCHANGE_BINANCE_US
#include "gtest/gtest.h"
#include "ccapi_cpp/service/ccapi_execution_management_service_binance_us.h"
namespace ccapi {
class ExecutionManagementServiceBinanceUsTest : public ::testing::Test {
 protected:
  void SetUp() override {
//    service = std::make_shared<ExecutionManagementServiceBinanceUs>(serviceEventHandler, sessionOptions, sessionConfigs, this->serviceContextPtr);
  }
  std::shared_ptr<ExecutionManagementServiceBinanceUs> service;
};
TEST_F(ExecutionManagementServiceBinanceUsTest, convertRequest) {
//  const Request& request, const TimePoint& now
//  http::request<http::string_body> req = service->convertRequest(request, now);
  EXPECT_EQ(1, 0);
}

//TEST_F(ExecutionManagementServiceBinanceUsTest, processSuccessfulTextMessageCreateOrder) {
//  int* n = q0_.Dequeue();
//  EXPECT_EQ(n, nullptr);
//
//  n = q1_.Dequeue();
//  ASSERT_NE(n, nullptr);
//  EXPECT_EQ(*n, 1);
//  EXPECT_EQ(q1_.size(), 0);
//  delete n;
//
//  n = q2_.Dequeue();
//  ASSERT_NE(n, nullptr);
//  EXPECT_EQ(*n, 2);
//  EXPECT_EQ(q2_.size(), 1);
//  delete n;
//}
//TEST_F(ExecutionManagementServiceBinanceUsTest, processSuccessfulTextMessageCancelOpenOrders) {
//  int* n = q0_.Dequeue();
//  EXPECT_EQ(n, nullptr);
//
//  n = q1_.Dequeue();
//  ASSERT_NE(n, nullptr);
//  EXPECT_EQ(*n, 1);
//  EXPECT_EQ(q1_.size(), 0);
//  delete n;
//
//  n = q2_.Dequeue();
//  ASSERT_NE(n, nullptr);
//  EXPECT_EQ(*n, 2);
//  EXPECT_EQ(q2_.size(), 1);
//  delete n;
//}
} /* namespace ccapi */
#endif
#endif
