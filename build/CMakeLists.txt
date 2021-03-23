cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(lisp_impl VERSION 1.0.0 LANGUAGES CXX)
add_executable(lisp_impl ../main.cpp)
set_property(TARGET lisp_impl PROPERTY CXX_STANDARD 17)
