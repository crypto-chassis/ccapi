#include "ccapi_cpp/ccapi_util_private.h"
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
    EXPECT_EQ(ConvertDecimalToString(x), "0.00000151");
  }
  {
    Decimal x("1.51E-6");
    EXPECT_EQ(ConvertDecimalToString(x), "0.00000151");
  }
  {
    Decimal x("3.14159e+000");
    EXPECT_EQ(ConvertDecimalToString(x), "3.14159");
  }
  {
    Decimal x("2.00600e+003");
    EXPECT_EQ(ConvertDecimalToString(x), "2006");
  }
  {
    Decimal x("1.00000e-010");
    EXPECT_EQ(ConvertDecimalToString(x), "0.0000000001");
  }
  {
    Decimal x("-3.50e-2");
    EXPECT_EQ(ConvertDecimalToString(x), "-0.035");
  }
}

TEST(DecimalTest, mostCommon) {
  Decimal bid_1("0.1");
  EXPECT_EQ(ConvertDecimalToString(bid_1), "0.1");
}

TEST(DecimalTest, trailingZero) {
  Decimal bid_1("0.10");
  EXPECT_EQ(ConvertDecimalToString(bid_1), "0.1");
}

TEST(DecimalTest, integer_1) {
  Decimal bid_1("1");
  EXPECT_EQ(ConvertDecimalToString(bid_1), "1");
}

TEST(DecimalTest, integer_2) {
  Decimal bid_1("1.0");
  EXPECT_EQ(ConvertDecimalToString(bid_1), "1");
}

TEST(DecimalTest, integer_3) {
  Decimal bid_1("0");
  EXPECT_EQ(ConvertDecimalToString(bid_1), "0");
}

TEST(DecimalTest, integer_4) {
  Decimal bid_1("0.0");
  EXPECT_EQ(ConvertDecimalToString(bid_1), "0");
}

TEST(DecimalTest, positiveNegativeZero) {
  Decimal a("+0");
  Decimal b("-0");
  Decimal c("0");
  EXPECT_EQ(a.sign, true);
  EXPECT_EQ(a.before, 0);
  EXPECT_EQ(a.frac, "");
  EXPECT_EQ(a, c);
  EXPECT_EQ(b.sign, true);
  EXPECT_EQ(b.before, 0);
  EXPECT_EQ(b.frac, "");
  EXPECT_EQ(b, c);
}

TEST(DecimalTest, zero) {
  Decimal a("0");
  EXPECT_EQ(a.toString(), "0");
  EXPECT_EQ(a.sign, true);
  EXPECT_EQ(a.before, 0);
  EXPECT_EQ(a.frac, "");
}

TEST(DecimalTest, compare) {
  EXPECT_TRUE(Decimal("1") == Decimal("1.0"));
  EXPECT_TRUE(Decimal("1.1") < Decimal("1.12"));
  EXPECT_TRUE(Decimal("9") < Decimal("10"));
  EXPECT_TRUE(Decimal("1") > Decimal("0"));
  EXPECT_TRUE(Decimal("-1") < Decimal("0"));
}

TEST(DecimalTest, subtract_0) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.000000549410817836") - (Decimal("0"))), "0.000000549410817836"); }

TEST(DecimalTest, add_1) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.020411") + (Decimal("0.006527"))), "0.026938"); }

TEST(DecimalTest, add_2) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.016527") + (Decimal("0.003884"))), "0.020411"); }

TEST(DecimalTest, add_3) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.908") + (Decimal("15119.106"))), "15120.014"); }

TEST(DecimalTest, add_41) { EXPECT_EQ(ConvertDecimalToString(Decimal("42745.9") + (Decimal("0.1"))), "42746"); }

TEST(DecimalTest, add_42) { EXPECT_EQ(ConvertDecimalToString(Decimal("42745") + (Decimal("0.1"))), "42745.1"); }

TEST(DecimalTest, subtract_11) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.026938") - (Decimal("0.020411"))), "0.006527"); }

TEST(DecimalTest, subtract_12) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.026938") - (Decimal("0.006527"))), "0.020411"); }

TEST(DecimalTest, subtract_21) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.020411") - (Decimal("0.016527"))), "0.003884"); }

TEST(DecimalTest, subtract_22) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.020411") - (Decimal("0.003884"))), "0.016527"); }

TEST(DecimalTest, subtract_31) { EXPECT_EQ(ConvertDecimalToString(Decimal("15120.014") - (Decimal("0.908"))), "15119.106"); }

TEST(DecimalTest, subtract_32) { EXPECT_EQ(ConvertDecimalToString(Decimal("15120.014") - (Decimal("15119.106"))), "0.908"); }

TEST(DecimalTest, subtract_41) { EXPECT_EQ(ConvertDecimalToString(Decimal("8.82412861") - (Decimal("0.20200000"))), "8.62212861"); }

TEST(DecimalTest, subtract_42) { EXPECT_EQ(ConvertDecimalToString(Decimal("8.00000000") - (Decimal("0.00000000"))), "8"); }

