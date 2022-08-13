#include "catch.hpp"
#include "catchy/stringeq.h"

#include "lox/printhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"
#include "lox/interpreter.h"
#include "lox/resolver.h"

#include "test.h"

using namespace catchy;

namespace
{
    bool
    run_string(std::shared_ptr<lox::Interpreter> interpreter, const std::string& source)
    {
        auto tokens = lox::scan_tokens(source, interpreter->get_error_handler());
        auto program = lox::parse_program(tokens.tokens, interpreter->get_error_handler());
        
        if(tokens.errors > 0 || program.errors > 0)
        {
            return false;
        }

        auto resolved = lox::resolve(*program.program, interpreter->get_error_handler());

        if(resolved.has_value() == false)
        {
            return false;
        }

        return interpreter->interpret(*program.program, *resolved);
    }
}


TEST_CASE("interpret fail", "[interpret]")
{
    std::vector<std::string> console_out;
    std::vector<ReportedError> error_list;
    auto printer = AddErrorErrors{&error_list};
    auto lx = lox::make_interpreter(&printer, [&](const std::string& s){ console_out.emplace_back(s); });

    constexpr ReportedError::Type error = ReportedError::Type::error;
    constexpr ReportedError::Type note = ReportedError::Type::note;

    // todo(Gustav): replace offsets with row + byte offset+length and leave offset handling to another test?

    SECTION("declare 2 var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun bad() {
                var a = "first";
                var a = "second";
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 74, 91, "There is already a variable with this name in this scope"},
            {note, 41, 57, "declared here"}
        }));
    }

    SECTION("return at top level")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            return ":(";
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 13, 25, "Can't return from top-level code"}
        }));
    }

    SECTION("shadowing in non-global")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = 1;
            {
                var a = a + 2;
                print a;
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 62, 63, "Can't read local variable in its own initializer"},
            {note, 54, 68, "declared here"}
        }));
    }

    SECTION("this in global")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            print this;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 19, 23, "Can't use 'this' outside of a class"},
        }));
    }

    SECTION("this in function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun notAMethod()
            {
                print this;
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 66, 70, "Can't use 'this' outside of a class"},
        }));
    }
}


