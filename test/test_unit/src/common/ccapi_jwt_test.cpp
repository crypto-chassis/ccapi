#include "gtest/gtest.h"
#include "ccapi_cpp/ccapi_jwt.h"
namespace ccapi {
TEST(JwtTest, generate) {
  auto result = Jwt::generate(Hmac::ShaVersion::SHA256,
                           "your-256-bit-secret",
                           "{\"sub\":\"1234567890\",\"name\":\"John Doe\",\"iat\":1516239022}");
  EXPECT_EQ(result, "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c");
}
} /* namespace ccapi */
