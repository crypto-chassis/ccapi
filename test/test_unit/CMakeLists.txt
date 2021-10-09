set(NAME unit_test)
project(${NAME})

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
 include_directories("${gtest_SOURCE_DIR}/include")
endif()
# find_package(GTest REQUIRED CONFIG)
include(GoogleTest)
include_directories(${CCAPI_PROJECT_DIR}/test/test_unit/include)
include_directories(${CCAPI_PROJECT_DIR}/app/include)
link_libraries(gtest_main gmock_main)
add_compile_definitions(CCAPI_EXPOSE_INTERNAL)
add_subdirectory(src/common)
add_subdirectory(src/market_data/generic)
add_subdirectory(src/execution_management/binance_usds_futures)
add_subdirectory(src/execution_management/binance_us)
add_subdirectory(src/execution_management/bitmex)
add_subdirectory(src/execution_management/coinbase)
add_subdirectory(src/execution_management/erisx)
add_subdirectory(src/execution_management/ftx)
add_subdirectory(src/execution_management/gateio)
add_subdirectory(src/execution_management/gateio_perpetual_futures)
add_subdirectory(src/execution_management/gemini)
add_subdirectory(src/execution_management/huobi)
add_subdirectory(src/execution_management/huobi_usdt_swap)
add_subdirectory(src/execution_management/huobi_coin_swap)
add_subdirectory(src/execution_management/kucoin)
add_subdirectory(src/execution_management/kraken)
add_subdirectory(src/execution_management/kraken_futures)
add_subdirectory(src/execution_management/okex)
add_subdirectory(src/app)
