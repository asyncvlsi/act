cmake_minimum_required(VERSION 3.10)

project(ActProject)
include_directories( . )
# include_directories( $ENV{ACT_HOME}/include/ ) add_library( ActLib SHARED
# IMPORTED ) set_target_properties( ActLib PROPERTIES IMPORTED_LOCATION
# $ENV{ACT_HOME}/lib/libact.a ) add_library( ActCommon SHARED IMPORTED )
# set_target_properties( ActCommon PROPERTIES IMPORTED_LOCATION
# $ENV{ACT_HOME}/lib/libvlsilib.a ) add_library( ActPasses SHARED IMPORTED )
# set_target_properties( ActPasses PROPERTIES IMPORTED_LOCATION
# $ENV{ACT_HOME}/lib/libactpass.a )

# set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++20 -O2 -g -fsanitize=undefined -march=native -DARCH_x86_64 -DOS_linux5_14_9 -DBASEOS_linux" #  -Wall -Wpedantic -Werror"
)
# -H
set(CMAKE_EXE_LINKER_FLAGS "-ldl")
# set(CMAKE_VERBOSE_MAKEFILE on)
add_subdirectory(common)
add_subdirectory(pgen)
 add_subdirectory(miniscm)
add_subdirectory(act)
add_subdirectory(passes)
# add_subdirectory(transform)
# add_subdirectory(simulation) add_subdirectory(tech) add_subdirectory(scripts)

# add_subdirectory(Catch2)

# add_subdirectory( src )
