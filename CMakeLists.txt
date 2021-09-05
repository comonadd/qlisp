cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(lisp_impl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_COMPILER g++)

set(TARGET "lisp_impl")
set(src "../src")

if (WIN32)
  set(platform_sources ${src}/platform/win32.cpp)
elseif (UNIX)
  set(platform_sources ${src}/platform/linux.cpp)
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
endif ()

set(sources ${src}/main.cpp ${src}/util.cpp ${src}/objects.cpp ${src}/interpreter.cpp)
add_compile_options(-Wall)
add_executable(${TARGET} ${platform_sources} ${sources})
set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 20)
