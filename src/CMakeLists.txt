# -----------------------------------------------------------------------------
# ast generator
set(gen_src
    lax/generate_ast.cc
)
add_executable(generate_ast ${gen_src})
target_link_libraries(generate_ast
    PUBLIC external::fmt
    PRIVATE lax_project_options lax_project_warnings external::exit_codes
)
source_group("" FILES ${gen_src})



# -----------------------------------------------------------------------------
# generate expression
set(EXPR_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/lax_expression.cc")
set(EXPR_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lax_expression.h")
add_custom_command(
    OUTPUT ${EXPR_SOURCE} ${EXPR_HEADER}
    DEPENDS ${gen_src}
    COMMAND generate_ast ${EXPR_SOURCE} ${EXPR_HEADER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating expression"
)



# -----------------------------------------------------------------------------
# main laxlib
set(lax_src
    lax/source.cc lax/source.h
    lax/ast.cc lax/ast.h
    lax/chunk.cc lax/chunk.h
    lax/debug.cc lax/debug.h
    lax/config.h
    lax/environment.cc lax/environment.h
    lax/errorhandler.cc lax/errorhandler.h
    lax/interpreter.cc lax/interpreter.h
    lax/ints.h
    lax/object.cc lax/object.h
    lax/parser.cc lax/parser.h
    lax/printhandler.cc lax/printhandler.h
    lax/program.cc lax/program.h
    lax/resolver.h lax/resolver.cc
    lax/scanner.cc lax/scanner.h
    lax/token.cc lax/token.h
    lax/tokentype.h
    lax/lax.cc lax/lax.h
)

add_library(liblax STATIC ${lax_src} ${EXPR_SOURCE} ${EXPR_HEADER})
target_link_libraries(liblax
    PUBLIC external::fmt
    PRIVATE lax_project_options lax_project_warnings
)
target_include_directories(liblax
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_precompile_headers(liblax
    PUBLIC lax/pch.public.h
    PRIVATE lax/pch.private.h
)
source_group("" FILES ${lax_src})
source_group("generated" FILES ${EXPR_SOURCE} ${EXPR_HEADER})
add_library(lax::liblax ALIAS liblax)


# -----------------------------------------------------------------------------
# unit test
if(LAX_MASTER_PROJECT)
    set(src_test
        lax/test.cc lax/test.h
        ../external/catch/main.cc

        lax/parser.test.cc
        lax/interpreter.test.cc
        lax/lax.test.cc
    )
    add_executable(test ${src_test})
    target_link_libraries(test
        PUBLIC external::catch catchy::catchy lax::liblax
        PRIVATE lax_project_options lax_project_warnings
    )
endif()
