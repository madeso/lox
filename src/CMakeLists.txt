set(src
    lox/cint.cc lox/cint.h
)

add_library(liblox STATIC ${src})
target_link_libraries(liblox
    PUBLIC fmt::fmt
    PRIVATE project_options project_warnings
)
target_include_directories(liblox
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

source_group("" FILES ${src})

set(src_test
    # lox/foo.test.cc
    ../external/catch/main.cc
)
add_executable(test ${src_test})
target_link_libraries(test
    PUBLIC catch catchy liblox
    PRIVATE project_options project_warnings
)
