set(src
    lox.main.cc
)

add_executable(lox ${src} ${embedded})
target_link_libraries(
    lox
    PUBLIC liblox
    PRIVATE project_options project_warnings
)
source_group("" FILES ${src})
