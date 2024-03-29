set(NAME binding_${LANG})
project(${NAME})
set(SWIG_TARGET_NAME ccapi_${NAME})

# Find javascript cli
find_program(NODE_EXECUTABLE NAMES node REQUIRED)
if(NOT NODE_EXECUTABLE)
  message(FATAL_ERROR "Check for node Program: not found")
else()
  message(STATUS "Found node Program: ${NODE_EXECUTABLE}")
endif()

execute_process(
  COMMAND ${NODE_EXECUTABLE} -v
  OUTPUT_VARIABLE NODE_EXECUTABLE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "node version: ${NODE_EXECUTABLE_VERSION}")

find_program(NODE_GYP_EXECUTABLE NAMES node-gyp REQUIRED)
if(NOT NODE_GYP_EXECUTABLE)
  message(FATAL_ERROR "Check for node-gyp Program: not found")
else()
  message(STATUS "Found node-gyp Program: ${NODE_EXECUTABLE}")
endif()

execute_process(
  COMMAND ${NODE_GYP_EXECUTABLE} -v
  OUTPUT_VARIABLE NODE_GYP_EXECUTABLE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "node-gyp version: ${NODE_GYP_EXECUTABLE_VERSION}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME})
set(BINDING_GYP_CFLAGS_CC_LIST "")
set(BINDING_GYP_LDFLAGS_LIST "")
set(BINDING_GYP_DEFINES_LIST "")
list(APPEND BINDING_GYP_CFLAGS_CC_LIST -std=c++17 -fPIC -Wno-deprecated-declarations)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
list(APPEND BINDING_GYP_CFLAGS_CC_LIST -g)
list(APPEND BINDING_GYP_LDFLAGS_LIST -g)
else()
list(APPEND BINDING_GYP_CFLAGS_CC_LIST -O3 -DNDEBUG)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
list(APPEND BINDING_GYP_CFLAGS_CC_LIST -pthread)
endif()
list(APPEND BINDING_GYP_DEFINES_LIST CCAPI_ENABLE_SERVICE_MARKET_DATA CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT CCAPI_ENABLE_SERVICE_FIX CCAPI_ENABLE_EXCHANGE_COINBASE CCAPI_ENABLE_EXCHANGE_GEMINI CCAPI_ENABLE_EXCHANGE_KRAKEN CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES CCAPI_ENABLE_EXCHANGE_BITSTAMP CCAPI_ENABLE_EXCHANGE_BITFINEX CCAPI_ENABLE_EXCHANGE_BITMEX CCAPI_ENABLE_EXCHANGE_BINANCE_US CCAPI_ENABLE_EXCHANGE_BINANCE CCAPI_ENABLE_EXCHANGE_BINANCE_MARGIN CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES CCAPI_ENABLE_EXCHANGE_HUOBI CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP CCAPI_ENABLE_EXCHANGE_OKX CCAPI_ENABLE_EXCHANGE_ERISX CCAPI_ENABLE_EXCHANGE_KUCOIN CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES CCAPI_ENABLE_EXCHANGE_DERIBIT CCAPI_ENABLE_EXCHANGE_GATEIO CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES CCAPI_ENABLE_EXCHANGE_CRYPTOCOM CCAPI_ENABLE_EXCHANGE_BYBIT CCAPI_ENABLE_EXCHANGE_BYBIT_DERIVATIVES CCAPI_ENABLE_EXCHANGE_ASCENDEX CCAPI_ENABLE_EXCHANGE_BITGET CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES CCAPI_ENABLE_EXCHANGE_BITMART CCAPI_ENABLE_EXCHANGE_MEXC CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES CCAPI_ENABLE_EXCHANGE_WHITEBIT)
if(CCAPI_ENABLE_LOG_ERROR)
list(APPEND BINDING_GYP_DEFINES_LIST CCAPI_ENABLE_LOG_ERROR)
endif()
if(CCAPI_ENABLE_LOG_WARN)
list(APPEND BINDING_GYP_DEFINES_LIST CCAPI_ENABLE_LOG_WARN)
endif()
if(CCAPI_ENABLE_LOG_INFO)
list(APPEND BINDING_GYP_DEFINES_LIST CCAPI_ENABLE_LOG_INFO)
endif()
if(CCAPI_ENABLE_LOG_DEBUG)
list(APPEND BINDING_GYP_DEFINES_LIST CCAPI_ENABLE_LOG_DEBUG)
endif()
if(CCAPI_ENABLE_LOG_TRACE)
list(APPEND BINDING_GYP_DEFINES_LIST CCAPI_ENABLE_LOG_TRACE)
endif()
list(JOIN BINDING_GYP_CFLAGS_CC_LIST "\",\"" BINDING_GYP_CFLAGS_CC)
message(STATUS "BINDING_GYP_CFLAGS_CC: ${BINDING_GYP_CFLAGS_CC}")
list(JOIN BINDING_GYP_LDFLAGS_LIST "\",\"" BINDING_GYP_LDFLAGS)
message(STATUS "BINDING_GYP_LDFLAGS: ${BINDING_GYP_LDFLAGS}")
list(JOIN BINDING_GYP_DEFINES_LIST "\",\"" BINDING_GYP_DEFINES)
message(STATUS "BINDING_GYP_DEFINES: ${BINDING_GYP_DEFINES}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/binding.gyp.in
  ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}/binding.gyp
@ONLY)
if(NOT NODE_GYP_BUILD_MODE)
  set(NODE_GYP_BUILD_MODE "release" CACHE STRING
    "Choose the type of build, options are: debug, release."
    FORCE)
