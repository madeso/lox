# -----------------------------------------------------------------------------
# ast generator
set(gen_src
    lox/generate_ast.cc
)
add_executable(generate_ast ${gen_src})
target_link_libraries(generate_ast
    PUBLIC fmt::fmt
    PRIVATE lox_project_options lox_project_warnings external::exit_codes
)
source_group("" FILES ${gen_src})



# -----------------------------------------------------------------------------
# generate expression
set(EXPR_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/lox_expression.cc")
set(EXPR_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lox_expression.h")
add_custom_command(
    OUTPUT ${EXPR_SOURCE} ${EXPR_HEADER}
    DEPENDS ${gen_src}
    COMMAND generate_ast ${EXPR_SOURCE} ${EXPR_HEADER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating expression"
)



# -----------------------------------------------------------------------------
# main loxlib
set(lox_src
    lox/source.cc lox/source.h
    lox/ast.cc lox/ast.h
    lox/config.h
    lox/environment.cc lox/environment.h
    lox/errorhandler.cc lox/errorhandler.h
    lox/interpreter.cc lox/interpreter.h
    lox/ints.h
    lox/object.cc lox/object.h
    lox/parser.cc lox/parser.h
    lox/printhandler.cc lox/printhandler.h
    lox/program.cc lox/program.h
    lox/resolver.h lox/resolver.cc
    lox/scanner.cc lox/scanner.h
    lox/token.cc lox/token.h
    lox/tokentype.h
    lox/lox.cc lox/lox.h
)

add_library(liblox STATIC ${lox_src} ${EXPR_SOURCE} ${EXPR_HEADER})
target_link_libraries(liblox
    PUBLIC fmt::fmt
    PRIVATE lox_project_options lox_project_warnings
)
target_include_directories(liblox
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_precompile_headers(liblox
    PUBLIC lox/pch.public.h
    PRIVATE lox/pch.private.h
)
source_group("" FILES ${lox_src})
source_group("generated" FILES ${EXPR_SOURCE} ${EXPR_HEADER})
add_library(lox::liblox ALIAS liblox)


# -----------------------------------------------------------------------------
# unit test
set(src_test
    lox/test.cc lox/test.h
    ../external/catch/main.cc

    lox/parser.test.cc
    lox/interpreter.test.cc
    lox/lox.test.cc
)
add_executable(test ${src_test})
target_link_libraries(test
    PUBLIC catch catchy liblox
    PRIVATE lox_project_options lox_project_warnings
)
