cmake_minimum_required(VERSION 3.17)
set(NAME binding)
set(BUILD_VERSION "1.0.0" CACHE STRING "User-specified build version")
message(STATUS "BUILD_VERSION: ${BUILD_VERSION}")
project(${NAME} LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

if(POLICY CMP0122)
cmake_policy(SET CMP0122 NEW)
endif()

# Apple: Don't modify install_name when touching RPATH.
if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
endif()

# Define file(GENERATE) behavior for relative paths.
if(POLICY CMP0070)
  cmake_policy(SET CMP0070 NEW)
endif()

# target_sources: use absolute path for INTERFACE_SOURCES.
if(POLICY CMP0076)
  cmake_policy(SET CMP0076 NEW)
endif()

# option() honors normal variables.
# see: https://cmake.org/cmake/help/git-stage/policy/CMP0077.html
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

# SWIG: use standard target name.
if(POLICY CMP0078)
  cmake_policy(SET CMP0078 NEW)
endif()

# SWIG: use SWIG_MODULE_NAME property.
if(POLICY CMP0086)
  cmake_policy(SET CMP0086 NEW)
endif()

# Force default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug, Release (default), RelWithDebInfo and MinSizeRel."
    FORCE)
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# Layout build dir like install dir
include(GNUInstallDirs)
if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries(.so or .dyld)." ON)
  message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  # for multi-config build system (e.g. Xcode, ninja Multi-Config)
  foreach(OUTPUTCONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OUTPUTCONFIG_UPPER} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_LIBDIR})
  endforeach()
else()
  # Currently Only support static build for windows
  set(BUILD_SHARED_LIBS OFF)
  message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config builds (e.g. msvc)
  foreach(OUTPUTCONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OUTPUTCONFIG_UPPER} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

option(BUILD_PYTHON "Build Python Library" OFF)
message(STATUS "Build Python: ${BUILD_PYTHON}")

option(BUILD_JAVA "Build Java Library" OFF)
message(STATUS "Build Java: ${BUILD_JAVA}")

option(BUILD_CSHARP "Build C# Library" OFF)
message(STATUS "Build C#: ${BUILD_CSHARP}")

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
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
  if(NOT HFFIX_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(hffix
    URL    https://github.com/jamesdbrock/hffix/archive/refs/tags/v1.3.0.tar.gz
    SOURCE_DIR        "${CMAKE_BINARY_DIR}/hffix"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
  )
  set(HFFIX_INCLUDE_DIR "${CMAKE_BINARY_DIR}/hffix/include")
  else()
  add_library(hffix INTERFACE)
  endif()
	include_directories(${CCAPI_PROJECT_DIR}/include ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${HFFIX_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
else()
  message(STATUS "use websocketpp")
  add_compile_definitions(CCAPI_LEGACY_USE_WEBSOCKETPP)
  set(RAPIDJSON_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/rapidjson/include)
  set(WEBSOCKETPP_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/websocketpp)
  set(BOOST_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/boost)
  set(HFFIX_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/hffix/include)
  include_directories(${CCAPI_PROJECT_DIR}/include ${WEBSOCKETPP_INCLUDE_DIR} ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${HFFIX_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
endif()
find_package(OpenSSL REQUIRED)
link_libraries(OpenSSL::Crypto OpenSSL::SSL)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(SOURCE_LOGGER ${CCAPI_PROJECT_DIR}/binding/ccapi_logger.cpp)
find_package(SWIG REQUIRED)
include(UseSWIG)
if(BUILD_TEST)
  include(CTest)
endif()
add_compile_options(-Wno-deprecated -Wno-nonnull -Wno-deprecated-declarations)

add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)

add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)

add_compile_definitions(CCAPI_ENABLE_SERVICE_FIX)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GEMINI)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KRAKEN)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITSTAMP)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITFINEX)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITMEX)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_US)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_MARGIN)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_HUOBI)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKX)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_ERISX)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KUCOIN)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_DERIBIT)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GATEIO)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_CRYPTOCOM)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BYBIT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BYBIT_DERIVATIVES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_ASCENDEX)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITGET)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITMART)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_MEXC)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES)

add_compile_definitions(CCAPI_ENABLE_EXCHANGE_WHITEBIT)

find_package(ZLIB REQUIRED)
link_libraries(ZLIB::ZLIB)

if(BUILD_PYTHON)
configure_file(
  swig_interface.i.in
  ${CMAKE_BINARY_DIR}/python/swig_interface.i
@ONLY)
set(SWIG_INTERFACE ${CMAKE_BINARY_DIR}/python/swig_interface.i)
  add_subdirectory(python)
endif()

if(BUILD_JAVA)
configure_file(
  swig_interface.i.in
  ${CMAKE_BINARY_DIR}/java/swig_interface.i
@ONLY)
set(SWIG_INTERFACE ${CMAKE_BINARY_DIR}/java/swig_interface.i)
  add_subdirectory(java)
endif()

if(BUILD_CSHARP)
file(READ csharp/swig_interface_ccapi_language_specific.i SWIG_INTERFACE_CCAPI_LANGUAGE_SPECIFIC)
configure_file(
  swig_interface.i.in
  ${CMAKE_BINARY_DIR}/csharp/swig_interface.i
@ONLY)
set(SWIG_INTERFACE ${CMAKE_BINARY_DIR}/csharp/swig_interface.i)
  add_subdirectory(csharp)
endif()
