#include "ccapi_cpp/ccapi_util_private.h"
#include "gtest/gtest.h"
namespace ccapi {
TEST(HashTest, hash256ReturnHex) {
  auto result = Hash::hash(Hash::ShaVersion::SHA256, "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j",
                           "symbol=LTCBTC&side=BUY&type=LIMIT&timeInForce=GTC&quantity=1&price=0.1&recvWindow=5000&timestamp=1499827319559", true);
  EXPECT_EQ(result, "c8db56825ae71d6d79447849e617115f4a920fa2acdcab2b053c4b2838bd6b71");
}
TEST(HashTest, hash256) {
  auto result = UtilAlgorithm::base64Encode(Hash::hash(
      Hash::ShaVersion::SHA256, UtilAlgorithm::base64Decode("+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ=="),
      "1610078918POST/orders{\"size\": \"0.00005\", \"price\": \"20000\", \"side\": \"buy\", \"product_id\": \"BTC-USD\"}", false));
  EXPECT_EQ(result, "oh4uOQrCJXLUV1rmcnQvL6BTdqdcE5MYu0Q7osUH3ug=");
}
TEST(HashTest, hash512ReturnHex) {
  auto result = Hash::hash(Hash::ShaVersion::SHA256, "NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j",
                           "symbol=LTCBTC&side=BUY&type=LIMIT&timeInForce=GTC&quantity=1&price=0.1&recvWindow=5000&timestamp=1499827319559", true);
  EXPECT_EQ(result, "c8db56825ae71d6d79447849e617115f4a920fa2acdcab2b053c4b2838bd6b71");
}
TEST(HashTest, hash512) {
  auto result = UtilAlgorithm::base64Encode(Hash::hash(
      Hash::ShaVersion::SHA256, UtilAlgorithm::base64Decode("+xT7GWTDRHi09EZEhkOC8S7ktzngKtoT1ZoZ6QclGURlq3ePfUd7kLQzK4+P54685NEqYDaIerYj9cuYFILOhQ=="),
      "1610078918POST/orders{\"size\": \"0.00005\", \"price\": \"20000\", \"side\": \"buy\", \"product_id\": \"BTC-USD\"}", false));
  EXPECT_EQ(result, "oh4uOQrCJXLUV1rmcnQvL6BTdqdcE5MYu0Q7osUH3ug=");
}
} /* namespace ccapi */
