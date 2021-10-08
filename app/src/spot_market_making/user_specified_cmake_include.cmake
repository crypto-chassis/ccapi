include_guard(DIRECTORY)

# If you encountered segmentation fault at run-time, comment out the following line.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)
#
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GEMINI)
#
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KRAKEN)
#
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_US)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
#
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKEX)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KUCOIN)
#
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_FTX)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_FTX_US)
#
# find_package(ZLIB REQUIRED)
# link_libraries(ZLIB::ZLIB)

# If you are backtesting, comment out the following line.
add_compile_definitions(APP_ENABLE_LOG_INFO)

# If uncommented, the program will print out more information.
add_compile_definitions(APP_ENABLE_LOG_DEBUG)
