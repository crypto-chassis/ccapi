set(NAME market_data_edgex_depth)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_EDGEX)
add_executable(${NAME} main.cpp)
add_dependencies(${NAME} boost rapidjson)

