#ifdef CCAPI_ENABLE_SERVICE_MARKET_DATA
#include "ccapi_cpp/ccapi_test_market_data_helper.h"
#include "ccapi_cpp/service/ccapi_market_data_service.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::ElementsAre;
using ::testing::Pair;
namespace ccapi {
class MarketDataServiceTest : public ::testing::Test {
 public:
  typedef Service::ServiceContextPtr ServiceContextPtr;
  void SetUp() override {
    this->service =
        std::make_shared<MarketDataServiceGeneric>([](Event&, Queue<Event>*) {}, SessionOptions(), SessionConfigs(), std::make_shared<ServiceContext>());
  }
  std::shared_ptr<MarketDataServiceGeneric> service{nullptr};
};

TEST_F(MarketDataServiceTest, updateOrderBookInsert) {
  std::map<Decimal, std::string> snapshot;
  Decimal p("1");
  auto pc = p;
  std::string s("2");
  auto sc = s;
  this->service->updateOrderBook(snapshot, p, s);
  EXPECT_THAT(snapshot, ElementsAre(Pair(pc, sc)));
}
TEST_F(MarketDataServiceTest, updateOrderBookUpdate) {
  std::map<Decimal, std::string> snapshot{
      {Decimal("1"), std::string("2")},
  };
  Decimal p("1");
  auto pc = p;
  std::string s("3");
  auto sc = s;
  this->service->updateOrderBook(snapshot, p, s);
  EXPECT_THAT(snapshot, ElementsAre(Pair(pc, sc)));
}
TEST_F(MarketDataServiceTest, updateOrderBookDelete) {
  std::map<Decimal, std::string> snapshot{
      {Decimal("1"), std::string("2")},
  };
  Decimal price("1");
  std::string size("0");
  this->service->updateOrderBook(snapshot, price, size);
  EXPECT_TRUE(snapshot.empty());
}
TEST_F(MarketDataServiceTest, updateOrderBookNoInsert) {
  std::map<Decimal, std::string> snapshot;
  Decimal price("1");
  std::string size("0");
  this->service->updateOrderBook(snapshot, price, size);
  EXPECT_TRUE(snapshot.empty());
}
} /* namespace ccapi */
#endif
