set(NAME binding_python)
project(${NAME})
set(SWIG_TARGET_NAME ccapi_${NAME})

# Find Python
find_package(Python REQUIRED COMPONENTS Interpreter Development)
message(STATUS "Python_VERSION: ${Python_VERSION}")
message(STATUS "Python_EXECUTABLE: ${Python_EXECUTABLE}")
 if(Python_VERSION VERSION_LESS 3)
   message(FATAL_ERROR "Require Python 3")
 endif()
list(APPEND CMAKE_SWIG_FLAGS "-py3")
set(SWIG_INTERFACE ${CCAPI_PROJECT_DIR}/binding/swig_interface.i)
set_property(SOURCE ${SWIG_INTERFACE} PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${SWIG_INTERFACE} PROPERTY COMPILE_OPTIONS "-builtin;-threads;-doxygen")
swig_add_library(${SWIG_TARGET_NAME}
  LANGUAGE python
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/python/${SWIG_TARGET_NAME}
SOURCES ${SWIG_INTERFACE} ${SOURCE_LOGGER})
if(NOT CCAPI_LEGACY_USE_WEBSOCKETPP)
  add_dependencies(${SWIG_TARGET_NAME} boost rapidjson hffix)
endif()
set_property(TARGET ${SWIG_TARGET_NAME} PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_include_directories(${SWIG_TARGET_NAME}
  PRIVATE
  ${Python_INCLUDE_DIRS}
  )
target_link_libraries(${SWIG_TARGET_NAME}	PRIVATE	${Python_LIBRARIES})

# #######################
# ## Python Packaging  ##
# #######################
configure_file(
  setup.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/setup.py.in
  @ONLY)
file(GENERATE
  OUTPUT setup.py
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py.in)

# Find if python module MODULE_NAME is available,
# if not install it to the Python user install directory.
# function(search_python_module MODULE_NAME)
#   execute_process(
#     COMMAND ${Python_EXECUTABLE} -c "import ${MODULE_NAME}; print(${MODULE_NAME}.__version__)"
#     RESULT_VARIABLE _RESULT
#     OUTPUT_VARIABLE MODULE_VERSION
#     ERROR_QUIET
#     OUTPUT_STRIP_TRAILING_WHITESPACE
#     )
#   if(${_RESULT} STREQUAL "0")
#     message(STATUS "Found python module: ${MODULE_NAME} (found version \"${MODULE_VERSION}\")")
#   else()
#     message(WARNING "Can't find python module \"${MODULE_NAME}\", user install it using pip...")
#     execute_process(
#       COMMAND ${Python_EXECUTABLE} -m pip install --upgrade ${MODULE_NAME}
#       OUTPUT_STRIP_TRAILING_WHITESPACE
#       )
#   endif()
# endfunction()

# Look for required python modules
# search_python_module(setuptools)
# search_python_module(wheel)
set(PACKAGING_DIR packaging)
set(PACKAGING_DIR_FULL ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGING_DIR}/${BUILD_VERSION})
file(MAKE_DIRECTORY ${PACKAGING_DIR_FULL})
set(PYTHON_PACKAGING_TARGET_NAME python_${PACKAGING_DIR})
add_custom_target(${PYTHON_PACKAGING_TARGET_NAME} ALL
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/setup.py ${PACKAGING_DIR_FULL}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}/ccapi.py ${PACKAGING_DIR_FULL}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SWIG_TARGET_NAME}> ${PACKAGING_DIR_FULL}
  COMMAND ${Python_EXECUTABLE} setup.py bdist_wheel
  COMMAND ${Python_EXECUTABLE} -m pip install --upgrade .
  WORKING_DIRECTORY ${PACKAGING_DIR_FULL}
)
add_dependencies(${PYTHON_PACKAGING_TARGET_NAME} ${SWIG_TARGET_NAME})

# Test
if(BUILD_TEST)
  set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test)
  configure_file(test/test.py ${TEST_DIR}/test.py COPYONLY)
  add_test(NAME test
    COMMAND ${Python_EXECUTABLE} test.py
  WORKING_DIRECTORY ${TEST_DIR})
endif()
