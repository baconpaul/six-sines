cmake_minimum_required(VERSION 3.28)
project(integer-fm VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE libs/simde)
add_subdirectory(libs/sst/sst-basic-blocks)
add_executable(ifm-main src/main.cpp)
target_link_libraries(ifm-main PRIVATE sst-basic-blocks)
