set(NAME binding_csharp)
project(${NAME})
set(SWIG_TARGET_NAME ccapi_${NAME})

if(NOT DOTNET_BUILD_CONFIGURATION)
  set(DOTNET_BUILD_CONFIGURATION "Release" CACHE STRING
    "Choose the type of build, options are: Debug, Release."
    FORCE)
endif()
message(STATUS "DOTNET_BUILD_CONFIGURATION: ${DOTNET_BUILD_CONFIGURATION}")

# Find dotnet cli
find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
if(NOT DOTNET_EXECUTABLE)
  message(FATAL_ERROR "Check for dotnet Program: not found")
else()
  message(STATUS "Found dotnet Program: ${DOTNET_EXECUTABLE}")
endif()

execute_process(
  COMMAND ${DOTNET_EXECUTABLE} --version
  OUTPUT_VARIABLE DOTNET_EXECUTABLE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Dotnet version: ${DOTNET_EXECUTABLE_VERSION}")

set(CSHARP_NAMESPACE "ccapi")

set_property(SOURCE ${SWIG_INTERFACE} PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${SWIG_INTERFACE} PROPERTY COMPILE_OPTIONS "-namespace;${CSHARP_NAMESPACE};-dllimport;${SWIG_TARGET_NAME}.so")

swig_add_library(${SWIG_TARGET_NAME}
  LANGUAGE csharp
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/csharp/${SWIG_TARGET_NAME}
SOURCES ${SWIG_INTERFACE} ${SOURCE_LOGGER})

if(NOT CCAPI_LEGACY_USE_WEBSOCKETPP)
  add_dependencies(${SWIG_TARGET_NAME} boost rapidjson hffix)
endif()

set(PACKAGING_DIR packaging)
set(PACKAGING_DIR_FULL ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGING_DIR}/${BUILD_VERSION})
file(MAKE_DIRECTORY ${PACKAGING_DIR_FULL})
set(SRC_DIR_FULL ${CMAKE_CURRENT_BINARY_DIR}/src)
file(MAKE_DIRECTORY ${SRC_DIR_FULL})
configure_file(
  ccapi.csproj.in
  ${SRC_DIR_FULL}/ccapi.csproj
  @ONLY)
set(CSHARP_PACKAGING_TARGET_NAME csharp_${PACKAGING_DIR})
add_custom_target(${CSHARP_PACKAGING_TARGET_NAME} ALL
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}/*.cs ${SRC_DIR_FULL}
  COMMAND ${DOTNET_EXECUTABLE} build -c ${DOTNET_BUILD_CONFIGURATION} -o ${PACKAGING_DIR_FULL} ccapi.csproj
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SWIG_TARGET_NAME}> ${PACKAGING_DIR_FULL}
  WORKING_DIRECTORY ${SRC_DIR_FULL}
)
add_dependencies(${CSHARP_PACKAGING_TARGET_NAME} ${SWIG_TARGET_NAME})

# Test
if(BUILD_TEST)
  set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)
  file(COPY example DESTINATION ${TEST_DIR})
  set(SUBDIRECTORY_LIST "enable_library_logging" "execution_management_simple_request" "execution_management_simple_subscription" "fix_simple" "handle_exception" "market_data_multiple_subscription" "market_data_simple_request" "market_data_simple_subscription")
  foreach(SUBDIRECTORY IN LISTS SUBDIRECTORY_LIST)
    set(CSHARP_TEST_TARGET_NAME csharp_test_${SUBDIRECTORY})
    add_custom_target(${CSHARP_TEST_TARGET_NAME} ALL
      COMMAND ${DOTNET_EXECUTABLE} clean
      COMMAND ${DOTNET_EXECUTABLE} build --property:CcapiLibraryPath=${PACKAGING_DIR_FULL}/ccapi.dll -c ${DOTNET_BUILD_CONFIGURATION}
      WORKING_DIRECTORY ${TEST_DIR}/example/${SUBDIRECTORY}
    )
    add_dependencies(${CSHARP_TEST_TARGET_NAME} ${CSHARP_PACKAGING_TARGET_NAME})
  endforeach()
  file(COPY test DESTINATION ${TEST_DIR})
  set(CSHARP_TEST_TARGET_NAME csharp_test_test)
    add_custom_target(${CSHARP_TEST_TARGET_NAME} ALL
      COMMAND ${DOTNET_EXECUTABLE} clean
      COMMAND ${DOTNET_EXECUTABLE} build --property:CcapiLibraryPath=${PACKAGING_DIR_FULL}/ccapi.dll -c ${DOTNET_BUILD_CONFIGURATION}
      WORKING_DIRECTORY ${TEST_DIR}/test
    )
    add_dependencies(${CSHARP_TEST_TARGET_NAME} ${CSHARP_PACKAGING_TARGET_NAME})
    add_test(NAME csharp_test
    COMMAND env LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${PACKAGING_DIR_FULL} ${DOTNET_EXECUTABLE} run -c ${DOTNET_BUILD_CONFIGURATION} --no-build
  WORKING_DIRECTORY ${TEST_DIR}/test)
endif()
