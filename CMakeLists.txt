cmake_minimum_required(VERSION 3.15)
set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
project(lox C CXX)
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(data/cmake/StandardProjectSettings.cmake)
include(data/cmake/CompilerWarnings.cmake)
include(data/cmake/Sanitizers.cmake)

add_library(lox_project_options INTERFACE)
add_library(lox_project_warnings INTERFACE)

target_compile_features(lox_project_options INTERFACE cxx_std_17)
set_project_warnings(lox_project_warnings)
enable_sanitizers(lox_project_options)

add_subdirectory(external)
add_subdirectory(src)
add_subdirectory(examples)

