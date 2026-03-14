#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "catchy/stringeq.h"

#include "lax/lax.h"
#include "lax/environment.h"

#include "test.h"

using namespace catchy;


TEST_CASE("lax binding fail" "[lax]")
{
    std::vector<std::string> console_out;
    std::vector<ReportedError> error_list;

    auto lax = lax::Lax{std::make_unique<AddErrorErrors>(&error_list), [&](const std::string& s){ console_out.emplace_back(s); }};

    constexpr ReportedError::Type error = ReportedError::Type::error;
    constexpr ReportedError::Type note = ReportedError::Type::note;

    SECTION("function 1 string arg")
    {
        lax.in_global_scope()->define_native_function
        (
            "nat",
            [](lax::Callable*, lax::ArgumentHelper& helper)
            {
                auto arg = helper.require_string("arg");
                if(helper.complete()) return lax::make_nil();
                return lax::make_string(arg);
            }
        );

        SECTION("send 0 arguments")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                nat();
            )lax");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 20, 22, {"Expected 1 arguments but got 0"}},
                {note, 17, 20, {"called with 0 arguments"}}
            }));
            CHECK(StringEq(console_out,{}));
        }

        SECTION("send wrong type")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                nat(42);
            )lax");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 20, 24, {"42 (int) is not accepted for argument 1, expected string"}},
            }));
            CHECK(StringEq(console_out,{}));
        }

        SECTION("send too many arguments")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                nat("hello", "world");
            )lax");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 20, 38, {"Expected 1 arguments but got 2"}},
                {note, 17, 20, {"called with 2 arguments"}},
                {note, 21, 28, {"argument 1 evaluated to string: \"hello\""}},
                {note, 30, 37, {"argument 2 evaluated to string: \"world\""}},
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
        lax.in_global_scope()->define_native_class<Adder>("Adder")
            .add_property<std::string>
            (
                "value",
                [](Adder& c) { return c.value; },
                [](Adder& c, const std::string& new_value) { c.value = new_value; }
            )
            ;

        SECTION("set property invalid type")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                var adder = new Adder();
                adder.value = 24;
            )lax");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 64, 74, {"24 (int) is not accepted for property 'value', expected string, on <native instance Adder>"}},
            }));
            CHECK(StringEq(console_out, {}));
        }
    }

    SECTION("native class: free functions")
    {
        struct Adder { };
        struct Subber {};
        lax.in_global_scope()->define_native_class<Adder>("Adder");
        lax.in_global_scope()->define_native_class<Subber>("Subber");

        lax.in_global_scope()->define_native_function
            (
                "add", [](lax::Callable*, lax::ArgumentHelper& ah)
                {
                    auto adder = ah.require_native<Subber>("adder");
                    if(ah.complete()) return lax::make_nil();
                    return lax::make_nil();
                }
            );

        SECTION("take wrong argument")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                var sub = new Adder();
                add(sub);
            )lax");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 59, 64, {"<native instance Adder> (Adder) is not accepted for argument 1, expected Subber"}},
            }));
            CHECK(StringEq(console_out, {}));
        }
    }
}


