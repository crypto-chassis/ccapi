#include "ccapi_cpp/ccapi_util_private.h"
#include "gtest/gtest.h"
namespace ccapi {
TEST(UtilAlgorithmTest, base64) {
  std::string original("+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ==");
  auto result = UtilAlgorithm::base64Encode(UtilAlgorithm::base64Decode(original));
  EXPECT_EQ(result, original);
}
TEST(UtilAlgorithmTest, base64Encode) {
  std::string hex("7e3d3679f4a5677689eb2b15d7a726f3a69045f98051fcfd8b1af1ab8c168aef");
  auto result = UtilAlgorithm::base64Encode(UtilAlgorithm::hexToString(hex));
  EXPECT_EQ(result, "fj02efSlZ3aJ6ysV16cm86aQRfmAUfz9ixrxq4wWiu8=");
}
TEST(UtilAlgorithmTest, hexStringConversion) {
  std::string original("d8e30e653ff472796722cbaa816e6ae4e3b0d0ec530f857e41002b0a9f5b7f00a32bc60017e82efb2e7a43c3049d16a0");
  auto result = UtilAlgorithm::stringToHex(UtilAlgorithm::hexToString(original));
  EXPECT_EQ(result, original);
}
TEST(UtilAlgorithmTest, base64UrlFromBase64) {
  std::string original("TJVA95OrM7E2cBab30RMHrHDcEfxjoYZgeFONFh7HgQ=");
  auto result = UtilAlgorithm::base64UrlFromBase64(original);
  EXPECT_EQ(result, "TJVA95OrM7E2cBab30RMHrHDcEfxjoYZgeFONFh7HgQ");
}
TEST(UtilAlgorithmTest, base64FromBase64Url) {
  std::string original("TJVA95OrM7E2cBab30RMHrHDcEfxjoYZgeFONFh7HgQ");
  auto result = UtilAlgorithm::base64FromBase64Url(original);
  EXPECT_EQ(result, "TJVA95OrM7E2cBab30RMHrHDcEfxjoYZgeFONFh7HgQ=");
}
TEST(UtilStringTest, roundInputBySignificantFigure) {
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(12345.01, 5, 1), "12346");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(12345.01, 5, -1), "12345");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(12345.01, 5, 0), "12345");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(2345.99, 5, 1), "2346.0");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(2345.99, 5, -1), "2345.9");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(2345.99, 5, 0), "2346.0");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(0.123456, 5, 1), "0.12346");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(0.123456, 5, -1), "0.12345");
  EXPECT_EQ(UtilString::roundInputBySignificantFigure(0.123456, 5, 0), "0.12346");
}
TEST(UtilStringTest, splitHasDot) {
  std::string original("12.345");
  {
    auto result = UtilString::split(original, ".");
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result.at(0), "12");
    EXPECT_EQ(result.at(1), "345");
  }
  {
    auto result = UtilString::split(original, '.');
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result.at(0), "12");
    EXPECT_EQ(result.at(1), "345");
  }
}
TEST(UtilStringTest, splitNoDot) {
  std::string original("12");
  {
    auto result = UtilString::split(original, ".");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0), "12");
  }
  {
    auto result = UtilString::split(original, '.');
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0), "12");
  }
}
TEST(UtilStringTest, splitEmptyString) {
  std::string original("");
  {
    auto result = UtilString::split(original, ".");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0), "");
  }
  {
    auto result = UtilString::split(original, '.');
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0), "");
  }
}
TEST(UtilStringTest, splitToSetEmptyString) {
  std::string original("");
  auto result = UtilString::splitToSet(original, ".");
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(*result.begin(), "");
}
TEST(UtilStringTest, normalizeDecimalString_1) {
  std::string original("1");
  EXPECT_EQ(UtilString::normalizeDecimalString(original), "1");
  EXPECT_EQ(UtilString::normalizeDecimalString(original.c_str()), "1");
}
TEST(UtilStringTest, normalizeDecimalString_2) {
  std::string original("0");
  EXPECT_EQ(UtilString::normalizeDecimalString(original), "0");
  EXPECT_EQ(UtilString::normalizeDecimalString(original.c_str()), "0");
}
TEST(UtilStringTest, normalizeDecimalString_3) {
  std::string original("10");
  EXPECT_EQ(UtilString::normalizeDecimalString(original), "10");
  EXPECT_EQ(UtilString::normalizeDecimalString(original.c_str()), "10");
}
TEST(UtilStringTest, normalizeDecimalString_4) {
  std::string original("1.0");
  EXPECT_EQ(UtilString::normalizeDecimalString(original), "1");
  EXPECT_EQ(UtilString::normalizeDecimalString(original.c_str()), "1");
}
TEST(UtilStringTest, normalizeDecimalString_5) {
  std::string original("1.10");
  EXPECT_EQ(UtilString::normalizeDecimalString(original), "1.1");
  EXPECT_EQ(UtilString::normalizeDecimalString(original.c_str()), "1.1");
}
TEST(UtilTimeTest, divideSecondsStr_1) {
  std::string str("1634929946");
  EXPECT_EQ(UtilTime::divide(str), (std::make_pair<long long, long long>(1634929946, 0)));
}
TEST(UtilTimeTest, divideSecondsStr_2) {
  std::string str("1634929946.000");
  EXPECT_EQ(UtilTime::divide(str), (std::make_pair<long long, long long>(1634929946, 0)));
}
TEST(UtilTimeTest, divideSecondsStr_3) {
  std::string str("1634929946.010");
  EXPECT_EQ(UtilTime::divide(str), (std::make_pair<long long, long long>(1634929946, 10000000)));
}
TEST(UtilTimeTest, divideSecondsStr_4) {
  std::string str("1634929946.123");
  EXPECT_EQ(UtilTime::divide(str), (std::make_pair<long long, long long>(1634929946, 123000000)));
}
TEST(UtilTimeTest, divideMilliSecondsStr_1) {
  std::string str("1634929946000");
  EXPECT_EQ(UtilTime::divideMilli(str), (std::make_pair<long long, long long>(1634929946000, 0)));
}
TEST(UtilTimeTest, divideMilliSecondsStr_2) {
  std::string str("1634929946123");
  EXPECT_EQ(UtilTime::divideMilli(str), (std::make_pair<long long, long long>(1634929946123, 0)));
}
TEST(UtilTimeTest, divideMilliSecondsStr_3) {
  std::string str("1634929946010.000");
  EXPECT_EQ(UtilTime::divideMilli(str), (std::make_pair<long long, long long>(1634929946010, 0)));
}
TEST(UtilTimeTest, divideMilliSecondsStr_4) {
  std::string str("1634929946010.010");
  EXPECT_EQ(UtilTime::divideMilli(str), (std::make_pair<long long, long long>(1634929946010, 10000)));
}
TEST(UtilTimeTest, divideMilliSecondsStr_5) {
  std::string str("1634929946010.123");
  EXPECT_EQ(UtilTime::divideMilli(str), (std::make_pair<long long, long long>(1634929946010, 123000)));
}
} /* namespace ccapi */
