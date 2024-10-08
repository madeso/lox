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
            {error, 74, 91, {"There is already a variable with this name in this scope"}},
            {note, 41, 57, {"declared here"}}
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
            {error, 13, 25, {"Can't return from top-level code"}}
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
            {error, 62, 63, {"Can't read local variable in its own initializer"}},
            {note, 54, 68, {"declared here"}}
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
            {error, 19, 23, {"Can't use 'this' outside of a class"}},
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
            {error, 66, 70, {"Can't use 'this' outside of a class"}}
        }));
    }
    
    SECTION("returning from initializer")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Foo {
                fun init()
                {
                    return "something else";
                }
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 97, 121, {"Can't return value from initializer"}}
        }));
    }

    SECTION("print missing var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            print foo;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 19, 22, {"Undefined variable foo"}},
        }));
    }

    SECTION("assign non declared var globally")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            foo = 42;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 13, 21, {"Global variable foo was never declared"}},
        }));
    }

    SECTION("assign non declared var in function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun test() { foo = 42; }
            test();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 26, 34, {"Variable foo was neither declared in global nor local scope"}},
            {note, 54, 56, {"called from here"}}
        }));
    }

    SECTION("call missing method on var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Foo {}
            var foo = new Foo();
            foo.bar();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 75, 78, {"<instance Foo> doesn't have a property named bar"}},
        }));
    }

    SECTION("can't declare 2 members with the same same")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Foo
            {
                var foo;
                var foo;
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 92, 100, {"'foo' declared multiple times"}},
            {note, 60, 68, {"as var foo here"}},
            {note, 92, 100, {"as var foo here"}},
        }));
    }

    SECTION("can't have a method and var with the same name")
    {
        SECTION("var fun")
        {
            const auto run_ok = run_string
            (lx, R"lox(
                class Foo
                {
                    var foo;
                    fun foo() {}
                }
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(StringEq(console_out,{}));
            CHECK(ErrorEq(error_list, {
                {error, 112, 120, {"'foo' declared multiple times"}},
                {note, 72, 80, {"as var foo here"}},
                {note, 112, 120, {"as fun foo here"}},
            }));
        }
        SECTION("fun var")
        {
            const auto run_ok = run_string
            (lx, R"lox(
                class Foo
                {
                    fun foo() {}
                    var foo;
                }
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(StringEq(console_out,{}));
            CHECK(ErrorEq(error_list, {
                {error, 112, 120, {"'foo' declared multiple times"}},
                {note, 76, 84, {"as fun foo here"}},
                {note, 112, 120, {"as var foo here"}},
            }));
        }
    }

    SECTION("must use new on class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Foo {}
            var foo = Foo();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 48, 51, {"class is not a callable, evaluates to <class Foo>"}},
            {note, 48, 51, {"did you forget to use new?"}},
            {note, 51, 53, {"call occured here"}},
        }));
    }

    SECTION("call missing method on function returning nil")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun get() { return nil; }
            var foo = get();
            foo.bar();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 84, 87, {"nil is not capable of having any properties"}},
        }));
    }

    SECTION("call missing method on function returning string")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            fun get() { return "cats"; }
            var foo = get();
            foo.bar();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 87, 90, {"string is not capable of having any properties, has value \"cats\""}},
        }));
    }
    
    SECTION("inheritance: infinite chain")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Oops : Oops {}
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            // todo(Gustav): fix this error
            // A class can't inherit from itself
            {error, 26, 30, {"Undefined variable Oops"}},
        }));
    }
    
    SECTION("inheritance: need to inherit from class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var NotAClass = "I am totally not a class";
            class Subclass : NotAClass {}
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 86, 95, {"Superclass must be a class, was string"}},
        }));
    }
    
    SECTION("inheritance: bare super")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            print super;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 24, 25, {"Expected '.' after 'super' keyword"}},
        }));
    }
    
    SECTION("inheritance: super call outside of class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            super.notEvenInAClass();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 13, 34, {"Can't use 'super' outside of class"}},
        }));
    }
    
    SECTION("inheritance: super in base class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                fun say()
                {
                    super.say();
                    print "Oh no";
                }
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 109, 118, {"Can't use 'super' in class with no superclass"}},
        }));
    }

    SECTION("super in static function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                fun say()
                {
                    print "hello";
                }
            }

            class Derived
            {
                static fun fail()
                {
                    super.say();
                }
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 276, 285, {"Can't use 'super' in a static method"}},
        }));
    }

    SECTION("this in static function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                var member;
                static fun say()
                {
                    this.member = "fail";
                }
            }
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 151, 155, {"Can't use 'this' in a static method"}},
        }));
    }

    SECTION("index array set with string")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            a['dog'] = 24;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 40, 52, {"array index needs to be a int, was string"}},
            {note, 39, 40, {"object evaluated to [42]"}},
            {note, 41, 46, {"index evaluated to \"dog\""}},
            {note, 50, 52, {"value evaluated to 24"}},
        }));
    }

    SECTION("index array set outside positive")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            a[1] = 24;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 40, 48, {"array index 1 is out of range, needs to be lower than 1"}},
            {note, 39, 40, {"object evaluated to [42]"}},
            {note, 41, 42, {"index evaluated to 1"}},
            {note, 46, 48, {"value evaluated to 24"}},
        }));
    }

    SECTION("index array set outside negative")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            a[-42] = 24;
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 40, 50, {"array index needs to be positive, was -42"}},
            {note, 39, 40, {"object evaluated to [42]"}},
            {note, 41, 44, {"index evaluated to -42"}},
            {note, 48, 50, {"value evaluated to 24"}},
        }));
    }

    SECTION("index array get with string")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            print a['dog'];
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 46, 53, {"array index needs to be a int, was string"}},
            {note, 45, 46, {"object evaluated to [42]"}},
            {note, 47, 52, {"index evaluated to \"dog\""}},
        }));
    }

    SECTION("index array get outside positive")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            print a[1];
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 46, 49, {"array index 1 out of range, needs to be lower than 1"}},
            {note, 45, 46, {"object evaluated to [42]"}},
            {note, 47, 48, {"index evaluated to 1"}},
        }));
    }

    SECTION("index array get outside negative")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            print a[-42];
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 46, 51, {"array index needs to be positive, was -42"}},
            {note, 45, 46, {"object evaluated to [42]"}},
            {note, 47, 50, {"index evaluated to -42"}},
        }));
    }

    SECTION("now invalid headscratcher: can't assign missing var/override functions in class")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Person
            {
                var name;

                fun sayName()
                {
                    print this.name;
                }
            }

            var jane = new Person();
            jane.name = "Jane";

            var bill = new Person();
            bill.name = "Bill";

            bill.sayName = jane.sayName;
            bill.sayName();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 356, 378, {"<instance Person> doesn't have a property named sayName"}},
        }));
    }

    SECTION("now invalid: base access can access derived value")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                fun say()
                {
                    print this.val;
                }
            }
            
            class Derived : Base
            {
                var val;
                fun init(v)
                {
                    this.val = v;
                }
            }
            new Derived("hello").say();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 120, 123, {"<instance Base> doesn't have a property named val"}},
            {note, 404, 406, {"called from here"}}
        }));
    }

    SECTION("call base ctor without base")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Derived
            {
                fun init()
                {
                    super("dog");
                }
            }
            new Derived();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 113, 125, {"Can't use 'super' in class with no superclass"}},
        }));
    }

    SECTION("access super before initialized")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                var data;

                fun init(d)
                {
                    this.data = d;
                }

                fun say()
                {
                    print this.data;
                }
            }

            class Derived : Base
            {
                fun init()
                {
                    print super.data;
                }
            }

            new Derived().say();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{}));
        CHECK(ErrorEq(error_list, {
            {error, 425, 435, {"Superclass is not initialized. It need to be manually initialized in init with a call to super() for super to work"}},
            {note, 493, 495, {"called from here"}},
        }));
    }

    SECTION("don't call required ctor in init")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                var data;

                fun init(d)
                {
                    this.data = d;
                }

                fun say()
                {
                    print this.data;
                }
            }

            class Derived : Base
            {
                fun init()
                {
                    print "in Derived::init";
                }
            }

            new Derived().say();
        )lox");
        CHECK_FALSE(run_ok);
        CHECK(StringEq(console_out,{"in Derived::init"}));
        CHECK(ErrorEq(error_list, {
            {error, 501, 503, {"Expected 1 arguments but got 0 while implicitly calling constructor for superclass Base"}},
            {note, 494, 501, {"called with 0 arguments"}},
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
                fun get_string()
                {
                    return "Hello, world!";
                }
            }

            print HelloWorlder;
            var instance = new HelloWorlder();
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
            class Classy{ var animals; }
            var instance = new Classy();
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
            class Box { var function; }

            fun notMethod(argument)
            {
                print "called function with " + argument;
            }

            var box = new Box();
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
                var string;

                fun init(start)
                {
                    this.string = start;
                }

                fun add(more)
                {
                    this.string = this.string + more;
                }

                fun get()
                {
                    return this.string;
                }
            }

            var str = new Adder("Hello");
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
                fun init()
                {
                    print this;
                }
            }

            var foo = new Foo();
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

    SECTION("void function return nil")
    {
        // todo(Gustav): should this be supported?
        const auto run_ok = run_string
        (lx, R"lox(
            class Foo
            {
                fun nop1() {}
            }

            fun nop2() {}

            var foo = new Foo();
            var r1 = foo.nop1();
            var r2 = nop2();
            print r1;
            print r2;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "nil",
            "nil"
        }));
    }
    
    SECTION("early return")
    {
        // todo(Gustav): should this be supported?
        const auto run_ok = run_string
        (lx, R"lox(
            class Animal
            {
                var val;

                fun init(val)
                {
                    this.val = val;
                    return; // "early" return, weird here but legal
                }
            }

            var r;
            var foo = new Animal("dogs?");
            print foo.val;
            r = foo.init("cats!");
            print foo.val;
            print r;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "dogs?",
            "cats!",
            "<instance Animal>"
        }));
    }
    
    SECTION("bound method: called method on class with out dot notation")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Person
            {
                var name;

                fun sayName()
                {
                    print this.name;
                }
            }

            var jane = new Person();
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
                fun getCallback()
                {
                    fun localFunction()
                    {
                        print this;
                    }

                    return localFunction;
                }
            }

            var callback = new Thing().getCallback();
            callback();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "<instance Thing>"
        }));
    }
    
    SECTION("inheritance: call function in derived from base")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                fun say()
                {
                    print "Hello, world!";
                }
            }

            class Derived : Base {}
            new Derived().say();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Hello, world!"
        }));
    }
    
    SECTION("inheritance: call overloaded function in derived")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                fun say()
                {
                    print "base";
                }
            }

            class Derived : Base
            {
                fun say()
                {
                    print "derived";
                }
            }
            new Derived().say();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "derived"
        }));
    }
    
    SECTION("inheritance: basic super call in overloaded function")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                fun say()
                {
                    print "base";
                }
            }

            class Derived : Base
            {
                fun say()
                {
                    super.say();
                    print "derived";
                }
            }

            new Derived().say();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "base", "derived"
        }));
    }
    
    SECTION("inheritance: call super in derived")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class A
            {
                fun method()
                {
                    print "A method";
                }
            }

            class B : A
            {
                fun method()
                {
                    print "B method";
                }

                fun test()
                {
                    super.method();
                }
            }

            class C : B {}

            new C().test();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "A method"
        }));
    }

    SECTION("inheritance: change property in base")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class A
            {
                var foo = 42;

                fun say()
                {
                    print this.foo;
                }
            }

            class B : A
            {
                fun test()
                {
                    print this.foo;
                    this.foo = "cats <3";
                    this.say();
                }
            }

            new B().test();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42", "cats <3"
        }));
    }

    SECTION("static method")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class A
            {
                static fun method()
                {
                    print "Hello, world!";
                }
            }

            A.method();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Hello, world!"
        }));
    }

    SECTION("method calls static method")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class A
            {
                static fun method()
                {
                    print "Hello, world!";
                }

                fun say()
                {
                    A.method();
                }
            }

            new A().say();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "Hello, world!"
        }));
    }

    SECTION("print array")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [1, 2, 3];
            print a;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "[1, 2, 3]"
        }));
    }

    SECTION("print array length")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [4, 5, 6];
            print a.len();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "3"
        }));
    }

    SECTION("push items to array")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [];
            print a;
            a.push(42);
            print a;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "[]",
            "[42]"
        }));
    }

    SECTION("index get and set")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var a = [42];
            a[0] = 24;
            print a[0];
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "24"
        }));
    }

    SECTION("op equal: += int var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var life = 40;
            life += 2;
            print life;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }
    
    SECTION("op equal: -= int var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var life = 24;
            life -= 2;
            print life;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "22"
        }));
    }

    SECTION("op equal: *= int var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var life = 24;
            life *= 10;
            print life;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "240"
        }));
    }

    SECTION("op equal: /= var")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var life = 24;
            life /= 2;
            print life;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "12"
        }));
    }

    SECTION("op equal: += string class property")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Classy{ var animals; }
            var instance = new Classy();
            instance.animals = "I love";
            instance.animals += " cats!";
            print instance.animals;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "I love cats!"
        }));
    }

    SECTION("op equal: += int array")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            var arr = [10];
            arr[0] += 2;
            print arr[0];
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "12"
        }));
    }

    SECTION("call class ctor")
    {
        const auto run_ok = run_string
        (lx, R"lox(
            class Base
            {
                var data;

                fun init(d)
                {
                    this.data = d;
                }

                fun say()
                {
                    print this.data;
                }
            }

            class Derived : Base
            {
                fun init()
                {
                    super("dog");
                }
            }

            new Derived().say();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "dog"
        }));
    }
}

