set(NAME unit_test)
project(${NAME})
find_package(GTest REQUIRED)
add_subdirectory(execution_management/binance_us)