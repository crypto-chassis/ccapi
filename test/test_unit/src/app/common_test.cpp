#include "gtest/gtest.h"

#include "app/common.h"
namespace ccapi {
TEST(AppUtilTest, linearInterpolate) {
  EXPECT_DOUBLE_EQ(
      AppUtil::linearInterpolate(0.5, 0.00005, 0.975609756097560953950090909090909090911614, 0.00010000000000000000479, 0.910732082043367723592741658),
      0.00009317952657378995);
}

TEST(AppUtilTest, roundInputRoundUp_1) { EXPECT_EQ(AppUtil::roundInput(34359.212874750002811, "0.01", true), "34359.22"); }

TEST(AppUtilTest, roundInputRoundDown_1) { EXPECT_EQ(AppUtil::roundInput(34359.212874750002811, "0.01", false), "34359.21"); }

TEST(AppUtilTest, roundInputRoundUp_2) { EXPECT_EQ(AppUtil::roundInput(0.097499008778091811322, "0.00000001", true), "0.09749901"); }

TEST(AppUtilTest, roundInputRoundDown_2) { EXPECT_EQ(AppUtil::roundInput(0.097499008778091811322, "0.00000001", false), "0.09749900"); }

} /* namespace ccapi */
