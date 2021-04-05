#include "ccapi_cpp/ccapi_url.h"
#include "gtest/gtest.h"
namespace ccapi {
TEST(UrlTest, urlEncode) {
  auto result = Url::urlEncode("12!@&34?=");
  EXPECT_EQ(result, "12%21%40%2634%3F%3D");
}
TEST(UrlTest, urlDecode) {
  auto result = Url::urlDecode("12%21%40%2634%3F%3D");
  EXPECT_EQ(result, "12!@&34?=");
}
} /* namespace ccapi */
