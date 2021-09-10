cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(lisp_impl VERSION 1.0.0 LANGUAGES CXX)

set(TARGET "lisp_impl")
set(src "../src")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (WIN32)
  set(platform_sources ${src}/platform/win32.cpp)
elseif (UNIX)
  set(platform_sources ${src}/platform/linux.cpp)
endif ()

# set(CMAKE_CXX_COMPILER g++)
set(sources
  ${platform_sources}
  ${src}/main.cpp ${src}/util.cpp ${src}/objects.cpp ${src}/interpreter.cpp)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
  add_compile_options(-Wall -O0 -g3 -ggdb -gdwarf-3)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3)
endif ()

add_executable(${TARGET} ${sources})

find_package(Threads)
target_link_libraries(${TARGET} ${CMAKE_THREAD_LIBS_INIT})
find_package(fmt)
target_link_libraries(${TARGET} fmt::fmt)