endif()
if("${NODE_GYP_BUILD_MODE}" STREQUAL "release")
  set(NODE_GYP_BUILD_DIR "build/Release")
else()
set(NODE_GYP_BUILD_DIR "build/Debug")
endif()
message(STATUS "NODE_GYP_BUILD_MODE: ${NODE_GYP_BUILD_MODE}")
add_custom_target(${SWIG_TARGET_NAME} ALL
  COMMAND ${SWIG_EXECUTABLE} -outcurrentdir -c++ -javascript -node -I${CCAPI_PROJECT_DIR}/include ${SWIG_INTERFACE}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/ccapi_logger.cpp .
  COMMAND ${NODE_GYP_EXECUTABLE} --${NODE_GYP_BUILD_MODE} clean configure build
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}
)
add_dependencies(${SWIG_TARGET_NAME} boost rapidjson hffix)

set(PACKAGING_DIR packaging)
set(PACKAGING_DIR_FULL ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGING_DIR}/${BUILD_VERSION})
file(MAKE_DIRECTORY ${PACKAGING_DIR_FULL})
set(JAVASCRIPT_PACKAGING_TARGET_NAME ${LANG}_${PACKAGING_DIR})
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/package.json.in
  ${PACKAGING_DIR_FULL}/package.json
@ONLY)
add_custom_target(${JAVASCRIPT_PACKAGING_TARGET_NAME} ALL
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}/${NODE_GYP_BUILD_DIR}/index.node ${PACKAGING_DIR_FULL}
  WORKING_DIRECTORY ${PACKAGING_DIR_FULL}
)
add_dependencies(${JAVASCRIPT_PACKAGING_TARGET_NAME} ${SWIG_TARGET_NAME})

# Test
if(BUILD_TEST)
  set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)
  find_program(NPM_EXECUTABLE NAMES npm REQUIRED)
  if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "Check for npm Program: not found")
  else()
    message(STATUS "Found npm Program: ${NPM_EXECUTABLE}")
  endif()
  file(COPY test DESTINATION ${TEST_DIR})
  set(JAVASCRIPT_TEST_TARGET_NAME javascript_test_test)
    set(JAVASCRIPT_TEST_BUILD_DIRECTORY ${TEST_DIR}/test)
    file(MAKE_DIRECTORY ${JAVASCRIPT_TEST_BUILD_DIRECTORY})
    add_custom_target(${JAVASCRIPT_TEST_TARGET_NAME} ALL
      COMMAND ${CMAKE_COMMAND} -E rm -rf node_modules
      COMMAND ${NPM_EXECUTABLE} install ${PACKAGING_DIR_FULL}
      WORKING_DIRECTORY ${JAVASCRIPT_TEST_BUILD_DIRECTORY}
    )
    add_dependencies(${JAVASCRIPT_TEST_TARGET_NAME} ${JAVASCRIPT_PACKAGING_TARGET_NAME})
    add_test(NAME javascript_test
    COMMAND ${NODE_EXECUTABLE} index.js
  WORKING_DIRECTORY ${JAVASCRIPT_TEST_BUILD_DIRECTORY})
endif()
