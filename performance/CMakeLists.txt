cmake_minimum_required(VERSION 3.14)
set(NAME performance)
project(${NAME})
set(CMAKE_CXX_STANDARD 17)
if(NOT APPLE AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
if (WIN32)
  set(CMAKE_BUILD_TYPE "Release")
endif()
# Force default build type to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug, Release (default), RelWithDebInfo and MinSizeRel."
    FORCE)
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
set(OPENSSL_USE_STATIC_LIBS TRUE)
if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  message(STATUS "Enable performance tuning")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
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
set(ADDITIONAL_LINK_LIBRARIES "")
if (WIN32)
  set(ADDITIONAL_LINK_LIBRARIES ws2_32)
endif()
link_libraries(OpenSSL::Crypto OpenSSL::SSL ${ADDITIONAL_LINK_LIBRARIES})
add_compile_options(-Wno-deprecated -Wno-nonnull -Wno-deprecated-declarations)
add_subdirectory(src/rest_vs_fix)
