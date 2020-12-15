#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_decimal.h"
namespace ccapi {
TEST(DecimalTest, compareScientificNotation) {
  Decimal bid_1("1.51e-6");
  Decimal ask_1("1.5113e-6");
  EXPECT_LT(bid_1, ask_1);
  Decimal bid_2("1.51E-6");
  Decimal ask_2("1.5113E-6");
  EXPECT_LT(bid_2, ask_2);
}
} /* namespace ccapi */
