if(LOX_MASTER_PROJECT)
    add_subdirectory(catchy)
endif()

add_library(exit_codes INTERFACE)
target_include_directories(exit_codes INTERFACE exit_codes)
add_library(external::exit_codes ALIAS exit_codes)
