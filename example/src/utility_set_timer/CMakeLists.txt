set(NAME utility_set_timer)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)
add_executable(${NAME} main.cpp)
add_dependencies(${NAME} boost rapidjson)
