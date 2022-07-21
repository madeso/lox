set(src
    lox/errorhandler.cc lox/errorhandler.h
    lox/object.cc lox/object.h
    lox/offset.h
    lox/scanner.cc lox/scanner.h
    lox/stringmap.cc lox/stringmap.h
    lox/token.cc lox/token.h
    lox/tokentype.h
)

add_library(liblox STATIC ${src})
target_link_libraries(liblox
    PUBLIC fmt::fmt
    PRIVATE project_options project_warnings
)
target_include_directories(liblox
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_precompile_headers(liblox
    PUBLIC lox/pch.public.h
    PRIVATE lox/pch.private.h
)


source_group("" FILES ${src})

set(src_test
    lox/lox.test.cc
    ../external/catch/main.cc
)
add_executable(test ${src_test})
target_link_libraries(test
    PUBLIC catch catchy liblox
    PRIVATE project_options project_warnings
)
