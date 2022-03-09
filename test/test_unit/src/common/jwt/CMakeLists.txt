set(NAME jwt)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_jwt_test.cpp)
gtest_discover_tests(${NAME})
