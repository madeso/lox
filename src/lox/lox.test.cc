#include "catch.hpp"
#include "catchy/stringeq.h"

#include "lox/printhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"

using namespace catchy;

namespace
{
    struct Output
    {
        std::string out;
        std::vector<std::string> err;
    };

    struct AddErrors : lox::PrintHandler
    {
        Output* output;
        bool error_detected = false;

        AddErrors(Output* o, std::string_view source)
            : lox::PrintHandler(source), output(o)
        {
        }

        void
        on_line(std::string_view line) override
        {
            output->err.emplace_back(line);
        }
    };

    Output parse_to_string(const std::string source)
    {
        auto output = Output{"<syntax errors>", {}};

        auto printer = AddErrors{&output, source};
        auto tokens = lox::ScanTokens(source, &printer);
        auto program = lox::parse_program(tokens, &printer);

        if(printer.error_detected == false)
        {
            output.out = lox::print_ast(*program);
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


TEST_CASE("interpret", "[interpret]")
{
    // lox::Lox lx;
}
