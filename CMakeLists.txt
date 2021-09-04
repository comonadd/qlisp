cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(lisp_impl VERSION 1.0.0 LANGUAGES CXX)
if (WIN32)
  set(platform_sources ../platform/win32.cpp)
elseif (UNIX)
  set(platform_sources ../platform/linux.cpp)
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
endif ()
add_executable(lisp_impl ../main.cpp ${platform_sources})
add_compile_options(
  "-Wall" "-Wpedantic" "-Wextra" "-fexceptions"
  "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
)
set_property(TARGET lisp_impl PROPERTY CXX_STANDARD 17)
