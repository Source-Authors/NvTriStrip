# NvTriStrip - NVidia tristrip library.

cmake_minimum_required(VERSION 3.10)
project(nvTriStrip)

# Use static MSVC runtime.
option(NVTS_ENABLE_STATIC_MSVC_RUNTIME "Build with static MSVC runtime." ON)

# Use Address Sanitizer.
option(NVTS_ENABLE_ASAN "Build with Address Sanitizer." OFF)

# Compiler id for Apple Clang is now AppleClang.
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

# Setup root directory of the nvTriStrip project.
set(NVTS_ROOT_DIR "${CMAKE_SOURCE_DIR}" CACHE STRING
  "Directory root of the nvTriStrip project.")

## Package information.
set(PACKAGE_NAME   "nvTriStrip")

# C++ standard can be overridden when this is used as a sub-project.
if(NOT CMAKE_CXX_STANDARD)
  # This project requires C++17.
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif(NOT CMAKE_CXX_STANDARD)

# Git is required to build software version via Git.
find_package(Git)

if (NOT DEFINED PACKAGE_VERSION)
  if (NOT Git_FOUND)
    message(FATAL "[common]: Unable to extract Git software version - no Git.")
  endif()

  message(STATUS "[common]: Extracting Git software version.")

  if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    execute_process(COMMAND cmd /c "git rev-list --branches HEAD | find /v " " /c" OUTPUT_VARIABLE NVTS_GIT_VERSION WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    execute_process(COMMAND bash "-c" "git rev-list --branches HEAD | wc -l | tr -d ' ' | tr -d '\n'" OUTPUT_VARIABLE NVTS_GIT_VERSION WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(PACKAGE_VER_PATCH 0)
  math(EXPR PACKAGE_VER_TWEAK ${NVTS_GIT_VERSION})
  message(STATUS
    "[common]: Build version from Git (patch.tweak): ${PACKAGE_VER_PATCH}.${PACKAGE_VER_TWEAK}.")

  set(PACKAGE_VER_MAJOR 1)
  set(PACKAGE_VER_MINOR 1)

  set(PACKAGE_VERSION
    "${PACKAGE_VER_MAJOR}.${PACKAGE_VER_MINOR}.${PACKAGE_VER_PATCH}.${PACKAGE_VER_TWEAK}")
endif()
set(PACKAGE_STRING     "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME    "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT  "https://github.com/Source-Authors/nvTriStrip/issues")

project(${PACKAGE_NAME}
  VERSION ${PACKAGE_VER_MAJOR}.${PACKAGE_VER_MINOR}.${PACKAGE_VER_PATCH}.${PACKAGE_VER_TWEAK}
  LANGUAGES CXX)

message(STATUS "[common]: CMake version: ${CMAKE_VERSION}.")
message(STATUS "[common]: ${PACKAGE_NAME} version: ${PACKAGE_VERSION}.")

## Common variables.

set(NVTS_BINARY_DIR "${CMAKE_BINARY_DIR}" CACHE STRING
  "Directory of the nvTriStrip binaries output.")
set(NVTS_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}" CACHE STRING
  "Name of the nvTriStrip build system.")

set(NVTS_ENABLE_STD_ONLY OFF)

if (${NVTS_SYSTEM_NAME} STREQUAL "Darwin")
  set(NVTS_OS_MACOS ON CACHE BOOL "MacOS is build system.")
  set(NVTS_ENABLE_STD_ONLY ON)
else()
  set(NVTS_OS_MACOS OFF CACHE BOOL "MacOS is NOT build system.")
endif()

if (${NVTS_SYSTEM_NAME} STREQUAL "Linux")
  set(NVTS_OS_LINUX ON CACHE BOOL "Linux distro is build system.")
  set(NVTS_ENABLE_STD_ONLY ON)
else()
  set(NVTS_OS_LINUX OFF CACHE BOOL "Linux distro is NOT build system.")
endif()

if (${NVTS_SYSTEM_NAME} STREQUAL "Windows")
  set(NVTS_OS_WIN ON CACHE BOOL "Windows is build system.")
else()
  set(NVTS_OS_WIN OFF CACHE BOOL "Windows is NOT build system.")
endif()

if (NVTS_OS_MACOS OR NVTS_OS_LINUX)
  set(NVTS_OS_POSIX ON CACHE BOOL "POSIX compatible os is build system.")
else()
  set(NVTS_OS_POSIX OFF CACHE BOOL "POSIX compatible os is NOT build system.")
endif()

set(NVTS_PRODUCT_NAME "nvTriStrip" CACHE STRING
  "Name of the nvTriStrip product.")

set(NVTS_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
  "CMake build type used to build ${PACKAGE_NAME}.")
set(NVTS_CMAKE_GENERATOR "${CMAKE_GENERATOR}" CACHE STRING
  "CMake generator used to generate build tree." FORCE)
set(NVTS_CXX_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING
  "Compiler used to build ${PACKAGE_NAME}.")
set(NVTS_CXX_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}" CACHE STRING
  "Compiler id used to build ${PACKAGE_NAME}.")
set(NVTS_CXX_COMPILER_FRONTEND_VARIANT "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}"
  CACHE STRING "Compiler frontend used to build ${PACKAGE_NAME}.")
