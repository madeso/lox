#include "catch.hpp"
#include "catchy/stringeq.h"

#include "lox/lox.h"
#include "lox/environment.h"

#include "test.h"

using namespace catchy;


TEST_CASE("lox binding" "[lox]")
{
    std::vector<std::string> console_out;
    std::vector<std::string> error_list;

    auto lox = lox::Lox{std::make_unique<AddStringErrors>(&error_list), [&](const std::string& s){ console_out.emplace_back(s); }};


    SECTION("lox -> cpp binding")
    {
        lox.define_global_native_function
        (
            "nat",
            [](const lox::Arguments& args)
            {
                lox::ArgumentHelper{args}.complete();
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
        
        auto res = call(callable, { {lox::make_string("world")} });
        auto str = lox::as_string(res);
        
        REQUIRE(str);
        REQUIRE(*str == "goodbye cruel world");
    }

    SECTION("argument helper: numbers")
    {
        lox.define_global_native_function
        (
            "add",
            [](const lox::Arguments& args)
            {
                auto ah = lox::ArgumentHelper{args};
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
        lox.define_global_native_function
        (
            "add",
            [](const lox::Arguments& args)
            {
                auto ah = lox::ArgumentHelper{args};
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
        lox.define_global_native_function
        (
            "add",
            [](const lox::Arguments& args)
            {
                auto ah = lox::ArgumentHelper{args};
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
        lox.define_global_native_function
        (
            "set_fun",
            [&callable](const lox::Arguments& args)
            {
                auto ah = lox::ArgumentHelper{args};
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

        auto fir_val = call(callable, {{}});
        auto sec_val = call(callable, {{}});
        
        auto fir = lox::as_number(fir_val);
        auto sec = lox::as_number(sec_val);
        
        INFO(fir_val->to_string());
        REQUIRE(fir);
        REQUIRE(sec);

        CHECK(*fir == Approx(1.0f));
        CHECK(*sec == Approx(2.0f));
    }
}
