if(LOX_MASTER_PROJECT)
    add_subdirectory(catchy)

    include(FetchContent)

    # fmt
    FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG        407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) # https://github.com/fmtlib/fmt/releases/tag/12.1.0
    FetchContent_MakeAvailable(fmt)
    add_library(external_fmt INTERFACE)
    target_link_libraries(external_fmt INTERFACE fmt::fmt)
    add_library(external::fmt ALIAS external_fmt)

    # catch
    set(CATCH_GIT_TAG v3.7.1)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${CATCH_GIT_TAG}
    )
    FetchContent_MakeAvailable(Catch2)
    set_target_properties(Catch2 PROPERTIES FOLDER "External")
    add_library(catch_main INTERFACE)
    target_link_libraries(catch_main INTERFACE Catch2::Catch2 )
    add_library(external::catch ALIAS catch_main)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)

endif()

add_library(exit_codes INTERFACE)
target_include_directories(exit_codes INTERFACE exit_codes)
add_library(external::exit_codes ALIAS exit_codes)
