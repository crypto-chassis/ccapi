set(NAME util)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_util_test.cpp)
gtest_discover_tests(${NAME})
