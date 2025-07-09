cmake_minimum_required(VERSION 3.15)

project(EfmSimProcessor)

option(CXX "enable C++ compilation" ON)
if(CXX)
  enable_language(CXX)
endif()

option(USE_GUROBI "use Gurobi" OFF)
if(USE_GUROBI)
  add_definitions(-DUSE_GUROBI)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# specify the C++ standard
add_library(project_compiler_flags INTERFACE)
target_compile_features(project_compiler_flags INTERFACE cxx_std_17)

# add compiler warning flags just when building this project via
# the BUILD_INTERFACE genex
set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")
target_compile_options(project_compiler_flags INTERFACE
  "$<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wall;-Wextra;-Wformat=2;-Wunused;-Werror=return-type;>>"
  "$<${msvc_cxx}:$<BUILD_INTERFACE:-W3>>"
)

if(USE_GUROBI)
  find_package(GUROBI REQUIRED)
  include_directories(${GUROBI_INCLUDE_DIRS})
endif()

add_subdirectory("external")
add_subdirectory("src")
