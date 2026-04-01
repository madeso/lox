set(LAX_MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(LAX_MASTER_PROJECT ON)
endif ()

cmake_minimum_required(VERSION 3.20.0)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_cmake")
project(lax C CXX)

include(data/cmake/StandardProjectSettings.cmake)
include(data/cmake/CompilerWarnings.cmake)
include(data/cmake/Sanitizers.cmake)

add_library(lax_project_options INTERFACE)
add_library(lax_project_warnings INTERFACE)

target_compile_features(lax_project_options INTERFACE cxx_std_17)
set_project_warnings(lax_project_warnings)
enable_sanitizers(lax_project_options)

function(cmake_source_group)
    source_group("_cmake/pch" REGULAR_EXPRESSION "cmake_pch.[ch]xx$")
    source_group("_cmake/rules" REGULAR_EXPRESSION "\.rule$")
    source_group("_cmake" REGULAR_EXPRESSION "CMakeLists\.txt")
endfunction()

add_subdirectory(external)
add_subdirectory(src)
add_subdirectory(examples)

