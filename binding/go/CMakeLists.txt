set(NAME binding_${LANG})
project(${NAME})
set(SWIG_TARGET_NAME ccapi_${NAME})

# Find go cli
find_program(GO_EXECUTABLE NAMES go REQUIRED)
if(NOT GO_EXECUTABLE)
  message(FATAL_ERROR "Check for go Program: not found")
else()
  message(STATUS "Found go Program: ${GO_EXECUTABLE}")
endif()

execute_process(
  COMMAND ${GO_EXECUTABLE} version
  OUTPUT_VARIABLE GO_EXECUTABLE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Go version: ${GO_EXECUTABLE_VERSION}")

set(GO_PACKAGE "ccapi")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME})

add_custom_target(${SWIG_TARGET_NAME} ALL
  COMMAND ${SWIG_EXECUTABLE} -outcurrentdir -c++ -go -intgosize 64 -package ${GO_PACKAGE} -I${CCAPI_PROJECT_DIR}/include ${SWIG_INTERFACE}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}
)
add_dependencies(${SWIG_TARGET_NAME} boost rapidjson hffix)

set(PACKAGING_DIR packaging)
set(PACKAGING_DIR_FULL ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGING_DIR}/${BUILD_VERSION})
file(MAKE_DIRECTORY ${PACKAGING_DIR_FULL})
set(GO_PACKAGING_TARGET_NAME ${LANG}_${PACKAGING_DIR})

set(COMPILER_OPTIONS_FILE_PATH ${PACKAGING_DIR_FULL}/export_compiler_options.sh)
file(WRITE ${COMPILER_OPTIONS_FILE_PATH} "")
set(CGO_CXXFLAGS_LIST "")
list(APPEND CGO_CXXFLAGS_LIST -std=c++17 -fPIC -Wno-deprecated-declarations)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
list(APPEND CGO_CXXFLAGS_LIST -g)
else()
list(APPEND CGO_CXXFLAGS_LIST -O3 -DNDEBUG)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
list(APPEND CGO_CXXFLAGS_LIST -pthread)
endif()
list(APPEND CGO_CXXFLAGS_LIST -I${CCAPI_PROJECT_DIR}/include -I${BOOST_INCLUDE_DIR} -I${RAPIDJSON_INCLUDE_DIR} -I${HFFIX_INCLUDE_DIR} -I${OPENSSL_INCLUDE_DIR})
list(APPEND CGO_CXXFLAGS_LIST -DCCAPI_ENABLE_SERVICE_MARKET_DATA -DCCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT -DCCAPI_ENABLE_SERVICE_FIX -DCCAPI_ENABLE_EXCHANGE_COINBASE -DCCAPI_ENABLE_EXCHANGE_GEMINI -DCCAPI_ENABLE_EXCHANGE_KRAKEN -DCCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES -DCCAPI_ENABLE_EXCHANGE_BITSTAMP -DCCAPI_ENABLE_EXCHANGE_BITFINEX -DCCAPI_ENABLE_EXCHANGE_BITMEX -DCCAPI_ENABLE_EXCHANGE_BINANCE_US -DCCAPI_ENABLE_EXCHANGE_BINANCE -DCCAPI_ENABLE_EXCHANGE_BINANCE_MARGIN -DCCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES -DCCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES -DCCAPI_ENABLE_EXCHANGE_HUOBI -DCCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP -DCCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP -DCCAPI_ENABLE_EXCHANGE_OKX -DCCAPI_ENABLE_EXCHANGE_ERISX -DCCAPI_ENABLE_EXCHANGE_KUCOIN -DCCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES -DCCAPI_ENABLE_EXCHANGE_DERIBIT -DCCAPI_ENABLE_EXCHANGE_GATEIO -DCCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES -DCCAPI_ENABLE_EXCHANGE_CRYPTOCOM -DCCAPI_ENABLE_EXCHANGE_BYBIT -DCCAPI_ENABLE_EXCHANGE_BYBIT_DERIVATIVES -DCCAPI_ENABLE_EXCHANGE_ASCENDEX -DCCAPI_ENABLE_EXCHANGE_BITGET -DCCAPI_ENABLE_EXCHANGE_BITGET_FUTURES -DCCAPI_ENABLE_EXCHANGE_BITMART -DCCAPI_ENABLE_EXCHANGE_MEXC -DCCAPI_ENABLE_EXCHANGE_MEXC_FUTURES -DCCAPI_ENABLE_EXCHANGE_WHITEBIT)
if(CCAPI_ENABLE_LOG_ERROR)
list(APPEND CGO_CXXFLAGS_LIST -DCCAPI_ENABLE_LOG_ERROR)
endif()
if(CCAPI_ENABLE_LOG_WARN)
list(APPEND CGO_CXXFLAGS_LIST -DCCAPI_ENABLE_LOG_WARN)
endif()
if(CCAPI_ENABLE_LOG_INFO)
list(APPEND CGO_CXXFLAGS_LIST -DCCAPI_ENABLE_LOG_INFO)
endif()
if(CCAPI_ENABLE_LOG_DEBUG)
list(APPEND CGO_CXXFLAGS_LIST -DCCAPI_ENABLE_LOG_DEBUG)
endif()
if(CCAPI_ENABLE_LOG_TRACE)
list(APPEND CGO_CXXFLAGS_LIST -DCCAPI_ENABLE_LOG_TRACE)
endif()
list(JOIN CGO_CXXFLAGS_LIST " " CGO_CXXFLAGS)
message(STATUS "CGO_CXXFLAGS: ${CGO_CXXFLAGS}")
file(APPEND ${COMPILER_OPTIONS_FILE_PATH} "export CGO_CXXFLAGS='${CGO_CXXFLAGS}'\n\n")

