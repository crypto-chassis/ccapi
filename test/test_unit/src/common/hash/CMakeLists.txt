set(NAME common)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_hash_test.cpp)
gtest_discover_tests(${NAME})
