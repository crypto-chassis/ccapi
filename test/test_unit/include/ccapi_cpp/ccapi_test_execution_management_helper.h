#ifndef INCLUDE_CCAPI_CPP_TEST_EXECUTION_MANAGEMENT_HELPER_H_
#define INCLUDE_CCAPI_CPP_TEST_EXECUTION_MANAGEMENT_HELPER_H_
namespace ccapi {
inline void verifyCorrelationId(const std::vector<Message>& messageList, const std::string& correlationId) {
  for (const auto & message : messageList) {
    auto correlationIdList = message.getCorrelationIdList();
    EXPECT_EQ(correlationIdList.size(), 1);
    EXPECT_EQ(correlationIdList.at(0), correlationId);
  }
}
} /* namespace ccapi */
#endif  // INCLUDE_CCAPI_CPP_TEST_EXECUTION_MANAGEMENT_HELPER_H_
