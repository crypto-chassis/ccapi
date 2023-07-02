cmake_minimum_required(VERSION 3.14)
set(NAME app)
project(${NAME})
if(NOT "${CMAKE_CXX_STANDARD}")
  set(CMAKE_CXX_STANDARD 17)
endif()
message(STATUS "CMAKE_CXX_STANDARD: ${CMAKE_CXX_STANDARD}")
if(NOT APPLE AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
set(OPENSSL_USE_STATIC_LIBS TRUE)
if (WIN32)
  set(CMAKE_BUILD_TYPE "Release")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
get_filename_component(CCAPI_PROJECT_DIR ../ ABSOLUTE)
message(STATUS "CCAPI_PROJECT_DIR: ${CCAPI_PROJECT_DIR}")
if(NOT CCAPI_LEGACY_USE_WEBSOCKETPP)
  message(STATUS "use boost beast websocket")
  if(NOT BOOST_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(boost
    URL    https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.gz
    SOURCE_DIR        "${CMAKE_BINARY_DIR}/boost"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
  )
  set(BOOST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/boost")
  else()
  add_library(boost INTERFACE)
  endif()
  if(NOT RAPIDJSON_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(rapidjson
    URL    https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz
    SOURCE_DIR        "${CMAKE_BINARY_DIR}/rapidjson"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
  )
  set(RAPIDJSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/rapidjson/include")
  else()
  add_library(rapidjson INTERFACE)
  endif()
	include_directories(${CCAPI_PROJECT_DIR}/include ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
else()
  message(STATUS "use websocketpp")
  add_compile_definitions(CCAPI_LEGACY_USE_WEBSOCKETPP)
  set(RAPIDJSON_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/rapidjson/include)
  set(WEBSOCKETPP_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/websocketpp)
  set(BOOST_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/boost)
  include_directories(${CCAPI_PROJECT_DIR}/include ${WEBSOCKETPP_INCLUDE_DIR} ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
endif()
set(APP_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/app/include)
find_package(OpenSSL REQUIRED)
include_directories(${APP_INCLUDE_DIR})
set(ADDITIONAL_LINK_LIBRARIES "")
if (WIN32)
  set(ADDITIONAL_LINK_LIBRARIES ws2_32)
endif()
link_libraries(OpenSSL::Crypto OpenSSL::SSL ${ADDITIONAL_LINK_LIBRARIES})
add_compile_options(-Wno-deprecated -Wno-nonnull -Wno-deprecated-declarations)

# If you encountered segmentation fault at run-time, comment out the following line.
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GEMINI)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KRAKEN)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITSTAMP)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITFINEX)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_US)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKX)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KUCOIN)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GATEIO)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_CRYPTOCOM)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BYBIT)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_ASCENDEX)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITGET)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITMART)
#
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_MEXC)
#
find_package(ZLIB REQUIRED)
link_libraries(ZLIB::ZLIB)

# If backtesting, you may want to comment out the following line to improve run-time speed.
add_compile_definitions(CCAPI_APP_ENABLE_LOG_INFO)

# If uncommented, the program will print out more information.
# add_compile_definitions(CCAPI_APP_ENABLE_LOG_DEBUG)

# If you have custom logics and want to reuse part of class EventHandlerBase.
# add_compile_definitions(CCAPI_APP_USE_CUSTOM_EVENT_HANDLER)
# get_filename_component(CUSTOM_EVENT_HANDLER_DIRECTORY ${CUSTOM_EVENT_HANDLER_PATH} DIRECTORY)
# include_directories(${CUSTOM_EVENT_HANDLER_DIRECTORY})

# If you have custom logics and need to repeatedly rebuild for backtesting, you may want to comment out the following line to improve build-time speed. If so, make sure to
# provide additional environment variables BASE_ASSET_OVERRIDE, QUOTE_ASSET_OVERRIDE, ORDER_PRICE_INCREMENT_OVERRIDE, and ORDER_QUANTITY_INCREMENT_OVERRIDE at run time.
# add_compile_definitions(CCAPI_APP_IS_BACKTEST)

add_subdirectory(src/spot_market_making)
add_subdirectory(src/single_order_execution)
