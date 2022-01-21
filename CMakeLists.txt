cmake_minimum_required(VERSION 3.20)

project(ActLib)
enable_language(CXX)
enable_language(C)
enable_testing()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

add_link_options(-ldl)

add_subdirectory(act)
add_subdirectory(scripts)
