set(NAME execution_management_gemini)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GEMINI)
add_executable(${NAME} ${SOURCE_LOGGER} test.cpp)
gtest_discover_tests(${NAME})
