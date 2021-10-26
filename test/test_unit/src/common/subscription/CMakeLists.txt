set(NAME subscription)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_subscription_test.cpp)
gtest_discover_tests(${NAME})
