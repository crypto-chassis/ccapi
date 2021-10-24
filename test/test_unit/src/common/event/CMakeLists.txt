set(NAME event)
project(${NAME})
add_executable(${NAME} ${SOURCE_LOGGER} ccapi_event_test.cpp)
gtest_discover_tests(${NAME})
