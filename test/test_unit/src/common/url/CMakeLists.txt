set(NAME url)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_url_test.cpp)
gtest_discover_tests(${NAME})
