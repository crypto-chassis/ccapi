set(NAME execution_management_binance_us)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_US)
add_executable(${NAME} ${SOURCE_LOGGER} test.cpp)
if(NOT CCAPI_LEGACY_USE_WEBSOCKETPP)
  add_dependencies(${NAME} boost rapidjson)
endif()
gtest_discover_tests(${NAME})
