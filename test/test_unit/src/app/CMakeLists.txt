set(NAME app)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} common_test.cpp)
gtest_discover_tests(${NAME})
