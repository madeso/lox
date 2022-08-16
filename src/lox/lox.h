#pragma once

#include "lox/object.h"

namespace lox
{


struct Lox;


namespace detail
{
    std::size_t create_type_id();

    template<typename T>
    std::size_t get_unique_id()
    {
        static const std::size_t id = create_type_id();
        return id;
    }

    template<typename T>
    struct NativeInstanceT : NativeInstance
    {
        T data;

        NativeInstanceT(T&& t, std::shared_ptr<NativeKlass> nk)
            : NativeInstance(nk)
            , data(t)
        {
        }
    };

    template<typename T>
    std::shared_ptr<NativeInstance>
    make_native_instance(std::shared_ptr<NativeKlass> k, T&& t)
    {
        return std::make_shared<NativeInstanceT<T>>(std::move(t), k);
    }

    template<typename T> T get_from_obj_or_error(std::shared_ptr<Object>);
    template<> std::string get_from_obj_or_error<std::string>(std::shared_ptr<Object> obj);
    template<> bool get_from_obj_or_error<bool>(std::shared_ptr<Object> obj);
    template<> float get_from_obj_or_error<float>(std::shared_ptr<Object> obj);


    template<typename T> std::shared_ptr<Object> make_object(T);
    template<> std::shared_ptr<Object> make_object<std::string>(std::string str);
    template<> std::shared_ptr<Object> make_object<bool>(bool b);
    template<> std::shared_ptr<Object> make_object<float>(float n);

    template<typename T, typename P>
    struct PropertyT : Property
    {
        std::function<P(T&)> getter;
        std::function<void (T&, P)> setter;

        PropertyT
        (
            std::function<P(T&)>&& g,
            std::function<void (T&, P)>&& s
        )
            : getter(g)
            , setter(s)
        {
        }

        std::shared_ptr<Object> get_value(NativeInstance* instance) override
        {
            assert(static_cast<NativeKlass*>(instance->klass.get())->native_id == get_unique_id<T>());
            auto& in = static_cast<NativeInstanceT<T>&>(*instance);
            return make_object<P>(getter(in.data));
        }

        void set_value(NativeInstance* instance, std::shared_ptr<Object> value) override
        {
            assert(static_cast<NativeKlass*>(instance->klass.get())->native_id == get_unique_id<T>());
            auto& in = static_cast<NativeInstanceT<T>&>(*instance);
            setter(in.data, get_from_obj_or_error<P>(value));
        }

    };
}


struct Interpreter;
struct ErrorHandler;


void
define_native_function
(
    const std::string& name,
    Environment& env,
    std::function<std::shared_ptr<Object>(Callable*, const Arguments& arguments)>&& func
);



struct ArgumentHelper
{
    const lox::Arguments& args;
    u64 next_argument;
    bool has_read_all_arguments;

    explicit ArgumentHelper(const lox::Arguments& args);
    ~ArgumentHelper();

    // todo(Gustav): add some match/switch helper to handle overloads

    std::string                 require_string   ();
    bool                        require_bool     ();
    float                       require_number   ();
    std::shared_ptr<Callable>   require_callable ();

    void complete();

    ArgumentHelper(ArgumentHelper&&) = delete;
    ArgumentHelper(const ArgumentHelper&) = delete;
    void operator=(ArgumentHelper&&) = delete;
    void operator=(const ArgumentHelper&) = delete;
};

template<typename T>
struct ClassAdder
{
    std::shared_ptr<NativeKlass> native_klass;
    
    ClassAdder<T>&
    add_function(const std::string& name, std::function<std::shared_ptr<Object>(T& t, ArgumentHelper& arguments)> func)
    {
        std::shared_ptr<Callable> native_func = make_native_function
        (
            name,
            [func](Callable* callable, const Arguments& args) -> std::shared_ptr<Object>
            {
                const auto cc = callable->to_string();
                assert(callable->is_bound());
                BoundCallable* bound = static_cast<BoundCallable*>(callable);
                assert(bound->bound != nullptr);
                assert(bound->bound->get_type() == ObjectType::native_instance);
                std::shared_ptr<NativeInstance> native_instance = std::static_pointer_cast<NativeInstance>(bound->bound);
                assert(native_instance != nullptr);
                // todo(Gustav): uncoment this!
                // assert(native_instance->native_id == detail::get_unique_id<T>());
                auto specific_type = std::static_pointer_cast<detail::NativeInstanceT<T>>(native_instance);
                ArgumentHelper helper{args};
                return func(specific_type->data, helper);
            }
        );
        [[maybe_unused]] const auto was_added = native_klass->add_method_or_false(name, native_func);
        assert(was_added);
        return *this;
    }

    template<typename P>
    ClassAdder<T>&
    add_property(const std::string& name, std::function<P(T&)> getter, std::function<void (T&, P)> setter)
    {
        auto prop = std::make_unique<detail::PropertyT<T, P>>(std::move(getter), std::move(setter));
        native_klass->add_property(name, std::move(prop));
        return *this;
    }
};

struct Lox
{
    Lox(std::unique_ptr<ErrorHandler> error_handler, std::function<void (const std::string&)> on_line);
    ~Lox();

    bool run_string(const std::string& source);

    void
    define_global_native_function
    (
        const std::string& name,
        std::function<std::shared_ptr<Object>(Callable*, const Arguments& arguments)>&& func
    );

    std::shared_ptr<NativeKlass>
    register_native_klass
    (
        const std::string& name,
        std::size_t id,
        std::function<std::shared_ptr<Object> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)>&& c
    );

    template<typename T>
    ClassAdder<T>
    define_global_native_class(const std::string& name)
    {
        auto native_klass = register_native_klass
        (
            name,
            detail::get_unique_id<T>(),
            [](std::shared_ptr<NativeKlass> klass, ArgumentHelper& args)
            {
                args.complete();
                return detail::make_native_instance<T>(klass, T{});
            }
        );
        return ClassAdder<T>{native_klass};
    }

    template<typename T>
    ClassAdder<T>
    define_global_native_class(const std::string& name, std::function<T (ArgumentHelper&)> constructor)
    {
        auto native_klass = register_native_klass
        (
            name,
            detail::get_unique_id<T>(),
            [constructor](std::shared_ptr<NativeKlass> klass, ArgumentHelper& args)
            {
                return detail::make_native_instance<T>(klass, constructor(args));
            }
        );
        return ClassAdder<T>{native_klass};
    }

    Environment& get_global_environment();

    std::unique_ptr<ErrorHandler> error_handler;
    std::shared_ptr<Interpreter> interpreter;
};


}
