#include "catch.hpp"
#include "catchy/stringeq.h"

#include "lox/printhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"
#include "lox/interpreter.h"

using namespace catchy;

namespace
{
    struct ParseOutput
    {
        std::string out;
        std::vector<std::string> err;
    };
    
    struct RunOutput
    {
        bool ok;
        std::vector<std::string> out;
        std::vector<std::string> err;
    };

    struct AddStringErrors : lox::PrintHandler
    {
        std::vector<std::string>* errors;
        bool error_detected = false;

        AddStringErrors(std::vector<std::string>* o, std::string_view source)
            : lox::PrintHandler(source), errors(o)
        {
        }

        void
        on_line(std::string_view line) override
        {
            errors->emplace_back(line);
        }
    };

    ParseOutput
    parse_to_string(const std::string source)
    {
        auto output = ParseOutput{"<syntax errors>", {}};

        auto printer = AddStringErrors{&output.err, source};
        auto tokens = lox::ScanTokens(source, &printer);
        auto program = lox::parse_program(tokens, &printer);

        if(printer.error_detected == false)
        {
            output.out = lox::print_ast(*program);
        }

        return output;
    }

    RunOutput
    run_string(lox::Interpreter* interpreter, const std::string& source)
    {
        auto output = RunOutput {false, {}, {}};
        auto printer = AddStringErrors{&output.err, source};
        auto tokens = lox::ScanTokens(source, &printer);
        auto program = lox::parse_program(tokens, &printer);
        
        if(printer.error_detected)
        {
            return output;
        }

        output.ok = lox::interpret(interpreter, *program, &printer, [&](const std::string& s){ output.out.emplace_back(s); });
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
    lox::Interpreter lx;

    SECTION("hello world 1")
    {
        const auto out = run_string
        (&lx, R"lox(
            // Your first Lox program!
            print "Hello, world!";
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "Hello, world!"
        }));
    }
    
    SECTION("hello world 2")
    {
        const auto out = run_string
        (&lx, R"lox(
            var hello = 'Hello, world!';
            print hello;
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "Hello, world!"
        }));
    }
    
    SECTION("declare var")
    {
        const auto out = run_string
        (&lx, R"lox(
            var a = 1;
            var b = 2;
            print a + b;
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "3"
        }));
    }
    
    SECTION("assignment")
    {
        const auto out = run_string
        (&lx, R"lox(
            var a;
            var b;
            a = b = 21;
            print a + b;
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "42"
        }));
    }
    
    SECTION("print assignment")
    {
        const auto out = run_string
        (&lx, R"lox(
            var a = 1;
            print a;
            print a = 2;
            print a;
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "1", "2", "2"
        }));
    }

    // todo(Gustav): add block script here

    SECTION("scoping")
    {
        const auto out = run_string
        (&lx, R"lox(
            // How loud?
            var volume = 11;
            // Calculate size of 3x4x5 cuboid.
            {
                var volume = 3 * 4 * 5;
                print volume;
            }
            print volume;

            var global = "outside";
            {
                var local = "inside";
                print global + local;
            }
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "60",
            "11",
            "outsideinside"
        }));
    }

    SECTION("more scoping")
    {
        const auto out = run_string
        (&lx, R"lox(
            var a = "global a";
            var b = "global b";
            var c = "global c";
            {
                var a = "outer a";
                var b = "outer b";
                {
                    var a = "inner a";
                    print a;
                    print b;
                    print c;
                }
                print("------");
                print a;
                print b;
                print c;
            }
            print("------");
            print a;
            print b;
            print c;
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "inner a", "outer b", "global c",
            "------",
            "outer a", "outer b", "global c",
            "------",
            "global a", "global b", "global c"
        }));
    }

    SECTION("while loop")
    {
        const auto out = run_string
        (&lx, R"lox(
            var a = 1;
            var b = 2;
            {
                var a = a + 2;
                var b = b + 2;
                print a;
                print b;
            }
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "3", "4"
        }));
    }
    
    // todo(Gustav): infuse first and second to test all combinations
    SECTION("if else")
    {
        const auto out = run_string
        (&lx, R"lox(
            var first = true;
            var second = false;

            if (first)
                if (second)
                    print "it's true";
                else
                    print "it's false";
            else
                print "it's super false";
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "it's false"
        }));
    }

    SECTION("or")
    {
        const auto out = run_string
        (&lx, R"lox(
            print "hi" or 2; // "hi".
            print nil or "yes"; // "yes".
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "hi",
            "yes"
        }));
    }

    SECTION("while loop")
    {
        const auto out = run_string
        (&lx, R"lox(
            var i = 0;
            while (i < 10)
            {
                print i+1;
                i = i + 1;
            }
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
        }));
    }

    SECTION("fibonacci")
    {
        const auto out = run_string
        (&lx, R"lox(
            // print the first 21 elements in the fibonacci sequence
            var a = 0;
            var temp;

            for (var b = 1; a < 10000; b = temp + b)
            {
                print a;
                temp = a;
                a = b;
            }
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "0", "1", "1", "2", "3", "5", "8", "13", "21", "34", "55", "89",
            "144", "233", "377", "610", "987", "1597", "2584", "4181", "6765"
        }));
    }

    SECTION("add 3 numbers via function")
    {
        const auto out = run_string
        (&lx, R"lox(
            fun add(a, b, c)
            {
                print a + b + c;
            }
            add(1, 2, 3);
            print add;
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "6",
            "<fn add>",
        }));
    }

    SECTION("count to 3")
    {
        const auto out = run_string
        (&lx, R"lox(
            fun count(n)
            {
                if (n > 1) count(n - 1);
                print n;
            }
            count(3);
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "1", "2", "3"
        }));
    }

    SECTION("binding")
    {
        lx.global_environment.define("nat", lox::make_native_function("nat_name",
            [](const lox::Arguments& args)
            {
                lox::verify_number_of_arguments(args, 0);
                return lox::make_string("hello world");
            }
        ));

        const auto out = run_string
        (&lx, R"lox(
            print nat;
            print nat();
        )lox");
        CHECK(out.ok);
        REQUIRE(StringEq(out.err, {}));
        CHECK(StringEq(out.out,{
            "<native fun nat_name>",
            "hello world"
        }));
    }
}