set(CGO_LDFLAGS_LIST "")
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
list(APPEND CGO_LDFLAGS_LIST -g)
endif()
list(APPEND CGO_LDFLAGS_LIST -L${OPENSSL_CRYPTO_LIBRARY_DIR} -L${OPENSSL_SSL_LIBRARY_DIR} -lcrypto -lssl)
foreach(ZLIB_LIBRARY IN LISTS ZLIB_LIBRARIES)
  get_filename_component(ZLIB_LIBRARY_DIR ${ZLIB_LIBRARY} DIRECTORY)
  list(APPEND CGO_LDFLAGS_LIST -L${ZLIB_LIBRARY_DIR} -lz)
endforeach()
list(JOIN CGO_LDFLAGS_LIST " " CGO_LDFLAGS)
message(STATUS "CGO_LDFLAGS: ${CGO_LDFLAGS}")
file(APPEND ${COMPILER_OPTIONS_FILE_PATH} "export CGO_LDFLAGS='${CGO_LDFLAGS}'\n")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/go.mod.in
  ${PACKAGING_DIR_FULL}/go.mod
@ONLY)
add_custom_target(${GO_PACKAGING_TARGET_NAME} ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME} ${PACKAGING_DIR_FULL}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/go.mod.in ${PACKAGING_DIR_FULL}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/ccapi_logger.cpp ${PACKAGING_DIR_FULL}
  WORKING_DIRECTORY ${PACKAGING_DIR_FULL}
)
add_dependencies(${GO_PACKAGING_TARGET_NAME} ${SWIG_TARGET_NAME})

# Test
if(BUILD_TEST)
  set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)
  file(COPY example DESTINATION ${TEST_DIR})
  set(SUBDIRECTORY_LIST "enable_library_logging" "execution_management_simple_request" "execution_management_simple_subscription" "fix_simple" "handle_exception" "market_data_multiple_subscription" "market_data_simple_request" "market_data_simple_subscription")
  file(READ ${PACKAGING_DIR_FULL}/export_compiler_options.sh EXPORT_COMPILER_OPTIONS)
  string(APPEND EXPORT_COMPILER_OPTIONS "${GO_EXECUTABLE} build .")
  file(WRITE ${PACKAGING_DIR_FULL}/build.sh "${EXPORT_COMPILER_OPTIONS}")
  foreach(SUBDIRECTORY IN LISTS SUBDIRECTORY_LIST)
    set(GO_TEST_TARGET_NAME go_test_${SUBDIRECTORY})
    set(GO_TEST_BUILD_DIRECTORY ${TEST_DIR}/example/${SUBDIRECTORY})
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/go.mod.in_2
      ${GO_TEST_BUILD_DIRECTORY}/go.mod
    @ONLY)
    add_custom_target(${GO_TEST_TARGET_NAME} ALL
      COMMAND bash ${PACKAGING_DIR_FULL}/build.sh
      WORKING_DIRECTORY ${GO_TEST_BUILD_DIRECTORY}
    )
    add_dependencies(${GO_TEST_TARGET_NAME} ${GO_PACKAGING_TARGET_NAME})
  endforeach()
  file(COPY test DESTINATION ${TEST_DIR})
  set(GO_TEST_TARGET_NAME go_test_test)
    set(GO_TEST_BUILD_DIRECTORY ${TEST_DIR}/test)
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/go.mod.in_2
      ${GO_TEST_BUILD_DIRECTORY}/go.mod
    @ONLY)
    add_custom_target(${GO_TEST_TARGET_NAME} ALL
      COMMAND bash ${PACKAGING_DIR_FULL}/build.sh
      WORKING_DIRECTORY ${GO_TEST_BUILD_DIRECTORY}
    )
    add_dependencies(${GO_TEST_TARGET_NAME} ${GO_PACKAGING_TARGET_NAME})
    add_test(NAME go_test
    COMMAND ${GO_TEST_BUILD_DIRECTORY}/main
  WORKING_DIRECTORY ${GO_TEST_BUILD_DIRECTORY})
endif()
