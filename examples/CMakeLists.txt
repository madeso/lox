set(src
    lax.main.cc
)

add_executable(lax ${src} ${embedded})
target_link_libraries(
    lax
    PUBLIC
        liblax
    PRIVATE
        lax_project_options
        lax_project_warnings
        external::exit_codes
)
source_group("" FILES ${src})
cmake_source_group()
