if(NOT BUILD_PYTHON)
  return()
endif()
set(NAME binding_python)
project(${NAME})
set(SWIG_TARGET_NAME ccapi_${NAME})
# Use latest UseSWIG module
# cmake_minimum_required(VERSION 3.14)

# if(NOT TARGET CMakeSwig::FooBar)
#   message(FATAL_ERROR "Python: missing FooBar TARGET")
# endif()

# Will need swig


# if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
#   list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
# endif()

# if(UNIX AND NOT APPLE)
#   list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
# endif()

# Find Python
find_package(Python REQUIRED COMPONENTS Interpreter Development)
message(STATUS "Python_VERSION: ${Python_VERSION}")
 if(Python_VERSION VERSION_LESS 3)
   message(FATAL_ERROR "Require Python 3")
 endif()
# if(Python_VERSION VERSION_GREATER_EQUAL 3)
list(APPEND CMAKE_SWIG_FLAGS "-py3")
  # list(APPEND CMAKE_SWIG_FLAGS "-py3;-DPY3")
# endif()
# list(APPEND CMAKE_SWIG_FLAGS "-threads")
# message(STATUS "CMAKE_SWIG_FLAGS: ${CMAKE_SWIG_FLAGS}")
# Swig wrap all libraries
# foreach(SUBPROJECT IN ITEMS Foo Bar FooBar)
  # add_subdirectory(${SUBPROJECT}/python)
# endforeach()





# set(TARGET_NAME ccapi_${NAME})
message(STATUS "SOURCE_LOGGER: ${SOURCE_LOGGER}")
set(SWIG_INTERFACE ${CCAPI_CPP_PROJECT_DIR}/binding/swig_interface.i)

