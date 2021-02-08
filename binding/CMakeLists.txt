cmake_minimum_required(VERSION 3.17)
set(NAME binding)
set(BUILD_VERSION "1.0.0" CACHE STRING "User-specified build version")
message(STATUS "BUILD_VERSION: ${BUILD_VERSION}")
project(${NAME} LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

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

# project(CMakeSwig VERSION 1.0 LANGUAGES CXX)
# message(STATUS "project: ${PROJECT_NAME}")


# Force default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug, Release (default), RelWithDebInfo and MinSizeRel."
    FORCE)
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
# message(STATUS "CMAKE_CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
# Layout build dir like install dir
include(GNUInstallDirs)
if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries(.so or .dyld)." ON)
  # set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config build system (e.g. Xcode, ninja Multi-Config)
  foreach(OUTPUTCONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OUTPUTCONFIG_UPPER} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_LIBDIR})
    # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
  endforeach()
else()
  # Currently Only support static build for windows
  set(BUILD_SHARED_LIBS OFF)
  message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config builds (e.g. msvc)
  foreach(OUTPUTCONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OUTPUTCONFIG_UPPER} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
    # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

# By default only the ortools C++ library is built.
# option(BUILD_CXX "Build C++ library" ON)
option(BUILD_PYTHON "Build Python Library" OFF)
option(INSTALL_PYTHON "Install Python Library" OFF)
# option(BUILD_JAVA "Build Java Library" OFF)
# option(BUILD_DOTNET "Build .NET Library" OFF)
# message(STATUS "Build C++ library: ${BUILD_CXX}")
message(STATUS "Build Python: ${BUILD_PYTHON}")
# message(STATUS "Build Java: ${BUILD_JAVA}")
# message(STATUS "Build .Net: ${BUILD_DOTNET}")

# include(CTest)
# if(BUILD_TESTING)
#   include(FetchContent)
#   #FetchContent_Declare(
#   #  googletest
#   #  GIT_REPOSITORY https://github.com/google/googletest.git
#   #  GIT_TAG master)
#   #set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#   #FetchContent_MakeAvailable(googletest)
#   FetchContent_Declare(
#     Catch2
#     GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#     GIT_TAG devel
#     GIT_SHALLOW TRUE
#     GIT_PROGRESS TRUE
#     )
#   FetchContent_MakeAvailable(Catch2)
# endif()
#
# include(cpp)

# if(WIN32 AND (BUILD_PYTHON OR BUILD_JAVA OR BUILD_DOTNET))
#   message(STATUS "Getting SWIG: ...")
#   include(swig)
#   message(STATUS "Getting SWIG: ...DONE")
# endif()
# include(cpp)