TEST(DecimalTest, subtract_43) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.00000010") - (Decimal("0.00000000"))), "0.0000001"); }

TEST(DecimalTest, subtract_44) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.00010000") - (Decimal("0.00000000"))), "0.0001"); }

TEST(DecimalTest, subtract_45) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.00089990") - (Decimal("0.00000000"))), "0.0008999"); }

TEST(DecimalTest, subtract_51) { EXPECT_EQ(ConvertDecimalToString(Decimal("42839.6") - (Decimal("0.1"))), "42839.5"); }

TEST(DecimalTest, subtract_52) { EXPECT_EQ(ConvertDecimalToString(Decimal("42839") - (Decimal("0.1"))), "42838.9"); }

TEST(DecimalTest, subtract_61) { EXPECT_EQ(ConvertDecimalToString(Decimal("0.0135436") - (Decimal("0.0135436"))), "0"); }

TEST(DecimalTest, subtract_62) { EXPECT_EQ(ConvertDecimalToString(Decimal("1") - (Decimal("1"))), "0"); }

// -----------------------------------------------------------------------------
//  Negate (unary minus)
// -----------------------------------------------------------------------------
TEST(DecimalTest, NegateSmall) {
  EXPECT_EQ(ConvertDecimalToString(-Decimal("0.0001")), "-0.0001");
  EXPECT_EQ(ConvertDecimalToString(-Decimal("-0.0001")), "0.0001");
  EXPECT_EQ(ConvertDecimalToString(-Decimal("0")), "0");  // -0 → 0
}

TEST(DecimalTest, NegateLarge) {
  EXPECT_EQ(ConvertDecimalToString(-Decimal("123456789012345.6789")), "-123456789012345.6789");
  EXPECT_EQ(ConvertDecimalToString(-Decimal("-987654321.000")), "987654321");
}

// -----------------------------------------------------------------------------
//  Addition corner‑cases
// -----------------------------------------------------------------------------
TEST(DecimalTest, AddDifferentScales) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("0.1") + Decimal("0.02")), "0.12");
  EXPECT_EQ(ConvertDecimalToString(Decimal("1.999") + Decimal("0.001")), "2");
  EXPECT_EQ(ConvertDecimalToString(Decimal("100") + Decimal("0.0001")), "100.0001");
}

TEST(DecimalTest, AddSigns) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("5") + Decimal("-2")), "3");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-5") + Decimal("2")), "-3");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-5") + Decimal("-2")), "-7");
}

TEST(DecimalTest, AddCarryAcrossInteger) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("9.999") + Decimal("0.001")), "10");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-9.999") + Decimal("-0.001")), "-10");
}

// -----------------------------------------------------------------------------
//  Subtraction corner‑cases
// -----------------------------------------------------------------------------
TEST(DecimalTest, SubtractDifferentScales) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("0.12") - Decimal("0.02")), "0.1");
  EXPECT_EQ(ConvertDecimalToString(Decimal("2") - Decimal("1.999")), "0.001");
  EXPECT_EQ(ConvertDecimalToString(Decimal("100.0001") - Decimal("0.0001")), "100");
}

TEST(DecimalTest, SubtractSigns) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("5") - Decimal("-2")), "7");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-5") - Decimal("2")), "-7");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-5") - Decimal("-2")), "-3");
}

TEST(DecimalTest, SubtractBorrowAcrossInteger) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("10") - Decimal("0.001")), "9.999");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-10") - Decimal("-0.001")), "-9.999");
}

// -----------------------------------------------------------------------------
//  Large‑magnitude checks (> 1e18 total digits)
// -----------------------------------------------------------------------------
TEST(DecimalTest, AddVeryLarge) {
  Decimal a("12345678901234567890.123456789");
  Decimal b("0.876543211");
  EXPECT_EQ(ConvertDecimalToString(a + b), "12345678901234567891");
}

TEST(DecimalTest, SubtractVeryLarge) {
  Decimal a("12345678901234567891");
  Decimal b("0.876543211");
  EXPECT_EQ(ConvertDecimalToString(a - b), "12345678901234567890.123456789");
}

// -----------------------------------------------------------------------------
//  Identity & zero properties
// -----------------------------------------------------------------------------
TEST(DecimalTest, AddZero) {
  Decimal x("123.456");
  EXPECT_EQ(x + Decimal("0"), x);
  EXPECT_EQ(Decimal("0") + x, x);
}

TEST(DecimalTest, SubtractZero) {
  Decimal x("-789.012");
  EXPECT_EQ(x - Decimal("0"), x);
  EXPECT_EQ(ConvertDecimalToString(Decimal("0") - x), "789.012");
}

TEST(DecimalTest, NegateTwice) {
  Decimal x("987.65");
  EXPECT_EQ(-(-x), x);
}

