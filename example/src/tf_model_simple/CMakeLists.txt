set(NAME tf_model_simple)
set(CMAKE_BUILD_TYPE Debug)
project(${NAME})

add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)

add_executable(${NAME} main.cpp)
target_include_directories(${NAME} PRIVATE include)
target_link_libraries (${NAME} tensorflow)
