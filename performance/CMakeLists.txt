cmake_minimum_required(VERSION 3.14)
set(NAME performance)
project(${NAME})
set(CMAKE_CXX_STANDARD 14)
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
set(RAPIDJSON_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/rapidjson/include)
set(WEBSOCKETPP_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/websocketpp)
set(BOOST_INCLUDE_DIR ${CCAPI_PROJECT_DIR}/dependency/boost)
find_package(OpenSSL REQUIRED)
include_directories(${CCAPI_PROJECT_DIR}/include ${WEBSOCKETPP_INCLUDE_DIR} ${BOOST_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
set(ADDITIONAL_LINK_LIBRARIES "")
if (WIN32)
  set(ADDITIONAL_LINK_LIBRARIES ws2_32)
endif()
link_libraries(OpenSSL::Crypto OpenSSL::SSL ${ADDITIONAL_LINK_LIBRARIES})
add_subdirectory(src/rest_vs_fix)