# set(SERVICE_LIST "MARKET_DATA" "EXECUTION_MANAGEMENT")
# foreach(SERVICE IN LISTS SERVICE_LIST)
#     if("${${SERVICE}}")
#       add_compile_definitions("CCAPI_ENABLE_SERVICE_${SERVICE}")
#     endif()
# endforeach()
#
# set(EXCHANGE_LIST "COINBASE" "GEMINI" "KRAKEN" "BITSTAMP" "BITFINEX" "BITMEX" "BINANCE_US" "BINANCE" "BINANCE_FUTURES" "HUOBI" "OKEX")
# foreach(EXCHANGE IN LISTS EXCHANGE_LIST)
#   if("${${EXCHANGE}}")
#     add_compile_definitions("CCAPI_ENABLE_EXCHANGE_${EXCHANGE}")
#     if("${EXCHANGE}" STREQUAL "HUOBI" OR "${EXCHANGE}" STREQUAL "OKEX")
#       message(STATUS "zlib")
#       find_package(ZLIB REQUIRED)
#     endif()
#   endif()
# endforeach()
#
# set(LOG_LIST "TRACE" "DEBUG" "INFO" "WARN" "ERROR" "FATAL")
# foreach(LOG_ IN LISTS LOG_LIST)
#     if("${${LOG_}}")
#       add_compile_definitions("CCAPI_ENABLE_LOG_${LOG_}")
#     endif()
# endforeach()
# option(CCAPI_ENABLE_SERVICE_MARKET_DATA "Enable service market data" OFF)
# if(CCAPI_ENABLE_SERVICE_MARKET_DATA)
#   add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)
# endif()
# option(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT "Enable service execution management" OFF)
# if(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
#   add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_COINBASE "Enable exchange coinbase" OFF)
# if(CCAPI_ENABLE_EXCHANGE_COINBASE)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_GEMINI "Enable exchange gemini" OFF)
# if(CCAPI_ENABLE_EXCHANGE_GEMINI)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_GEMINI)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_KRAKEN "Enable exchange kraken" OFF)
# if(CCAPI_ENABLE_EXCHANGE_KRAKEN)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_KRAKEN)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_BITSTAMP "Enable exchange bitstamp" OFF)
# if(CCAPI_ENABLE_EXCHANGE_BITSTAMP)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITSTAMP)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_BITFINEX "Enable exchange bitfinex" OFF)
# if(CCAPI_ENABLE_EXCHANGE_BITFINEX)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITFINEX)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_BITMEX "Enable exchange bitmex" OFF)
# if(CCAPI_ENABLE_EXCHANGE_BITMEX)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BITMEX)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_BINANCE_US "Enable exchange binance-us" OFF)
# if(CCAPI_ENABLE_EXCHANGE_BINANCE_US)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_US)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_BINANCE "Enable exchange binance" OFF)
# if(CCAPI_ENABLE_EXCHANGE_BINANCE)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE)
# endif()
# option(CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES "Enable Exchange binance-futures" OFF)
# if(CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES)
#   add_compile_definitions(CCAPI_ENABLE_EXCHANGE_BINANCE_FUTURES)
# endif()
#
#
# add_compile_definitions(CCAPI_ENABLE_LOG_TRACE)
# add_compile_definitions(CCAPI_ENABLE_EXCHANGE_COINBASE)
# add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)
#
# if("${EXCHANGE}" STREQUAL "HUOBI" OR "${EXCHANGE}" STREQUAL "OKEX")
#   message(STATUS "zlib")
#   find_package(ZLIB REQUIRED)
# endif()
include(user_specified_cmake_include)


set(CMAKE_CXX_STANDARD 14)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
get_filename_component(CCAPI_CPP_PROJECT_DIR ../ ABSOLUTE)
message(STATUS "CCAPI_CPP_PROJECT_DIR: ${CCAPI_CPP_PROJECT_DIR}")
set(RAPIDJSON_INCLUDE_DIR ${CCAPI_CPP_PROJECT_DIR}/dependency/rapidjson/include)
set(WEBSOCKETPP_INCLUDE_DIR ${CCAPI_CPP_PROJECT_DIR}/dependency/websocketpp)
set(BOOST_INCLUDE_DIR ${CCAPI_CPP_PROJECT_DIR}/dependency/boost)
set(DATE_INCLUDE_DIR ${CCAPI_CPP_PROJECT_DIR}/dependency/date/include)
find_package(OpenSSL REQUIRED)
set(OPENSSL_USE_STATIC_LIBS TRUE)
include_directories(${CCAPI_CPP_PROJECT_DIR}/include ${WEBSOCKETPP_INCLUDE_DIR} ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${DATE_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
link_libraries(OpenSSL::Crypto OpenSSL::SSL)

# add_compile_definitions(CCAPI_FINAL)
# add_compile_definitions(CCAPI_SWIG)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(SOURCE_LOGGER ${CCAPI_CPP_PROJECT_DIR}/binding/ccapi_logger.cpp)
set(CMAKE_SWIG_FLAGS)
find_package(SWIG REQUIRED)
include(UseSWIG)
add_subdirectory(python)
# include(java)
# include(dotnet)
