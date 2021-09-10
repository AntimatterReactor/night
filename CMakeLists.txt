cmake_minimum_required(VERSION 3.0)

project(night LANGUAGES CXX VERSION 1.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall -Wextra -O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-W0 -O3 ")

include_directories(${PROJECT_SOURCE_DIR}/night/include)

configure_file(${PROJECT_SOURCE_DIR}/night/include/cmakedef.hpp.in ${PROJECT_SOURCE_DIR}/night/include/cmakedef.hpp @ONLY)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "night/src/*.cpp")
add_executable(night ${SOURCES})
