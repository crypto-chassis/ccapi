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
TEST(DecimalTest, scientificNotation) {
  {
    Decimal x("1.51e-6");
    EXPECT_EQ(x.toString(), "0.00000151");
  }
  {
    Decimal x("1.51E-6");
    EXPECT_EQ(x.toString(), "0.00000151");
  }
  {
    Decimal x("3.14159e+000");
    EXPECT_EQ(x.toString(), "3.14159");
  }
  {
    Decimal x("2.00600e+003");
    EXPECT_EQ(x.toString(), "2006");
  }
  {
    Decimal x("1.00000e-010");
    EXPECT_EQ(x.toString(), "0.0000000001");
  }
}
TEST(DecimalTest, trailingZero) {
  Decimal bid_1("0.10");
  EXPECT_EQ(bid_1.toString(), "0.1");
}
TEST(DecimalTest, subtract_0) { EXPECT_EQ(Decimal("0.000000549410817836").subtract(Decimal("0")).toString(), "0.000000549410817836"); }
TEST(DecimalTest, add_1) { EXPECT_EQ(Decimal("0.020411").add(Decimal("0.006527")).toString(), "0.026938"); }
TEST(DecimalTest, add_2) { EXPECT_EQ(Decimal("0.016527").add(Decimal("0.003884")).toString(), "0.020411"); }
TEST(DecimalTest, add_3) { EXPECT_EQ(Decimal("0.908").add(Decimal("15119.106")).toString(), "15120.014"); }
TEST(DecimalTest, subtract_11) { EXPECT_EQ(Decimal("0.026938").subtract(Decimal("0.020411")).toString(), "0.006527"); }
TEST(DecimalTest, subtract_12) { EXPECT_EQ(Decimal("0.026938").subtract(Decimal("0.006527")).toString(), "0.020411"); }
TEST(DecimalTest, subtract_21) { EXPECT_EQ(Decimal("0.020411").subtract(Decimal("0.016527")).toString(), "0.003884"); }
TEST(DecimalTest, subtract_22) { EXPECT_EQ(Decimal("0.020411").subtract(Decimal("0.003884")).toString(), "0.016527"); }
TEST(DecimalTest, subtract_31) { EXPECT_EQ(Decimal("15120.014").subtract(Decimal("0.908")).toString(), "15119.106"); }
TEST(DecimalTest, subtract_32) { EXPECT_EQ(Decimal("15120.014").subtract(Decimal("15119.106")).toString(), "0.908"); }
TEST(DecimalTest, subtract_4) { EXPECT_EQ(Decimal("8.82412861").subtract(Decimal("0.20200000")).toString(), "8.62212861"); }
} /* namespace ccapi */
