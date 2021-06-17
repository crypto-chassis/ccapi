set(NAME execution_management_advanced_subscription)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKEX)
add_compile_definitions(CCAPI_ENABLE_LOG_TRACE)
# "wss://ws.okex.com:8443/ws/v5"
add_compile_definitions(CCAPI_OKEX_URL_WS_BASE="wss://wspap.okex.com:8443")
add_compile_definitions(CCAPI_OKEX_PRIVATE_WS_TARGET="/ws/v5/private?brokerId=9999")
add_executable(${NAME} main.cpp)
