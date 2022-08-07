#include "catch.hpp"
#include "catchy/stringeq.h"

#include "lox/printhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"

#include "test.h"


using namespace catchy;


namespace
{
    ParseOutput
    parse_to_string(const std::string source)
    {
        auto output = ParseOutput{"<syntax errors>", {}};

        auto printer = AddStringErrors{&output.err};
        auto tokens = lox::scan_tokens(source, &printer);
        auto program = lox::parse_program(tokens.tokens, &printer);

        if(tokens.errors == 0 && program.errors == 0)
        {
            output.out = lox::print_ast(*program.program);
        }

        return output;
    }
}


#define XSTR(x) #x
#define STR(x) XSTR(x)
#define XSECTION() SECTION(STR(__LINE__))

TEST_CASE("parser", "[parser]")
{
    XSECTION()
    {
        const auto out = parse_to_string
        (R"lox(
            var foo = 42;
            print foo;
        )lox");
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out, "(program (decl foo 42) (print (get foo)))"));
    }
    XSECTION()
    {
        const auto out = parse_to_string
        (R"lox(
            var i = 0;
            while (i < 10)
            {
                print i+1;
                i = i + 1;
            }
        )lox");
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out, "(program (decl i 0) (while (< (get i) 10) ({} (print (+ (get i) 1)) (expr (= i (+ (get i) 1))))))"));
    }
}