TEST(DecimalTest, MultiplyAssignOperatorIntegral) {
  // Original cases
  Decimal d("2.5");
  d *= 4;
  EXPECT_EQ(ConvertDecimalToString(d), "10");

  Decimal d2("-0.25");
  d2 *= -8;
  EXPECT_EQ(ConvertDecimalToString(d2), "2");

  Decimal d3("100.00001");
  d3 *= 1;
  EXPECT_EQ(ConvertDecimalToString(d3), "100.00001");

  Decimal d4("123456.789");
  d4 *= 0;
  EXPECT_EQ(ConvertDecimalToString(d4), "0");

  // Additional cases

  // Multiplying by -1
  Decimal d5("123.456");
  d5 *= -1;
  EXPECT_EQ(ConvertDecimalToString(d5), "-123.456");

  // Multiplying negative by negative
  Decimal d6("-123.456");
  d6 *= -2;
  EXPECT_EQ(ConvertDecimalToString(d6), "246.912");

  // Multiplying negative by positive
  Decimal d7("-1.5");
  d7 *= 3;
  EXPECT_EQ(ConvertDecimalToString(d7), "-4.5");

  // Multiplying very small decimal by large integer
  Decimal d8("0.0000002");
  d8 *= 3500000;
  EXPECT_EQ(ConvertDecimalToString(d8), "0.7");

  // Multiplying very large decimal by small integer
  Decimal d9("999999999.999999");
  d9 *= 2;
  EXPECT_EQ(ConvertDecimalToString(d9), "1999999999.999998");

  // Multiplying 0 by any number
  Decimal d10("0");
  d10 *= 123456789;
  EXPECT_EQ(ConvertDecimalToString(d10), "0");

  // Multiplying by 0 with negative number
  Decimal d11("-999.99");
  d11 *= 0;
  EXPECT_EQ(ConvertDecimalToString(d11), "0");

  // Multiplying whole number
  Decimal d12("50");
  d12 *= 2;
  EXPECT_EQ(ConvertDecimalToString(d12), "100");

  // Multiplying very small negative by negative
  Decimal d13("-0.000001");
  d13 *= -1000000;
  EXPECT_EQ(ConvertDecimalToString(d13), "1");

  // Multiplying repeating decimal
  Decimal d14("0.3333");
  d14 *= 3;
  EXPECT_EQ(ConvertDecimalToString(d14), "0.9999");

  // Multiplying very large integer
  Decimal d15("1");
  d15 *= 1000000000000000LL;
  EXPECT_EQ(ConvertDecimalToString(d15), "1000000000000000");

  // Multiplying high-precision decimal
  Decimal d16("0.123456789123456789");
  d16 *= 10;
  EXPECT_EQ(ConvertDecimalToString(d16), "1.23456789123456789");

  // Multiplying by 1 (should be no change)
  Decimal d17("8888.8888");
  d17 *= 1;
  EXPECT_EQ(ConvertDecimalToString(d17), "8888.8888");

  // Multiplying negative decimal by 1
  Decimal d18("42.42");
  d18 *= 1;
  EXPECT_EQ(ConvertDecimalToString(d18), "42.42");

  // Multiplying negative decimal by -1
  Decimal d19("42.42");
  d19 *= -1;
  EXPECT_EQ(ConvertDecimalToString(d19), "-42.42");
}

TEST(DecimalTest, AbsZeroVariants) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("0").abs()), "0");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-0").abs()), "0");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-0.0").abs()), "0");
  EXPECT_EQ(ConvertDecimalToString(Decimal("0.000").abs()), "0");
}

TEST(DecimalTest, AbsPositiveIntegers) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("1").abs()), "1");
  EXPECT_EQ(ConvertDecimalToString(Decimal("42").abs()), "42");
  EXPECT_EQ(ConvertDecimalToString(Decimal("999999999").abs()), "999999999");
}

TEST(DecimalTest, AbsNegativeIntegers) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("-1").abs()), "1");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-42").abs()), "42");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-999999999").abs()), "999999999");
}

TEST(DecimalTest, AbsPositiveDecimals) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("3.14").abs()), "3.14");
  EXPECT_EQ(ConvertDecimalToString(Decimal("0.0001").abs()), "0.0001");
  EXPECT_EQ(ConvertDecimalToString(Decimal("123.456000").abs()), "123.456");
}

TEST(DecimalTest, AbsNegativeDecimals) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("-3.14").abs()), "3.14");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-0.0001").abs()), "0.0001");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-123.456000").abs()), "123.456");
}

TEST(DecimalTest, AbsVeryLargeNumbers) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("-999999999.999999").abs()), "999999999.999999");
  EXPECT_EQ(ConvertDecimalToString(Decimal("18446744073709551615").abs()), "18446744073709551615");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-18446744073709551615").abs()), "18446744073709551615");
}

TEST(DecimalTest, AbsWithLeadingZeros) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("000123.45000").abs()), "123.45");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-000123.45000").abs()), "123.45");
}

TEST(DecimalTest, AbsScientificNotationStyleInputs) {
  EXPECT_EQ(ConvertDecimalToString(Decimal("1.23e3").abs()), "1230");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-1.23e3").abs()), "1230");
  EXPECT_EQ(ConvertDecimalToString(Decimal("1.23e-3").abs()), "0.00123");
  EXPECT_EQ(ConvertDecimalToString(Decimal("-1.23e-3").abs()), "0.00123");
}

} /* namespace ccapi */
