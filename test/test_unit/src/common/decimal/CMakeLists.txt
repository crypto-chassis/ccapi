set(NAME decimal)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_decimal_test.cpp)
gtest_discover_tests(${NAME})
