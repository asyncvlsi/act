cmake_minimum_required(VERSION 3.20)

project(ActLib)
enable_language(CXX)
enable_language(C)
enable_testing()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ensure the environment is configured correctly. We were doing shared libraries wrong before. The right way is to have
# their location added to LD_LIBRARY_PATH
if (NOT DEFINED ENV{ACT_HOME})
    message(FATAL_ERROR "Set the enviroment variable 'ACT_HOME' to the install location")
endif()
string(REGEX MATCH "^(.*:)?$ENV{ACT_HOME}/lib/?(:.*)?$" ACTHOME_LIB_IS_IN_LD_PATH $ENV{LD_LIBRARY_PATH})
if (NOT ACTHOME_LIB_IS_IN_LD_PATH)
    message(FATAL_ERROR "Add $ENV{ACT_HOME}/lib to LD_LIBRARY_PATH")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/config")
    message(FATAL_ERROR "Run `./configure`. This will check that the dependencies are installed, and generate config, which is needed for legacy support in installation")
endif()

include_directories(.)

add_compile_options(-g -ubsanitizer=undefined -O2)

add_subdirectory(act)
add_subdirectory(scripts)
