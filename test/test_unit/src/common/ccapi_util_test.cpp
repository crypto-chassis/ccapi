#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_util.h"
namespace ccapi {
TEST(UtilAlgorithmTest, base64) {
  auto result = UtilAlgorithm::base64Encode(UtilAlgorithm::base64Decode("+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ=="));
  EXPECT_EQ(result, "+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ==");
}
} /* namespace ccapi */
