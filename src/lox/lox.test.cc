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
                auto arg = helper.require_string("arg");
                if(helper.complete()) return lox::make_nil();
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
                {error, 20, 22, {"Expected 1 arguments but got 0"}},
                {note, 17, 20, {"called with 0 arguments"}}
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
                {error, 20, 24, {"42 (int) is not accepted for argument 1, expected string"}},
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
                {error, 64, 74, {"24 (int) is not accepted for property 'value', expected string, on <native instance Adder>"}},
            }));
            CHECK(StringEq(console_out, {}));
        }
    }

    SECTION("native class: free functions")
    {
        struct Adder { };
        struct Subber {};
        lox.in_global_scope()->define_native_class<Adder>("Adder");
        lox.in_global_scope()->define_native_class<Subber>("Subber");

        lox.in_global_scope()->define_native_function
            (
                "add", [](lox::Callable*, lox::ArgumentHelper& ah)
                {
                    auto adder = ah.require_native<Subber>("adder");
                    if(ah.complete()) return lox::make_nil();
                    return lox::make_nil();
                }
            );

        SECTION("take wrong argument")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var sub = new Adder();
                add(sub);
            )lox");
            CHECK_FALSE(run_ok);
            CHECK(ErrorEq(error_list, {
                {error, 59, 64, {"<native instance Adder> (Adder) is not accepted for argument 1, expected Subber"}},
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
                if(helper.complete()) return lox::make_nil();
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
        
        auto res = callable->call(lox.get_interpreter(), {{lox::make_string("world")} });
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
                auto lhs = ah.require_int("lhs");
                auto rhs = ah.require_int("rhs");
                if(ah.complete()) return lox::make_nil();
                return lox::make_number_int(lhs + rhs);
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
                auto lhs = ah.require_string("lhs");
                auto rhs = ah.require_string("rhs");
                if(ah.complete()) return lox::make_nil();
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
                auto lhs = ah.require_bool("bool");
                if(ah.complete()) return lox::make_nil();
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
                callable = ah.require_callable("callable");
                if(ah.complete()) return lox::make_nil();
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

        auto fir_val = callable->call(lox.get_interpreter(), {{}});
        auto sec_val = callable->call(lox.get_interpreter(), {{}});
        
        auto fir = lox::as_int(fir_val);
        auto sec = lox::as_int(sec_val);
        
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
                    if(arguments.complete()) return lox::make_nil();
                    return lox::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lox::make_nil();
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
                    const auto initial = args.require_string("initial");
                    if(args.complete()) return Adder{};
                    Adder a;
                    a.value = initial;
                    return a;
                }
            )
            .add_function
            (
                "get", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lox::make_nil();
                    return lox::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lox::make_nil();
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


    SECTION("native class: free functions")
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
                    const auto initial = args.require_string("initial");
                    if(args.complete()) return Adder{};
                    Adder a;
                    a.value = initial;
                    return a;
                }
            )
            .add_function
            (
                "get", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lox::make_nil();
                    return lox::make_string(c.value);
                }
            );

        SECTION("class -> string")
        {
            lox.in_global_scope()->define_native_function
            (
                "test", [](lox::Callable*, lox::ArgumentHelper& ah)
                {
                    auto adder = ah.require_native<Adder>("adder");
                    if(ah.complete()) return lox::make_nil();
                    return lox::make_string(adder->value);
                }
            );
            const auto run_ok = lox.run_string
            (R"lox(
                var a = new Adder("abc");
                print test(a);
                print a.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"abc", "abc"}));
        }

        SECTION("string -> class")
        {
            lox.in_global_scope()->define_native_function
            (
                "test", [&lox](lox::Callable*, lox::ArgumentHelper& ah)
                {
                    auto initial = ah.require_string("initial");
                    if(ah.complete()) return lox::make_nil();
                    Adder a;
                    a.value = initial;
                    return lox.make_native(a);
                }
            );
            const auto run_ok = lox.run_string
            (R"lox(
                var a = test("doggy dog");
                print a.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"doggy dog"}));
        }
    }


    SECTION("take instance and call member function")
    {
        std::shared_ptr<lox::Instance> object;
        lox.in_global_scope()->define_native_function
        (
            "set_foo", [&object](lox::Callable*, lox::ArgumentHelper& ah)
            {
                auto initial = ah.require_instance("initial");
                if(ah.complete()) return lox::make_nil();
                object = initial;
                return lox::make_nil();
            }
        );
        const auto run_ok = lox.run_string
        (R"lox(
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
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{}));
        REQUIRE(object != nullptr);

        auto callable = object->get_bound_method_or_null("hello");
        REQUIRE(callable != nullptr);
        
        auto res = callable->call(lox.get_interpreter(), {{lox::make_string("world")} });
        auto str = lox::as_string(res);
        
        REQUIRE(str);
        REQUIRE(*str == "hello world");
    }


    SECTION("single package: argument helper: numbers")
    {
        lox.in_package("pkg")->define_native_function
        (
            "add",
            [](lox::Callable*, lox::ArgumentHelper& ah)
            {
                auto lhs = ah.require_int("lhs");
                auto rhs = ah.require_int("rhs");
                if(ah.complete()) return lox::make_nil();
                return lox::make_number_int(lhs + rhs);
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            print pkg.add(40, 2);
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "42"
        }));
    }

    SECTION("nested package: argument helper: numbers")
    {
        lox.in_package("with.some.pkg")->define_native_function
        (
            "add",
            [](lox::Callable*, lox::ArgumentHelper& ah)
            {
                auto lhs = ah.require_int("lhs");
                auto rhs = ah.require_int("rhs");
                if(ah.complete()) return lox::make_nil();
                return lox::make_number_int(lhs + rhs);
            }
        );

        const auto run_ok = lox.run_string
        (R"lox(
            print with.some.pkg.add(40, 2);
        )lox");
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
        lox.in_package("pkg")->define_native_class<Adder>("Adder")
            .add_function
            (
                "get", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lox::make_nil();
                    return lox::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lox::make_nil();
                    c.value += new_value;
                    return lox::make_nil();
                }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var adder = new pkg.Adder();
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

                var adder = new pkg.Adder();
                callback(adder.add);
                print adder.get();
            )lox");
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
        lox.in_package("with.some.pkg")->define_native_class<Adder>("Adder")
            .add_function
            (
                "get", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lox::make_nil();
                    return lox::make_string(c.value);
                }
            )
            .add_function
            (
                "add", [](Adder& c, lox::ArgumentHelper& arguments)
                {
                    const auto new_value = arguments.require_string("new_value");
                    if(arguments.complete()) return lox::make_nil();
                    c.value += new_value;
                    return lox::make_nil();
                }
            )
            ;

        SECTION("call function")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var adder = new with.some.pkg.Adder();
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

                var adder = new with.some.pkg.Adder();
                callback(adder.add);
                print adder.get();
            )lox");
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
        lox.in_global_scope()->define_native_class<Base>("Base")
            .add_function
            (
                "move", [](Base& b, lox::ArgumentHelper& arguments)
                {
                    const auto delta = arguments.require_string("delta");
                    if(arguments.complete()) return lox::make_nil();

                    b.move(delta);

                    return lox::make_nil();
                }
            )
            .add_function
            (
                "get", [](Base& b, lox::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lox::make_nil();

                    return lox::make_string(b.movement);
                }
            )
            ;

        SECTION("binding works")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                var a = new Base();
                a.move("dog");
                print a.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("basic derive from base")
        {
            const auto run_ok = lox.run_string
            (R"lox(
                class Derived : Base
                {
                }
                var a = new Derived();
                a.move("dog");
                print a.get();
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dog"}));
        }

        SECTION("derive from base with super call")
        {
            const auto run_ok = lox.run_string
            (R"lox(
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
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"dogdog"}));
        }

        SECTION("derive from base call super with this")
        {
            const auto run_ok = lox.run_string
            (R"lox(
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
            )lox");
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
        lox.in_global_scope()->define_native_class<Base>
            (
                "Base", [](lox::ArgumentHelper& ah) -> Base
                {
                    const auto start = ah.require_string("start");
                    if(ah.complete()) return Base{""};
                    return Base{start};
                }
            )
            .add_function
            (
                "move", [](Base& b, lox::ArgumentHelper& arguments)
                {
                    const auto delta = arguments.require_string("delta");
                    if(arguments.complete()) return lox::make_nil();

                    b.move(delta);

                    return lox::make_nil();
                }
            )
            .add_function
            (
                "get", [](Base& b, lox::ArgumentHelper& arguments)
                {
                    if(arguments.complete()) return lox::make_nil();

                    return lox::make_string(b.movement);
                }
            )
            ;
        lox::NativeRef<Base> base;
        lox.in_global_scope()->define_native_function("cpp", [&](lox::Callable*, lox::ArgumentHelper& ah) -> std::shared_ptr<lox::Object>
        {
            auto inst = ah.require_instance("instance");
            auto passed_base = lox::get_derived<Base>(inst);
            if(ah.complete()) return lox::make_nil();

            base = passed_base;
            return lox::make_nil();
        });

        SECTION("binding works")
        {
            CHECK(base == nullptr);
            const auto run_ok = lox.run_string
            (R"lox(
                var a = new Base("cat and ");
                a.move("dog");
                print a.get();
                cpp(a);
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cat and dog"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "cat and dog");
        }

        SECTION("basic derive from base")
        {
            CHECK(base == nullptr);
            const auto run_ok = lox.run_string
            (R"lox(
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
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"doggy dog"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "doggy dog");
        }

        SECTION("derive from base with super call")
        {
            CHECK(base == nullptr);
            const auto run_ok = lox.run_string
            (R"lox(
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
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"doggy dogdog"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "doggy dogdog");
        }

        SECTION("derive from base call super with this")
        {
            CHECK(base == nullptr);
            const auto run_ok = lox.run_string
            (R"lox(
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
            )lox");
            CHECK(run_ok);
            REQUIRE(StringEq(error_list, {}));
            CHECK(StringEq(console_out, {"cats"}));
            REQUIRE(base != nullptr);
            CHECK(base->movement == "cats");
        }
    }

    SECTION("package - constants")
    {
        lox.in_package("defs.ints")
            ->add_native_getter("one", []() { return lox::make_number_int(1); })
            .add_native_getter("life", []() { return lox::make_number_int(42); })
        ;

        const auto run_ok = lox.run_string
        (R"lox(
            print defs.ints.one;
            print defs.ints.life;
        )lox");
        CHECK(run_ok);
        REQUIRE(StringEq(error_list, {}));
        CHECK(StringEq(console_out,{
            "1", "42"
        }));
    }
}
