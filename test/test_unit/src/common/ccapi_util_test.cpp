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
TEST(UtilStringTest, splitHasDot) {
  std::string original("12.345");
  auto result = UtilString::split(original, ".");
  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result.at(0), "12");
  EXPECT_EQ(result.at(1), "345");
}
TEST(UtilStringTest, splitNoDot) {
  std::string original("12");
  auto result = UtilString::split(original, ".");
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result.at(0), "12");
}
TEST(UtilStringTest, splitEmptyString) {
  std::string original("");
  auto result = UtilString::split(original, ".");
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result.at(0), "");
}
} /* namespace ccapi */