set_property(SOURCE ${SWIG_INTERFACE} PROPERTY CPLUSPLUS ON)
set_property(SOURCE ${SWIG_INTERFACE} PROPERTY COMPILE_OPTIONS "-builtin;-threads")
# set_property(SOURCE foobar.i PROPERTY SWIG_MODULE_NAME ${NAME})
# set_property(SOURCE swig_interface.i ${SOURCE_LOGGER} PROPERTY COMPILE_DEFINITIONS CCAPI_FINAL="")
# set_property(SOURCE swig_interface.i PROPERTY GENERATED_COMPILE_DEFINITIONS SWIG)
# add_compile_definitions(CCAPI_FINAL="")
swig_add_library(${SWIG_TARGET_NAME}
  LANGUAGE python
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/python/${SWIG_TARGET_NAME}
SOURCES ${SWIG_INTERFACE} ${SOURCE_LOGGER})
# set_property(TARGET ${NAME} PROPERTY COMPILE_DEFINITIONS CCAPI_FINAL="")
# set_property(TARGET ${NAME} PROPERTY SWIG_COMPILE_DEFINITIONS CCAPI_FINAL="")
# set_property(TARGET ${NAME} PROPERTY SWIG_GENERATED_COMPILE_DEFINITIONS SWIG)
set_property(TARGET ${SWIG_TARGET_NAME} PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
# target_compile_definitions(${NAME} PUBLIC CCAPI_FINAL="")
target_include_directories(${SWIG_TARGET_NAME}
  PRIVATE
  ${Python_INCLUDE_DIRS}
  )
target_link_libraries(${SWIG_TARGET_NAME}	PRIVATE	${Python_LIBRARIES})
# note: macOS is APPLE and also UNIX !
# if(APPLE)
#   set_target_properties(${SWIG_TARGET_NAME} PROPERTIES
#     SUFFIX ".so"
#     INSTALL_RPATH "@loader_path;@loader_path/../../${SWIG_TARGET_NAME}/.libs"
#     )
#   # set_property(TARGET ${NAME} APPEND PROPERTY
#   #   LINK_FLAGS "-flat_namespace -undefined suppress"
#   #   )
# elseif(UNIX)
#   set_target_properties(${SWIG_TARGET_NAME} PROPERTIES
#     INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${SWIG_TARGET_NAME}/.libs"
#     )
# endif()
# target_link_libraries(${NAME} PRIVATE FooBar)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
# if(MSVC)
#   target_link_libraries(${NAME}	PRIVATE	${Python_LIBRARIES})
# endif()




# #######################
# ## Python Packaging  ##
# #######################
# # setup.py.in contains cmake variable e.g. @PROJECT_NAME@ and
# # generator expression e.g. $<TARGET_FILE_NAME:>
configure_file(
  setup.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/setup.py.in
  @ONLY)
file(GENERATE
  OUTPUT $<CONFIG>/setup.py
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py.in)

# Find if python module MODULE_NAME is available,
# if not install it to the Python user install directory.
function(search_python_module MODULE_NAME)
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import ${MODULE_NAME}; print(${MODULE_NAME}.__version__)"
    RESULT_VARIABLE _RESULT
    OUTPUT_VARIABLE MODULE_VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(${_RESULT} STREQUAL "0")
    message(STATUS "Found python module: ${MODULE_NAME} (found version \"${MODULE_VERSION}\")")
  else()
    message(WARNING "Can't find python module \"${MODULE_NAME}\", user install it using pip...")
    execute_process(
      COMMAND ${Python_EXECUTABLE} -m pip install --upgrade --user ${MODULE_NAME}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  endif()
endfunction()

# Look for required python modules
search_python_module(setuptools)
search_python_module(wheel)
set(PACKAGING_DIR packaging)
set(PACKAGING_DIR_FULL ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGING_DIR})

# message(STATUS "PACKAGING_DIR_FULL: ${PACKAGING_DIR_FULL}")
# message(STATUS "${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}/ccapi.py")
file(MAKE_DIRECTORY ${PACKAGING_DIR_FULL})
set(PYTHON_PACKAGING_TARGET_NAME python_${PACKAGING_DIR})



add_custom_target(${PYTHON_PACKAGING_TARGET_NAME} ALL
  # COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGING_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/setup.py ${PACKAGING_DIR_FULL}
  # COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/python/__init__.py.in ${PROJECT_NAME}/__init__.py
  # COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/python/__init__.py.in ${PROJECT_NAME}/Foo/__init__.py
  # COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/python/__init__.py.in ${PROJECT_NAME}/Bar/__init__.py
  # COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/python/__init__.py.in ${PROJECT_NAME}/python/__init__.py

  # COMMAND ${CMAKE_COMMAND} -E remove_directory dist
  # COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_NAME}/.libs
  # COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:> ${PROJECT_NAME}/Foo
  # COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pyBar> ${PROJECT_NAME}/Bar
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${SWIG_TARGET_NAME}/ccapi.py ${PACKAGING_DIR_FULL}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${SWIG_TARGET_NAME}> ${PACKAGING_DIR_FULL}
  # Don't need to copy static lib on Windows
  # COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:${UNIX}>,copy,true>
  # $<TARGET_FILE:binding_python> ${PROJECT_NAME}/.libs
  # $<TARGET_FILE:Foo> $<TARGET_FILE:Bar> $<TARGET_FILE:FooBar> ${PROJECT_NAME}/.libs
  COMMAND ${Python_EXECUTABLE} setup.py bdist_wheel
  # COMMAND ${Python_EXECUTABLE} -m pip wheel .
  # COMMAND ${Python_EXECUTABLE} -m pip install --upgrade --user .
  # BYPRODUCTS
  # python/${PROJECT_NAME}
  # python/build
  # python/dist
  # python/${PROJECT_NAME}.egg-info
  WORKING_DIRECTORY ${PACKAGING_DIR_FULL}
  )
add_dependencies(${PYTHON_PACKAGING_TARGET_NAME} ${SWIG_TARGET_NAME})
install(CODE "
  execute_process(COMMAND ${Python_EXECUTABLE} -m pip install --upgrade --user .
  WORKING_DIRECTORY ${PACKAGING_DIR_FULL})
")
# # Test
# if(BUILD_TESTING)
#   # Look for python module virtualenv
#   search_python_module(virtualenv)
#   # Testing using a vitual environment
#   set(VENV_EXECUTABLE ${Python_EXECUTABLE} -m virtualenv)
#   set(VENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/venv)
#   if(WIN32)
#     set(VENV_Python_EXECUTABLE "${VENV_DIR}\\Scripts\\python.exe")
#   else()
#     set(VENV_Python_EXECUTABLE ${VENV_DIR}/bin/python)
#   endif()
#   # make a virtualenv to install our python package in it
#   add_custom_command(TARGET python_package POST_BUILD
#     COMMAND ${VENV_EXECUTABLE} -p ${Python_EXECUTABLE} ${VENV_DIR}
#     # Must not call it in a folder containing the setup.py otherwise pip call it
#     # (i.e. "python setup.py bdist") while we want to consume the wheel package
#     COMMAND ${VENV_Python_EXECUTABLE} -m pip install --find-links=${CMAKE_CURRENT_BINARY_DIR}/python/dist ${PROJECT_NAME}
#     BYPRODUCTS ${VENV_DIR}
#     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#   # run the tests within the virtualenv
#   add_test(NAME pytest_venv
#     COMMAND ${VENV_Python_EXECUTABLE} ${PROJECT_SOURCE_DIR}/python/test.py)
# endif()