set(NVTS_EXPORT_COMPILE_COMMANDS ${CMAKE_EXPORT_COMPILE_COMMANDS} CACHE BOOL
  "Enable/Disable output of compile commands during generation.")

if (NVTS_CXX_COMPILER_ID MATCHES "Clang")
  set(NVTS_COMPILER_CLANG ON CACHE BOOL "Compiler is Clang.")
else()
  set(NVTS_COMPILER_CLANG OFF CACHE BOOL "Compiler is NOT Clang.")
endif()

if (NVTS_CXX_COMPILER_ID STREQUAL "GNU")
  set(NVTS_COMPILER_GCC ON CACHE BOOL "Compiler is GCC.")
else()
  set(NVTS_COMPILER_GCC OFF CACHE BOOL "Compiler is NOT GCC.")
endif()

if (NVTS_CXX_COMPILER_ID STREQUAL "MSVC")
  set(NVTS_COMPILER_MSVC ON CACHE BOOL "Compiler is MSVC.")
else()
  set(NVTS_COMPILER_MSVC OFF CACHE BOOL "Compiler is NOT MSVC.")
endif()

## Common dependencies.

if (Git_FOUND)
  message(STATUS
    "[common]: Git version ${GIT_VERSION_STRING} found at '${GIT_EXECUTABLE}'.")

  # Get the current working branch.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${NVTS_ROOT_DIR}
    OUTPUT_VARIABLE NVTS_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Get the latest commit hash.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY ${NVTS_ROOT_DIR}
    OUTPUT_VARIABLE NVTS_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Get the subject of the commit.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" log -1 --format=%s
    WORKING_DIRECTORY "${NVTS_ROOT_DIR}"
    OUTPUT_VARIABLE NVTS_GIT_COMMIT_SUBJECT
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

  message(STATUS
    "[common]: Git using branch '${NVTS_GIT_BRANCH}', "
    "commit ${NVTS_GIT_COMMIT_HASH} | ${NVTS_GIT_COMMIT_SUBJECT}.")
endif()

message(STATUS
  "[common]: ${PACKAGE_NAME} using CMake generator: ${NVTS_CMAKE_GENERATOR}.")

# Select correct threading library.
if (NOT NVTS_OS_WIN)
  set(CMAKE_THREAD_PREFER_PTHREAD ON)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_package(Threads REQUIRED)
list(APPEND CMAKE_REQUIRED_LIBRARIES Threads::Threads)
if (NOT CMAKE_THREAD_LIBS_INIT)
  set(NVTS_THREADS_LIBRARY "system default" CACHE STRING
    "Threads library used to build ${PACKAGE_NAME}.")
else()
  set(NVTS_THREADS_LIBRARY "${CMAKE_THREAD_LIBS_INIT}" CACHE STRING
    "Threads library used to build ${PACKAGE_NAME}.")
endif()
message(STATUS
  "[common]: ${PACKAGE_NAME} using threading library: ${NVTS_THREADS_LIBRARY}.")

if (XCODE_VERSION)
  message(STATUS
    "[common]: ${PACKAGE_NAME} using XCode version: ${XCODE_VERSION}.")
endif()

message(STATUS "[common]: ${PACKAGE_NAME} cxx compiler: ${NVTS_CXX_COMPILER}.")
message(STATUS "[common]: ${PACKAGE_NAME} build type: ${NVTS_BUILD_TYPE}.")

# https://github.com/izenecloud/cmake/blob/master/SetCompilerWarningAll.cmake
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Use the highest warning level for Visual Studio.
  set(CMAKE_CXX_WARNING_LEVEL 4)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")

  # Disable C++ exceptions.
  string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs- /EHc-")
  add_definitions(-D_HAS_EXCEPTIONS=0)

  # Disable RTTI.
  string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

  if (NVTS_ENABLE_ASAN)
    message(STATUS "[options]: ASAN enabled.")

    # Use AddressSanitizer.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
  endif (NVTS_ENABLE_ASAN)

  if (NVTS_ENABLE_STATIC_MSVC_RUNTIME)
    # Use static runtime.
    foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if (${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach()
  endif (NVTS_ENABLE_STATIC_MSVC_RUNTIME)
else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Use -Wall for clang and gcc.
  if(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  endif(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")

  # Use -Wextra for clang and gcc.
  if(NOT CMAKE_CXX_FLAGS MATCHES "-Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
  endif(NOT CMAKE_CXX_FLAGS MATCHES "-Wextra")

  # Use -Werror.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

  # Disable C++ exceptions.
  string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

  # Disable RTTI.
  string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

  if (NVTS_ENABLE_ASAN)
    message(STATUS "[options]: ASAN enabled.")

    # Use AddressSanitizer.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  endif (NVTS_ENABLE_ASAN)
endif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

add_library(${PACKAGE_NAME} STATIC "")

target_sources(${PACKAGE_NAME}
  PRIVATE
    NvTriStripObjects.h
    VertexCache.h
    NvTriStrip.cpp
    NvTriStripObjects.cpp

  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/NvTriStrip.h>
)

target_include_directories(${PACKAGE_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)

set_target_properties(${PACKAGE_NAME}
  PROPERTIES
    VERSION ${PACKAGE_VER_MAJOR}.${PACKAGE_VER_MINOR}
    SOVERSION ${PACKAGE_VER_MAJOR}.${PACKAGE_VER_MINOR}
)
