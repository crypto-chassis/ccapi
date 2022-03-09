#include "ccapi_cpp/ccapi_decimal.h"

#include "gtest/gtest.h"
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
  {
    Decimal x("-3.50e-2");
    EXPECT_EQ(x.toString(), "-0.035");
  }
}
TEST(DecimalTest, mostCommon) {
  Decimal bid_1("0.1");
  EXPECT_EQ(bid_1.toString(), "0.1");
}
TEST(DecimalTest, trailingZero) {
  Decimal bid_1("0.10");
  EXPECT_EQ(bid_1.toString(), "0.1");
}
TEST(DecimalTest, integer_1) {
  Decimal bid_1("1");
  EXPECT_EQ(bid_1.toString(), "1");
}
TEST(DecimalTest, integer_2) {
  Decimal bid_1("1.0");
  EXPECT_EQ(bid_1.toString(), "1");
}
TEST(DecimalTest, integer_3) {
  Decimal bid_1("0");
  EXPECT_EQ(bid_1.toString(), "0");
}
TEST(DecimalTest, integer_4) {
  Decimal bid_1("0.0");
  EXPECT_EQ(bid_1.toString(), "0");
}
TEST(DecimalTest, zero) {
  Decimal a("+0");
  Decimal b("-0");
  EXPECT_EQ(a, b);
}
TEST(DecimalTest, compare) {
  EXPECT_TRUE(Decimal("1") == Decimal("1.0"));
  EXPECT_TRUE(Decimal("1.1") < Decimal("1.12"));
  EXPECT_TRUE(Decimal("9") < Decimal("10"));
}
TEST(DecimalTest, subtract_0) { EXPECT_EQ(Decimal("0.000000549410817836").subtract(Decimal("0")).toString(), "0.000000549410817836"); }
TEST(DecimalTest, add_1) { EXPECT_EQ(Decimal("0.020411").add(Decimal("0.006527")).toString(), "0.026938"); }
TEST(DecimalTest, add_2) { EXPECT_EQ(Decimal("0.016527").add(Decimal("0.003884")).toString(), "0.020411"); }
TEST(DecimalTest, add_3) { EXPECT_EQ(Decimal("0.908").add(Decimal("15119.106")).toString(), "15120.014"); }
TEST(DecimalTest, add_41) { EXPECT_EQ(Decimal("42745.9").add(Decimal("0.1")).toString(), "42746"); }
TEST(DecimalTest, add_42) { EXPECT_EQ(Decimal("42745").add(Decimal("0.1")).toString(), "42745.1"); }
TEST(DecimalTest, subtract_11) { EXPECT_EQ(Decimal("0.026938").subtract(Decimal("0.020411")).toString(), "0.006527"); }
TEST(DecimalTest, subtract_12) { EXPECT_EQ(Decimal("0.026938").subtract(Decimal("0.006527")).toString(), "0.020411"); }
TEST(DecimalTest, subtract_21) { EXPECT_EQ(Decimal("0.020411").subtract(Decimal("0.016527")).toString(), "0.003884"); }
TEST(DecimalTest, subtract_22) { EXPECT_EQ(Decimal("0.020411").subtract(Decimal("0.003884")).toString(), "0.016527"); }
TEST(DecimalTest, subtract_31) { EXPECT_EQ(Decimal("15120.014").subtract(Decimal("0.908")).toString(), "15119.106"); }
TEST(DecimalTest, subtract_32) { EXPECT_EQ(Decimal("15120.014").subtract(Decimal("15119.106")).toString(), "0.908"); }
TEST(DecimalTest, subtract_41) { EXPECT_EQ(Decimal("8.82412861").subtract(Decimal("0.20200000")).toString(), "8.62212861"); }
TEST(DecimalTest, subtract_42) { EXPECT_EQ(Decimal("8.00000000").subtract(Decimal("0.00000000")).toString(), "8"); }
TEST(DecimalTest, subtract_43) { EXPECT_EQ(Decimal("0.00000010").subtract(Decimal("0.00000000")).toString(), "0.0000001"); }
TEST(DecimalTest, subtract_44) { EXPECT_EQ(Decimal("0.00010000").subtract(Decimal("0.00000000")).toString(), "0.0001"); }
TEST(DecimalTest, subtract_45) { EXPECT_EQ(Decimal("0.00089990").subtract(Decimal("0.00000000")).toString(), "0.0008999"); }
TEST(DecimalTest, subtract_51) { EXPECT_EQ(Decimal("42839.6").subtract(Decimal("0.1")).toString(), "42839.5"); }
TEST(DecimalTest, subtract_52) { EXPECT_EQ(Decimal("42839").subtract(Decimal("0.1")).toString(), "42838.9"); }
TEST(DecimalTest, subtract_61) { EXPECT_EQ(Decimal("0.0135436").subtract(Decimal("0.0135436")).toString(), "0"); }
TEST(DecimalTest, subtract_62) { EXPECT_EQ(Decimal("1").subtract(Decimal("1")).toString(), "0"); }
} /* namespace ccapi */
