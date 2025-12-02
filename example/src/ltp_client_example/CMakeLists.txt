set(NAME ltp_client_example)
project(${NAME})

add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_PORTFOLIO_MARGIN)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKX)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BYBIT)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_HUOBI)

add_executable(${NAME}
  main.cpp
  ${CCAPI_PROJECT_DIR}/include/ccapi_cpp/ltp_trading_client.cpp
)
add_dependencies(${NAME} boost rapidjson)

# Link LTP UCLI FFI library
link_directories(/usr/local/lib)
target_link_libraries(${NAME} ucli_ffi pthread ssl crypto)

# Set runtime library search path
set_target_properties(${NAME} PROPERTIES
    BUILD_RPATH "/usr/local/lib"
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)