TEST_CASE("lax binding" "[lax]")
{
    std::vector<std::string> console_out;
    std::vector<std::string> error_list;

    auto lax = lax::Lax{std::make_unique<AddStringErrors>(&error_list), [&](const std::string& s){ console_out.emplace_back(s); }};


    SECTION("lax -> cpp binding")
    {
        lax.in_global_scope()->define_native_function
        (
            "nat",
            [](lax::Callable*, lax::ArgumentHelper& helper)
            {
                if(helper.complete()) return lax::make_nil();
                return lax::make_string("hello world");
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
            print nat;
            print nat();
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "<native fun nat ([])>",
            "hello world"
        }));
    }

    SECTION("cpp -> lax binding")
    {
        const auto run_ok = lax.run_string
        (R"lax(
            fun hello(name)
            {
                return "goodbye cruel " + name;
            }
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{}));

        auto function_object = lax.get_global_environment().get_or_null("hello");
        REQUIRE(function_object != nullptr);

        auto callable = lax::as_callable(function_object);
        REQUIRE(callable != nullptr);
        
        auto res = callable->call(lax.get_interpreter(), {{lax::make_string("world")} });
        auto str = lax::as_string(res);
        
        REQUIRE(str);
        REQUIRE(*str == "goodbye cruel world");
    }

    SECTION("argument helper: numbers")
    {
        lax.in_global_scope()->define_native_function
        (
            "add",
            [](lax::Callable*, lax::ArgumentHelper& ah)
            {
                auto lhs = ah.require_int("lhs");
                auto rhs = ah.require_int("rhs");
                if(ah.complete()) return lax::make_nil();
                return lax::make_number_int(lhs + rhs);
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
            print add(40, 2);
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }

    SECTION("argument helper: strings")
    {
        lax.in_global_scope()->define_native_function
        (
            "add",
            [](lax::Callable*, lax::ArgumentHelper& ah)
            {
                auto lhs = ah.require_string("lhs");
                auto rhs = ah.require_string("rhs");
                if(ah.complete()) return lax::make_nil();
                return lax::make_bool(lhs < rhs);
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
            print add("abc", "xyz");
            print add("xyz", "abc");
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "true", "false"
        }));
    }

    SECTION("argument helper: boolean")
    {
        lax.in_global_scope()->define_native_function
        (
            "add",
            [](lax::Callable*, lax::ArgumentHelper& ah)
            {
                auto lhs = ah.require_bool("bool");
                if(ah.complete()) return lax::make_nil();
                if(lhs) return lax::make_string("yes!");
                else    return lax::make_string("or no?");
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
            print add(true);
            print add(false);
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "yes!", "or no?"
        }));
    }

    SECTION("argument helper: callable + closure")
    {
        std::shared_ptr<lax::Callable> callable;
        lax.in_global_scope()->define_native_function
        (
            "set_fun",
            [&callable](lax::Callable*, lax::ArgumentHelper& ah)
            {
                callable = ah.require_callable("callable");
                if(ah.complete()) return lax::make_nil();
                return lax::make_nil();
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
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
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out, {}));

        // script should have called `set_fun` and callable should now be set
        REQUIRE(callable != nullptr);

        auto fir_val = callable->call(lax.get_interpreter(), {{}});
        auto sec_val = callable->call(lax.get_interpreter(), {{}});
        
        auto fir = lax::as_int(fir_val);
        auto sec = lax::as_int(sec_val);
        
        REQUIRE(fir);
        REQUIRE(sec);

        CHECK(*fir == Catch::Approx(1.0f));
        CHECK(*sec == Catch::Approx(2.0f));
    }

    SECTION("native class default constructor")
    {
        struct Adder
        {
            std::string value;
        };
        lax.in_global_scope()->define_native_class<Adder>("Adder")
            .add_function
            (
                "get", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();
                    return lax::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lax::make_nil();
                    c.value += new_value;
                    return lax::make_nil();
                }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                var adder = new Adder();
                adder.add("dog");
                print adder.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("pass function as callback")
        {
            const auto run_ok = lax.run_string
            (R"lax(

                fun callback(func)
                {
                    func("cats");
                }

                var adder = new Adder();
                callback(adder.add);
                print adder.get();
            )lax");
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
        lax.in_global_scope()->define_native_class<Adder>
            (
                "Adder",
                [](lax::ArgumentHelper& args)
                {
                    const auto initial = args.require_string("initial");
                    if(args.complete()) return Adder{};
                    Adder a;
                    a.value = initial;
                    return a;
                }
            )
            .add_function
            (
                "get", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();
                    return lax::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lax::make_nil();
                    c.value += new_value;
                    return lax::make_nil();
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
            const auto run_ok = lax.run_string
            (R"lax(
                var adder = new Adder("good ");
                adder.add("dog?");
                print adder.get();
                adder.value = "yes";
                adder.add("!");
                print adder.value;
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"good dog?", "yes!"}));
        }

        SECTION("pass function as callback")
        {
            const auto run_ok = lax.run_string
            (R"lax(

                fun callback(func)
                {
                    func("cats");
                }

                var adder = new Adder("cute ");
                callback(adder.add);
                print adder.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cute cats"}));
        }
    }


    SECTION("native class: free functions")
    {
        struct Adder
        {
            std::string value;
        };
        lax.in_global_scope()->define_native_class<Adder>
            (
                "Adder",
                [](lax::ArgumentHelper& args)
                {
                    const auto initial = args.require_string("initial");
                    if(args.complete()) return Adder{};
                    Adder a;
                    a.value = initial;
                    return a;
                }
            )
            .add_function
            (
                "get", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();
                    return lax::make_string(c.value);
                }
            );

        SECTION("class -> string")
        {
            lax.in_global_scope()->define_native_function
            (
                "test", [](lax::Callable*, lax::ArgumentHelper& ah)
                {
                    auto adder = ah.require_native<Adder>("adder");
                    if(ah.complete()) return lax::make_nil();
                    return lax::make_string(adder->value);
                }
            );
            const auto run_ok = lax.run_string
            (R"lax(
                var a = new Adder("abc");
                print test(a);
                print a.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"abc", "abc"}));
        }

        SECTION("string -> class")
        {
            lax.in_global_scope()->define_native_function
            (
                "test", [&lax](lax::Callable*, lax::ArgumentHelper& ah)
                {
                    auto initial = ah.require_string("initial");
                    if(ah.complete()) return lax::make_nil();
                    Adder a;
                    a.value = initial;
                    return lax.make_native(a);
                }
            );
            const auto run_ok = lax.run_string
            (R"lax(
                var a = test("doggy dog");
                print a.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"doggy dog"}));
        }
    }


    SECTION("take instance and call member function")
    {
        std::shared_ptr<lax::Instance> object;
        lax.in_global_scope()->define_native_function
        (
            "set_foo", [&object](lax::Callable*, lax::ArgumentHelper& ah)
            {
                auto initial = ah.require_instance("initial");
                if(ah.complete()) return lax::make_nil();
                object = initial;
                return lax::make_nil();
            }
        );
        const auto run_ok = lax.run_string
        (R"lax(
            class Bar
            {
                var greeting;
                fun init(g)
                {
                    this.greeting = g;
                }
                fun hello(name)
                {
                    return this.greeting + name;
                }
            }

            set_foo(new Bar("hello "));
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{}));
        REQUIRE(object != nullptr);

        auto callable = object->get_bound_method_or_null("hello");
        REQUIRE(callable != nullptr);
        
        auto res = callable->call(lax.get_interpreter(), {{lax::make_string("world")} });
        auto str = lax::as_string(res);
        
        REQUIRE(str);
        REQUIRE(*str == "hello world");
    }


    SECTION("single package: argument helper: numbers")
    {
        lax.in_package("pkg")->define_native_function
        (
            "add",
            [](lax::Callable*, lax::ArgumentHelper& ah)
            {
                auto lhs = ah.require_int("lhs");
                auto rhs = ah.require_int("rhs");
                if(ah.complete()) return lax::make_nil();
                return lax::make_number_int(lhs + rhs);
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
            print pkg.add(40, 2);
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }

    SECTION("nested package: argument helper: numbers")
    {
        lax.in_package("with.some.pkg")->define_native_function
        (
            "add",
            [](lax::Callable*, lax::ArgumentHelper& ah)
            {
                auto lhs = ah.require_int("lhs");
                auto rhs = ah.require_int("rhs");
                if(ah.complete()) return lax::make_nil();
                return lax::make_number_int(lhs + rhs);
            }
        );

        const auto run_ok = lax.run_string
        (R"lax(
            print with.some.pkg.add(40, 2);
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }

    SECTION("single package: native class default constructor")
    {
        struct Adder
        {
            std::string value;
        };
        lax.in_package("pkg")->define_native_class<Adder>("Adder")
            .add_function
            (
                "get", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();
                    return lax::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lax::make_nil();
                    c.value += new_value;
                    return lax::make_nil();
                }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                var adder = new pkg.Adder();
                adder.add("dog");
                print adder.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("pass function as callback")
        {
            const auto run_ok = lax.run_string
            (R"lax(

                fun callback(func)
                {
                    func("cats");
                }

                var adder = new pkg.Adder();
                callback(adder.add);
                print adder.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cats"}));
        }
    }

    SECTION("nested package: native class default constructor")
    {
        struct Adder
        {
            std::string value;
        };
        lax.in_package("with.some.pkg")->define_native_class<Adder>("Adder")
            .add_function
            (
                "get", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();
                    return lax::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lax::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lax::make_nil();
                    c.value += new_value;
                    return lax::make_nil();
                }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                var adder = new with.some.pkg.Adder();
                adder.add("dog");
                print adder.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("pass function as callback")
        {
            const auto run_ok = lax.run_string
            (R"lax(

                fun callback(func)
                {
                    func("cats");
                }

                var adder = new with.some.pkg.Adder();
                callback(adder.add);
                print adder.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cats"}));
        }
    }

    SECTION("native class (default ctor) -> scipt class")
    {
        struct Base
        {
            std::string movement;
            void move(const std::string& delta)
            {
                movement += delta;
            }
        };
        lax.in_global_scope()->define_native_class<Base>("Base")
            .add_function
            (
                "move", [](Base& b, lax::ArgumentHelper& arguments)
                {
                    const auto delta = arguments.require_string("delta");
                    if(arguments.complete()) return lax::make_nil();

                    b.move(delta);

                    return lax::make_nil();
                }
            )
            .add_function
            (
                "get", [](Base& b, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();

                    return lax::make_string(b.movement);
                }
            )
            ;

        SECTION("binding works")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                var a = new Base();
                a.move("dog");
                print a.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("basic derive from base")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                class Derived : Base
                {
                }
                var a = new Derived();
                a.move("dog");
                print a.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("derive from base with super call")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                class Derived : Base
                {
                    fun move(what)
                    {
                        super.move(what + what);
                    }
                }
                var a = new Derived();
                a.move("dog");
                print a.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dogdog"}));
        }

        SECTION("derive from base call super with this")
        {
            const auto run_ok = lax.run_string
            (R"lax(
                class Derived : Base
                {
                    fun pet()
                    {
                        this.move("cats");
                    }
                }
                var a = new Derived();
                a.pet();
                print a.get();
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cats"}));
        }
    }


    SECTION("native class (custom ctor) -> scipt class")
    {
        struct Base
        {
            std::string movement;

            explicit Base(const std::string& s)
                 : movement(s)
            {
            }

            void move(const std::string& delta)
            {
                movement += delta;
            }
        };
        lax.in_global_scope()->define_native_class<Base>
            (
                "Base", [](lax::ArgumentHelper& ah) -> Base
                {
                    const auto start = ah.require_string("start");
                    if(ah.complete()) return Base{""};
                    return Base{start};
                }
            )
            .add_function
            (
                "move", [](Base& b, lax::ArgumentHelper& arguments)
                {
                    const auto delta = arguments.require_string("delta");
                    if(arguments.complete()) return lax::make_nil();

                    b.move(delta);

                    return lax::make_nil();
                }
            )
            .add_function
            (
                "get", [](Base& b, lax::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lax::make_nil();

                    return lax::make_string(b.movement);
                }
            )
            ;
        lax::NativeRef<Base> base;
        lax.in_global_scope()->define_native_function("cpp", [&](lax::Callable*, lax::ArgumentHelper& ah) -> std::shared_ptr<lax::Object>
        {
            auto inst = ah.require_instance("instance");
            auto passed_base = lax::get_derived<Base>(inst);
            if(ah.complete()) return lax::make_nil();

            base = passed_base;
            return lax::make_nil();
        });

        SECTION("binding works")
        {
            CHECK(base == nullptr);
            const auto run_ok = lax.run_string
            (R"lax(
                var a = new Base("cat and ");
                a.move("dog");
                print a.get();
                cpp(a);
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cat and dog"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "cat and dog");
        }

        SECTION("basic derive from base")
        {
            CHECK(base == nullptr);
            const auto run_ok = lax.run_string
            (R"lax(
                class Derived : Base
                {
                    fun init()
                    {
                        super("doggy ");
                    }
                }
                var a = new Derived();
                a.move("dog");
                print a.get();
                cpp(a);
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"doggy dog"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "doggy dog");
        }

        SECTION("derive from base with super call")
        {
            CHECK(base == nullptr);
            const auto run_ok = lax.run_string
            (R"lax(
                class Derived : Base
                {
                    fun init(x)
                    {
                        super(x);
                    }
                    fun move(what)
                    {
                        super.move(what + what);
                    }
                }
                var a = new Derived("doggy ");
                a.move("dog");
                print a.get();
                cpp(a);
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"doggy dogdog"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "doggy dogdog");
        }

        SECTION("derive from base call super with this")
        {
            CHECK(base == nullptr);
            const auto run_ok = lax.run_string
            (R"lax(
                class Derived : Base
                {
                    fun init()
                    {
                        super("");
                    }
                    fun pet()
                    {
                        this.move("cats");
                    }
                }
                var a = new Derived();
                a.pet();
                print a.get();
                cpp(a);
            )lax");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cats"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "cats");
        }
    }

    SECTION("package - constants")
    {
        lax.in_package("defs.ints")
            ->add_native_getter("one", []() { return lax::make_number_int(1); })
            .add_native_getter("life", []() { return lax::make_number_int(42); })
        ;

        const auto run_ok = lax.run_string
        (R"lax(
            print defs.ints.one;
            print defs.ints.life;
        )lax");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "42"
        }));
    }
}
