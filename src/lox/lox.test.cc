#include "catch.hpp"
#include "catchy/stringeq.h"

#include "lox/lox.h"
#include "lox/environment.h"

#include "test.h"

using namespace catchy;


TEST_CASE("lox binding fail" "[lox]")
{
    std::vector<std::string> console_out;
    std::vector<ReportedError> error_list;

    auto lox = lox::Lox{std::make_unique<AddErrorErrors>(&error_list), [&](const std::string& s){ console_out.emplace_back(s); }};

    constexpr ReportedError::Type error = ReportedError::Type::error;
    constexpr ReportedError::Type note = ReportedError::Type::note;

    SECTION("function 1 string arg")
    {
        lox.in_global_scope()->define_native_function
        (
            "nat",
            [](lox::Callable*, lox::ArgumentHelper& helper)
            {
                auto arg = helper.require_string();
                helper.complete();
                return lox::make_string(arg);
            }
        );

        SECTION("send 0 arguments")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                nat();
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 20, 22, "Expected 1 arguments but got 0"},
                {note, 17, 20, "called with 0 arguments"}
            }));
            CHECK(StringEq(console_out,{}));
        }

        SECTION("send wrong type")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                nat(42);
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 20, 24, "number (42) is not accepted for argument 1, expected string"},
            }));
            CHECK(StringEq(console_out,{}));
        }

        SECTION("send too many arguments")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                nat("hello", "world");
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 20, 38, "Expected 1 arguments but got 2"},
                {note, 17, 20, "called with 2 arguments"},
                {note, 21, 28, "argument 1 evaluated to string: hello"},
                {note, 30, 37, "argument 2 evaluated to string: world"},
            }));
            CHECK(StringEq(console_out,{}));
        }
    }

    SECTION("native class default constructor")
    {
        struct Adder
        {
            std::string value;
        };
        lox.in_global_scope()->define_native_class<Adder>("Adder")
            .add_property<std::string>
            (
                "value",
                [](Adder& c) { return c.value; },
                [](Adder& c, const std::string& new_value) { c.value = new_value; }
            )
            ;

        SECTION("set property invalid type")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var adder = new Adder();
                adder.value = 24;
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 64, 74, "number (24) is not accepted for property 'value' on <native instance Adder>, expected string"},
            }));
            CHECK(StringEq(console_out, {}));
        }
    }
}


TEST_CASE("lox binding" "[lox]")
{
    std::vector<std::string> console_out;
    std::vector<std::string> error_list;

    auto lox = lox::Lox{std::make_unique<AddStringErrors>(&error_list), [&](const std::string& s){ console_out.emplace_back(s); }};


    SECTION("lox -> cpp binding")
    {
        lox.in_global_scope()->define_native_function
        (
            "nat",
            [](lox::Callable*, lox::ArgumentHelper& helper)
            {
                helper.complete();
                return lox::make_string("hello world");
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            print nat;
            print nat();
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "<native fun nat>",
            "hello world"
        }));
    }

    SECTION("cpp -> lox binding")
    {
        const auto run_ok = lox.run_string
        (R"lox(
            fun hello(name)
            {
                return "goodbye cruel " + name;
            }
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{}));

        auto function_object = lox.get_global_environment().get_or_null("hello");
        REQUIRE(function_object != nullptr);

        auto callable = lox::as_callable(function_object);
        REQUIRE(callable != nullptr);
        
        auto res = callable->call({{lox::make_string("world")} });
        auto str = lox::as_string(res);
        
        REQUIRE(str);
        REQUIRE(*str == "goodbye cruel world");
    }

    SECTION("argument helper: numbers")
    {
        lox.in_global_scope()->define_native_function
        (
            "add",
            [](lox::Callable*, lox::ArgumentHelper& ah)
            {
                auto lhs = ah.require_number();
                auto rhs = ah.require_number();
                ah.complete();
                return lox::make_number(lhs + rhs);
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            print add(40, 2);
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }

    SECTION("argument helper: strings")
    {
        lox.in_global_scope()->define_native_function
        (
            "add",
            [](lox::Callable*, lox::ArgumentHelper& ah)
            {
                auto lhs = ah.require_string();
                auto rhs = ah.require_string();
                ah.complete();
                return lox::make_bool(lhs < rhs);
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            print add("abc", "xyz");
            print add("xyz", "abc");
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "true", "false"
        }));
    }

    SECTION("argument helper: boolean")
    {
        lox.in_global_scope()->define_native_function
        (
            "add",
            [](lox::Callable*, lox::ArgumentHelper& ah)
            {
                auto lhs = ah.require_bool();
                ah.complete();
                if(lhs) return lox::make_string("yes!");
                else    return lox::make_string("or no?");
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            print add(true);
            print add(false);
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "yes!", "or no?"
        }));
    }

    SECTION("argument helper: callable + closure")
    {
        std::shared_ptr<lox::Callable> callable;
        lox.in_global_scope()->define_native_function
        (
            "set_fun",
            [&callable](lox::Callable*, lox::ArgumentHelper& ah)
            {
                callable = ah.require_callable();
                ah.complete();
                return lox::make_nil();
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            fun makeCounter()
            {
                var i = 0;
                fun count()
                {
                    i = i + 1;
                    return i;
                }
                return count;
            }
            set_fun(makeCounter());
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out, {}));

        // script should have called `set_fun` and callable should now be set
        REQUIRE(callable != nullptr);

        auto fir_val = callable->call({{}});
        auto sec_val = callable->call({{}});
        
        auto fir = lox::as_number(fir_val);
        auto sec = lox::as_number(sec_val);
        
        INFO(fir_val->to_string());
        REQUIRE(fir);
        REQUIRE(sec);

        CHECK(*fir == Approx(1.0f));
        CHECK(*sec == Approx(2.0f));
    }

    SECTION("native class default constructor")
    {
        struct Adder
        {
            std::string value;
        };
        lox.in_global_scope()->define_native_class<Adder>("Adder")
            .add_function
            (
                "get", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    arguments.complete();
                    return lox::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string();
                    arguments.complete();
                    c.value += new_value;
                    return lox::make_nil();
                }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var adder = new Adder();
                adder.add("dog");
                print adder.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("pass function as callback")
        {
            const auto run_ok = lox.run_string
            (R"lox(

                fun callback(func)
                {
                    func("cats");
                }

                var adder = new Adder();
                callback(adder.add);
                print adder.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cats"}));
        }
    }

    SECTION("native class custom constructor")
    {
        struct Adder
        {
            std::string value;
        };
        lox.in_global_scope()->define_native_class<Adder>
            (
                "Adder",
                [](lox::ArgumentHelper& args)
                {
                    const auto initial = args.require_string();
                    args.complete();
                    Adder a;
                    a.value = initial;
                    return a;
                }
            )
            .add_function
            (
                "get", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    arguments.complete();
                    return lox::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string();
                    arguments.complete();
                    c.value += new_value;
                    return lox::make_nil();
                }
            )
            .add_property<std::string>
            (
                "value",
                [](Adder& c) { return c.value; },
                [](Adder& c, const std::string& new_value) { c.value = new_value; }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var adder = new Adder("good ");
                adder.add("dog?");
                print adder.get();
                adder.value = "yes";
                adder.add("!");
                print adder.value;
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"good dog?", "yes!"}));
        }

        SECTION("pass function as callback")
        {
            const auto run_ok = lox.run_string
            (R"lox(

                fun callback(func)
                {
                    func("cats");
                }

                var adder = new Adder("cute ");
                callback(adder.add);
                print adder.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cute cats"}));
        }
    }
}
