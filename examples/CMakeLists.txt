set(src
    lox.main.cc
)

add_executable(lox ${src} ${embedded})
target_link_libraries(
    lox
    PUBLIC
        liblox
    PRIVATE
        lox_project_options
        lox_project_warnings
        external::exit_codes
)
source_group("" FILES ${src})
