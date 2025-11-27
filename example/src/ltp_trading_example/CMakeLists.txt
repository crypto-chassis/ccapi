set(NAME ltp_trading_example)
project(${NAME})
add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_PORTFOLIO_MARGIN)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKX)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BYBIT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_HUOBI)

# 获取项目根目录
get_filename_component(CCAPI_PROJECT_DIR ../../.. ABSOLUTE)

# 添加 LTP UCLI FFI 头文件路径
include_directories(${CCAPI_PROJECT_DIR}/example/src)

# 主测试程序 - 连通性测试
add_executable(${NAME} main.cpp)
add_dependencies(${NAME} boost rapidjson)

# 链接 LTP UCLI FFI 库
# 如果库在 /usr/local/lib，确保链接器能找到它
link_directories(/usr/local/lib)
target_link_libraries(${NAME} ucli_ffi pthread ssl crypto)

# 压测程序 - 币安U本位合约WebSocket压测
set(NAME_STRESS ltp_trading_stress_test)
add_executable(${NAME_STRESS} stress_test.cpp)
add_dependencies(${NAME_STRESS} boost rapidjson)
# 压测程序也需要链接 LTP UCLI FFI 库（如果使用）
target_link_libraries(${NAME_STRESS} ucli_ffi pthread ssl crypto)