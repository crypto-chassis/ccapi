cmake_minimum_required(VERSION 3.14)
set(NAME example)
project(${NAME})
set(CMAKE_CXX_STANDARD 17)
if(NOT APPLE AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
if(WIN32)
  set(CMAKE_BUILD_TYPE "Release")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
get_filename_component(CCAPI_PROJECT_DIR ../ ABSOLUTE)
message(STATUS "CCAPI_PROJECT_DIR: ${CCAPI_PROJECT_DIR}")

if(NOT BOOST_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(
    boost
    URL https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/boost"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(BOOST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/boost")
else()
  add_library(boost INTERFACE)
endif()
if(NOT RAPIDJSON_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(
    rapidjson
    URL https://github.com/Tencent/rapidjson/archive/3b2441b87f99ab65f37b141a7b548ebadb607b96.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/rapidjson"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(RAPIDJSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/rapidjson/include")
else()
  add_library(rapidjson INTERFACE)
endif()
if(NOT HFFIX_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(
    hffix
    URL https://github.com/jamesdbrock/hffix/archive/refs/tags/v1.4.1.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/hffix"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(HFFIX_INCLUDE_DIR "${CMAKE_BINARY_DIR}/hffix/include")
else()
  add_library(hffix INTERFACE)
endif()

include_directories(${CCAPI_PROJECT_DIR}/include)
include_directories(SYSTEM ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR}
                    ${HFFIX_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)
set(ADDITIONAL_LINK_LIBRARIES "")
if(WIN32)
  set(ADDITIONAL_LINK_LIBRARIES ws2_32)
endif()
link_libraries(OpenSSL::Crypto OpenSSL::SSL ${ADDITIONAL_LINK_LIBRARIES})
add_compile_options(-Wno-deprecated -Wno-nonnull -Wno-deprecated-declarations)
add_subdirectory(market_data_simple_request)
add_subdirectory(market_data_simple_subscription)
add_subdirectory(market_data_advanced_request)
add_subdirectory(market_data_advanced_subscription)
add_subdirectory(generic_public_request)
add_subdirectory(generic_private_request)
add_subdirectory(execution_management_simple_request)
add_subdirectory(execution_management_simple_subscription)
add_subdirectory(execution_management_advanced_request)
add_subdirectory(execution_management_advanced_subscription)
add_subdirectory(fix_simple)
add_subdirectory(fix_advanced)
add_subdirectory(enable_library_logging)
add_subdirectory(set_timer)
add_subdirectory(heartbeat)
add_subdirectory(override_exchange_url_at_runtime)
add_subdirectory(test_order_latency)
add_subdirectory(test_cpu_usage)
add_subdirectory(use_multiple_sessions)
add_subdirectory(reduce_build_time)
