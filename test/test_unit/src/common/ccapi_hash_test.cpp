#include "ccapi_cpp/ccapi_util_private.h"
#include "gtest/gtest.h"
namespace ccapi {
TEST(HashTest, hash256ReturnHex) {
  auto result = UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA256,
                                           "1633827062021004nonce=1633827062021004&price=20000&volume=1&type=buy&ordertype=limit&pair=XXBTZUSD", true);
  EXPECT_EQ(result, "a6c7b5f88e73b93aa18bc09a3096aa3fa614d68594128a3f06adeaade351ea9f");
}
TEST(HashTest, hash256) {
  auto result = UtilAlgorithm::stringToHex(UtilAlgorithm::computeHash(
      UtilAlgorithm::ShaVersion::SHA256, "1633827062021004nonce=1633827062021004&price=20000&volume=1&type=buy&ordertype=limit&pair=XXBTZUSD", false));
  EXPECT_EQ(result, "a6c7b5f88e73b93aa18bc09a3096aa3fa614d68594128a3f06adeaade351ea9f");
}
TEST(HashTest, hash512ReturnHex) {
  auto result =
      UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, R"({"price":"20000","amount":"0.001","side":"buy","currency_pair":"BTC_USDT"})", true);
  EXPECT_EQ(result, "c6fa1c13d45b685ac529fa3b629a9ba74b20512985c2e82b41a426e15c78fbcd4bb11fc3af6e1dfc3f2c2cc96bbecd2504614034089af407819da1c7e145dd59");
}
TEST(HashTest, hash512) {
  auto result = UtilAlgorithm::stringToHex(
      UtilAlgorithm::computeHash(UtilAlgorithm::ShaVersion::SHA512, R"({"price":"20000","amount":"0.001","side":"buy","currency_pair":"BTC_USDT"})", false));
  EXPECT_EQ(result, "c6fa1c13d45b685ac529fa3b629a9ba74b20512985c2e82b41a426e15c78fbcd4bb11fc3af6e1dfc3f2c2cc96bbecd2504614034089af407819da1c7e145dd59");
}
} /* namespace ccapi */