TEST_CASE("interpret ok", "[interpret]")
{
    std::vector<std::string> console_out;
    std::vector<std::string> error_list;
    auto printer = AddStringErrors{&error_list};
    auto lx = lox::make_interpreter(&printer, [&](const std::string& s){ console_out.emplace_back(s); });

    SECTION("hello world 1")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            // Your first Lox program!
            print "Hello, world!";
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Hello, world!"
        }));
    }
    
    SECTION("hello world 2")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var hello = 'Hello, world!';
            print hello;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Hello, world!"
        }));
    }
    
    SECTION("declare var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = 1;
            var b = 2;
            print a + b;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "3"
        }));
    }
    
    SECTION("assignment")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a;
            var b;
            a = b = 21;
            print a + b;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }
    
    SECTION("print assignment")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = 1;
            print a;
            print a = 2;
            print a;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "2", "2"
        }));
    }

    // todo(Gustav): add block script here

    SECTION("scoping")
    {
        const auto run_ok = run_string
        (lx, R"lox(
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
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "60",
            "11",
            "outsideinside"
        }));
    }

    SECTION("more scoping")
    {
        const auto run_ok = run_string
        (lx, R"lox(
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
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "inner a", "outer b", "global c",
            "------",
            "outer a", "outer b", "global c",
            "------",
            "global a", "global b", "global c"
        }));
    }
    
    // todo(Gustav): infuse first and second to test all combinations
    SECTION("if else")
    {
        const auto run_ok = run_string
        (lx, R"lox(
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
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "it's false"
        }));
    }

    SECTION("or")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            print "hi" or 2; // "hi".
            print nil or "yes"; // "yes".
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "hi",
            "yes"
        }));
    }

    SECTION("while loop")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var i = 0;
            while (i < 10)
            {
                print i+1;
                i = i + 1;
            }
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"
        }));
    }

    SECTION("fibonacci")
    {
        const auto run_ok = run_string
        (lx, R"lox(
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
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "0", "1", "1", "2", "3", "5", "8", "13", "21", "34", "55", "89",
            "144", "233", "377", "610", "987", "1597", "2584", "4181", "6765"
        }));
    }

    SECTION("add 3 numbers via function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun add(a, b, c)
            {
                print a + b + c;
            }
            add(1, 2, 3);
            print add;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "6",
            "<fn add>",
        }));
    }

    SECTION("count to 3")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun count(n)
            {
                if (n > 1) count(n - 1);
                print n;
            }
            count(3);
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "2", "3"
        }));
    }

    SECTION("print void function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun procedure()
            {
                print "don't return anything";
            }
            var result = procedure();
            print result;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "don't return anything",
            "nil"
        }));
    }
    
    SECTION("early return")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun count(n)
            {
                while (n < 100)
                {
                    if (n == 3) return n;
                    print n;
                    n = n + 1;
                }
            }
            count(1);
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "2"
        }));
    }


    SECTION("reccursive fibonacci")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun fib(n)
            {
                if (n <= 1) return n;
                return fib(n - 2) + fib(n - 1);
            }

            for(var i = 0; i < 10; i = i + 1)
            {
                print fib(i);
            }
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "0", "1", "1", "2", "3", "5", "8", "13", "21", "34"
        }));
    }

    SECTION("local functions and closures")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun makeCounter()
            {
                var i = 0;
                fun count()
                {
                    i = i + 1;
                    print i;
                }
                return count;
            }

            var counter = makeCounter();
            counter();
            counter();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "2"
        }));
    }
    
    SECTION("local functions and closures")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = "global";
            {
                fun showA()
                {
                    print a;
                }

                showA();
                var a = "block";
                showA();
            }
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "global", "global"
        }));
    }
    
    SECTION("simple class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class HelloWorlder
            {
                get_string()
                {
                    return "Hello, world!";
                }
            }

            print HelloWorlder;
            var instance = HelloWorlder();
            print instance;
            print instance.get_string();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "<class HelloWorlder>",
            "<instance HelloWorlder>",
            "Hello, world!"
        }));
    }

    SECTION("simple properies on class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Classy{}
            var instance = Classy();
            instance.animals = "I love cats!";
            print instance.animals;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "I love cats!"
        }));
    }

    
    SECTION("called function on class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Box {}

            fun notMethod(argument)
            {
                print "called function with " + argument;
            }

            var box = Box();
            box.function = notMethod;
            box.function("argument");
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "called function with argument"
        }));
    }

    SECTION("simple class with members and constructor")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Adder
            {
                init(start)
                {
                    this.string = start;
                }

                add(more)
                {
                    this.string = this.string + more;
                }

                get()
                {
                    return this.string;
                }
            }

            var str = Adder("Hello");
            str.add(", ");
            str.add("world!");
            print str.get();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Hello, world!"
        }));
    }

    SECTION("invoking constructor directly")
    {
        // todo(Gustav): should this be supported?
        const auto run_ok = run_string
        (lx, R"lox(
            class Foo
            {
                init()
                {
                    print this;
                }
            }

            var foo = Foo();
            print foo.init();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "<instance Foo>",
            "<instance Foo>",
            "<instance Foo>"
        }));
    }
    
    SECTION("bound method: called method on class with out dot notation")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Person
            {
                sayName()
                {
                    print this.name;
                }
            }

            var jane = Person();
            jane.name = "Jane";

            var method = jane.sayName;
            method();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Jane"
        }));
    }
    
    SECTION("bound method: return this in callback")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Thing
            {
                getCallback()
                {
                    fun localFunction()
                    {
                        print this;
                    }

                    return localFunction;
                }
            }

            var callback = Thing().getCallback();
            callback();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "<instance Thing>"
        }));
    }
    
    SECTION("bound method: call method on class with dot instance on other class instance")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Person
            {
                sayName()
                {
                    print this.name;
                }
            }

            var jane = Person();
            jane.name = "Jane";

            var bill = Person();
            bill.name = "Bill";

            bill.sayName = jane.sayName;
            bill.sayName();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Jane"
        }));
    }
}
