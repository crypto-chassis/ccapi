set(NAME execution_management_simple_request)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
add_executable(${NAME} main.cpp)
add_dependencies(${NAME} boost rapidjson)
