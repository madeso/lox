#pragma once

#include <memory>

#include "lox/ints.h"


namespace lox
{

using Ti = i64;
using Tf = double;

enum class ObjectType
{
    nil, string, boolean, array, number_int, number_float, callable, klass, instance, native_instance, native_package
};


struct Environment;
struct ArgumentHelper;
struct Interpreter;
struct Instance;


constexpr std::string_view objecttype_to_string(ObjectType ot)
{
    switch (ot)
    {
    case ObjectType::nil:             return "nil";
    case ObjectType::string:          return "string";
    case ObjectType::boolean:         return "boolean";
    case ObjectType::array:         return "array";
    case ObjectType::number_int:      return "int";
    case ObjectType::number_float:    return "float";
    case ObjectType::klass:           return "class";
    case ObjectType::instance:        return "instance";
    case ObjectType::native_instance: return "native instance";
    case ObjectType::native_package:  return "native package";
    default:                          assert(false && "invalid type"); return "???";
    }
}


// ----------------------------------------------------------------------------

// thrown when there is a native error that needs to be reported back to the script
struct NativeError
{
    std::string message;
};

void raise_error(const std::string& message);

// ----------------------------------------------------------------------------

struct Object;
struct NativeFunction;


struct ToStringOptions
{
    constexpr ToStringOptions() = default;

    std::string_view indent = "    ";
    constexpr ToStringOptions with_indent(std::string_view new_value) const
    {
        ToStringOptions r = *this;
        r.indent = new_value;
        return r;
    }

    std::size_t max_length = 40;
    constexpr ToStringOptions with_max_length(std::size_t new_value) const
    {
        ToStringOptions r = *this;
        r.max_length = new_value;
        return r;
    }

    bool quote_string = true;
    constexpr ToStringOptions with_quote_string(bool new_value) const
    {
        ToStringOptions r = *this;
        r.quote_string = new_value;
        return r;
    }

    constexpr static ToStringOptions for_error()
    {
        return {};
    }
    constexpr static ToStringOptions for_print()
    {
        return ToStringOptions{}.with_quote_string(false);
    }
    constexpr static ToStringOptions for_debug()
    {
        return {};
    }
};

struct Object : std::enable_shared_from_this<Object>
{
    Object() = default;
    virtual ~Object() = default;

    std::string to_flat_string(const ToStringOptions& tso) const;

    virtual ObjectType get_type() const = 0;
    virtual std::vector<std::string> to_string(const ToStringOptions&) const = 0;
    virtual bool is_callable() const = 0;
    
    virtual bool has_properties();
    virtual std::shared_ptr<Object> get_property_or_null(const std::string& name);
    virtual bool set_property_or_false(const std::string& name, std::shared_ptr<Object> value);
    
    virtual bool has_index() const;
    virtual std::shared_ptr<Object> get_index_or_null(std::shared_ptr<Object> index);
    virtual bool set_index_or_false(std::shared_ptr<Object> index, std::shared_ptr<Object> value);
};

using ObjectGenerator = std::function<std::shared_ptr<Object>()>;

struct Arguments
{
    std::vector<std::shared_ptr<Object>> arguments;
};

struct Callable : public Object
{
    virtual std::shared_ptr<Object> call(const Arguments& arguments) = 0;
    ObjectType get_type() const override;
    bool is_callable() const override;

    virtual bool is_bound() const;

    virtual std::shared_ptr<Callable> bind(std::shared_ptr<Object> instance) = 0;
};

struct BoundCallable : Callable
{
    std::shared_ptr<Object> bound;
    std::shared_ptr<NativeFunction> callable;

    BoundCallable(std::shared_ptr<Object> bound, std::shared_ptr<NativeFunction> callable);
    ~BoundCallable();
    std::vector<std::string> to_string(const ToStringOptions&) const override;
    std::shared_ptr<Object> call(const Arguments& arguments) override;
    std::shared_ptr<Callable> bind(std::shared_ptr<Object> instance) override;
    bool is_bound() const override;
};

// ----------------------------------------------------------------------------

struct Klass : Object
{
    std::string klass_name;
    std::shared_ptr<Klass> superklass;
    std::unordered_map<std::string, std::shared_ptr<Callable>> methods;
    std::unordered_map<std::string, std::shared_ptr<Callable>> static_methods;

    Klass(const std::string& n, std::shared_ptr<Klass> sk);
    

    ObjectType
    get_type() const override;

    bool is_callable() const override;

    virtual std::shared_ptr<Instance> constructor(const Arguments& arguments) = 0;

    bool add_method_or_false(const std::string& name, std::shared_ptr<Callable> method);
    std::shared_ptr<Callable> find_method_or_null(const std::string& name);

    bool add_static_method_or_false(const std::string& name, std::shared_ptr<Callable> method);
    bool has_properties() override;
    std::shared_ptr<Object> get_property_or_null(const std::string& name) override;
    bool set_property_or_false(const std::string& name, std::shared_ptr<Object> value) override;
};

// ----------------------------------------------------------------------------


struct Instance : Object
{
    std::shared_ptr<Instance> parent;
    std::shared_ptr<Klass> klass;

    explicit Instance(std::shared_ptr<Klass> o);
    bool is_callable() const override;
    bool has_properties() override;

    //property = method or member variable
    std::shared_ptr<Object> get_property_or_null(const std::string& name) override;
    bool set_property_or_false(const std::string& name, std::shared_ptr<Object> value) override;

    // use this to call member functions on instance
    std::shared_ptr<Callable> get_bound_method_or_null(const std::string& name);

    // member variable only
    virtual std::shared_ptr<Object> get_field_or_null(const std::string& name) = 0;
    virtual bool set_field_or_false(const std::string& name, std::shared_ptr<Object> value) = 0;
};

// ----------------------------------------------------------------------------

struct NativeInstance;

struct Property
{
    virtual ~Property() = default;

    virtual std::shared_ptr<Object> get_value(NativeInstance* instance) = 0;
    virtual bool set_value(NativeInstance* instance, std::shared_ptr<Object> value) = 0;
};

struct NativeKlass : Klass
{
    std::size_t native_id;
    std::unordered_map<std::string, std::unique_ptr<Property>> properties;

    NativeKlass(const std::string& n, std::size_t id, std::shared_ptr<Klass> sk);

    std::vector<std::string> to_string(const ToStringOptions&) const override;

    void add_property(const std::string& name, std::unique_ptr<Property> prop);
};

struct NativeInstance : Instance
{
    NativeInstance(std::shared_ptr<NativeKlass> o);

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;

    std::shared_ptr<Object> get_field_or_null(const std::string& name) override;
    bool set_field_or_false(const std::string& name, std::shared_ptr<Object> value) override;
};


struct ObjectIntegrationImpl;

struct Array : public Object
{
    ObjectIntegrationImpl* integration;
    std::vector<std::shared_ptr<Object>> values;
    
    Array(ObjectIntegrationImpl* i, std::vector<std::shared_ptr<Object>>&& b);
    ~Array() = default;

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;
    bool is_callable() const override;

    std::optional<std::string> to_flat_string_representation(const ToStringOptions&) const;

    bool has_properties() override;
    std::shared_ptr<Object> get_property_or_null(const std::string& name) override;
    bool set_property_or_false(const std::string& name, std::shared_ptr<Object> value) override;

    static std::size_t as_array_index(std::shared_ptr<Object> o);

    bool has_index() const override;
    std::shared_ptr<Object> get_index_or_null(std::shared_ptr<Object> index_object) override;
    bool set_index_or_false(std::shared_ptr<Object> index_object, std::shared_ptr<Object> new_value) override;
};

// ----------------------------------------------------------------------------


std::shared_ptr<Object>     make_nil              ();
std::shared_ptr<Object>     make_string           (const std::string& str);
std::shared_ptr<Object>     make_bool             (bool b);
std::shared_ptr<Object>     make_number_int       (Ti f);
std::shared_ptr<Object>     make_number_float     (Tf f);
std::shared_ptr<Callable>   make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)>&& func
);

struct ObjectIntegration
{
    ObjectIntegration() = default;
    virtual ~ObjectIntegration() = default;

    virtual std::shared_ptr<Object> make_array(std::vector<std::shared_ptr<Object>>&& values) = 0;
};

std::unique_ptr<ObjectIntegration> make_object_integration();

// ----------------------------------------------------------------------------


bool                        is_nil         (std::shared_ptr<Object> o);
std::optional<std::string>  as_string      (std::shared_ptr<Object> o);
std::optional<bool>         as_bool        (std::shared_ptr<Object> o);
std::optional<Ti>           as_int         (std::shared_ptr<Object> o);
std::optional<Tf>           as_float       (std::shared_ptr<Object> o);
std::shared_ptr<Callable>   as_callable    (std::shared_ptr<Object> o);
std::shared_ptr<Array>      as_array       (std::shared_ptr<Object> o);
std::shared_ptr<Instance>   as_instance    (std::shared_ptr<Object> o);
std::shared_ptr<Klass>      as_klass       (std::shared_ptr<Object> o);
std::shared_ptr<NativeInstance> as_native_instance_of_type(std::shared_ptr<Object> o, std::size_t type);



// ----------------------------------------------------------------------------


std::string  get_string_or_ub  (std::shared_ptr<Object> o);
bool         get_bool_or_ub    (std::shared_ptr<Object> o);
Ti           get_int_or_ub     (std::shared_ptr<Object> o);
Tf           get_float_or_ub   (std::shared_ptr<Object> o);


bool is_truthy(std::shared_ptr<Object> o);


// ----------------------------------------------------------------------------


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
    template<> std::shared_ptr<Object> make_object<Tf>(Tf n);
    template<> std::shared_ptr<Object> make_object<Ti>(Ti n);

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

        bool set_value(NativeInstance* instance, std::shared_ptr<Object> value) override
        {
            assert(static_cast<NativeKlass*>(instance->klass.get())->native_id == get_unique_id<T>());
            auto& in = static_cast<NativeInstanceT<T>&>(*instance);
            setter(in.data, get_from_obj_or_error<P>(value));
            return true;
        }
    };

    template<typename T, typename P>
    struct PropertyGetT : Property
    {
        std::function<P(T&)> getter;

        PropertyGetT
        (
            std::function<P(T&)>&& g
        )
            : getter(g)
        {
        }

        std::shared_ptr<Object> get_value(NativeInstance* instance) override
        {
            assert(static_cast<NativeKlass*>(instance->klass.get())->native_id == get_unique_id<T>());
            auto& in = static_cast<NativeInstanceT<T>&>(*instance);
            return make_object<P>(getter(in.data));
        }

        bool set_value(NativeInstance*, std::shared_ptr<Object>) override
        {
            return false;
        }
    };

    template<typename T, typename P>
    struct PropertyNativeGetT : Property
    {
        std::function<std::shared_ptr<Object>(T&)> getter;

        PropertyNativeGetT
        (
            std::function<std::shared_ptr<Object>(T&)>&& g
        )
            : getter(g)
        {
        }

        std::shared_ptr<Object> get_value(NativeInstance* instance) override
        {
            assert(static_cast<NativeKlass*>(instance->klass.get())->native_id == get_unique_id<T>());
            auto& in = static_cast<NativeInstanceT<T>&>(*instance);
            return getter(in.data);
        }

        bool set_value(NativeInstance*, std::shared_ptr<Object>) override
        {
            return false;
        }
    };

    struct PropertyGet : Property
    {
        std::function<std::shared_ptr<Object>()> getter;

        PropertyGet
        (
            std::function<std::shared_ptr<Object>()>&& g
        );

        std::shared_ptr<Object> get_value(NativeInstance*) override;
        bool set_value(NativeInstance*, std::shared_ptr<Object>) override;
    };
}


struct Interpreter;
struct ErrorHandler;


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
            [func](Callable* callable, ArgumentHelper& helper) -> std::shared_ptr<Object>
            {
                // const auto cc = callable->to_string();
                assert(callable->is_bound());
                BoundCallable* bound = static_cast<BoundCallable*>(callable);
                assert(bound->bound != nullptr);
                assert(bound->bound->get_type() == ObjectType::native_instance);
                std::shared_ptr<NativeInstance> native_instance = std::static_pointer_cast<NativeInstance>(bound->bound);
                assert(native_instance != nullptr);
                // todo(Gustav): uncoment this!
                // assert(native_instance->native_id == detail::get_unique_id<T>());
                auto specific_type = std::static_pointer_cast<detail::NativeInstanceT<T>>(native_instance);
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

    template<typename P>
    ClassAdder<T>&
    add_getter(const std::string& name, std::function<P(T&)> getter)
    {
        auto prop = std::make_unique<detail::PropertyGetT<T, P>>(std::move(getter));
        native_klass->add_property(name, std::move(prop));
        return *this;
    }

    
    template<typename P>
    ClassAdder<T>&
    add_native_getter(const std::string& name, std::function<std::shared_ptr<Object>(T&)> getter)
    {
        auto prop = std::make_unique<detail::PropertyNativeGetT<T, P>>(std::move(getter));
        native_klass->add_property(name, std::move(prop));
        return *this;
    }
};






// ----------------------------------------------------------------------------

template<typename T>
struct NativeRef
{
    std::shared_ptr<NativeInstance> instance;


    NativeRef()
        : instance(nullptr)
    {
    }

    explicit NativeRef(std::shared_ptr<NativeInstance>&& in)
        : instance(std::move(in))
    {
    }

    T* get_ptr() const
    {
        if(instance == nullptr) { return nullptr; }
        return &std::static_pointer_cast<detail::NativeInstanceT<T>>(instance)->data;
    }

    T* operator->()
    {
        assert(instance != nullptr);
        return get_ptr();
    }

    T& operator*()
    {
        assert(instance != nullptr);
        return *get_ptr();
    }

    operator bool()
    {
        return instance != nullptr;
    }

    bool operator==(T* rhs) const
    {
        return get_ptr() == rhs;
    }

    bool operator!=(T* rhs) const
    {
        return get_ptr() != rhs;
    }
};

template<typename T>
NativeRef<T> as_native(std::shared_ptr<Object> obj)
{
    return NativeRef<T>{as_native_instance_of_type(obj, detail::get_unique_id<T>())};
}

// ----------------------------------------------------------------------------


struct ArgumentHelper
{
    const lox::Arguments& args;
    u64 next_argument;
    bool has_read_all_arguments;

    explicit ArgumentHelper(const lox::Arguments& args);
    void verify_completion();

    // todo(Gustav): add some match/switch helper to handle overloads

    std::shared_ptr<Instance>   require_instance ();
    std::shared_ptr<Object>     require_object   ();
    std::string                 require_string   ();
    bool                        require_bool     ();
    Ti                          require_int      ();
    Tf                          require_float    ();
    std::shared_ptr<Callable>   require_callable ();
    std::shared_ptr<Array>      require_array    ();

    std::shared_ptr<NativeInstance> impl_require_native_instance(std::size_t klass);

    template<typename T>
    NativeRef<T> require_native()
    {
        return NativeRef<T>{impl_require_native_instance(detail::get_unique_id<T>())};
    }

    void complete();

    ArgumentHelper(ArgumentHelper&&) = delete;
    ArgumentHelper(const ArgumentHelper&) = delete;
    void operator=(ArgumentHelper&&) = delete;
    void operator=(const ArgumentHelper&) = delete;
};


// ----------------------------------------------------------------------------

template<typename T>
NativeRef<T> get_derived(const ArgumentHelper&, std::shared_ptr<Instance> instance)
{
    std::shared_ptr<Instance> in = instance;
    while(in != nullptr)
    {
        auto native = as_native<T>(in);
        if(native != nullptr)
        {
            return native;
        }
        in = in->parent;
    }

    // todo(Gustav): report error
    return {};
}

// ----------------------------------------------------------------------------


struct Scope
{
    Interpreter* interpreter;
    explicit Scope(Interpreter* inter);
    virtual ~Scope() = default;

    void
    define_native_function
    (
        const std::string& name,
        std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)>&& func
    );

    std::shared_ptr<NativeKlass>
    register_native_klass_impl
    (
        const std::string& name,
        std::size_t id,
        std::function<std::shared_ptr<Instance> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)>&& c
    );

    virtual void
    set_property(const std::string& name, std::shared_ptr<Object> value) = 0;

    virtual void
    add_property(const std::string& name, ObjectGenerator&& value) = 0;

    template<typename T>
    ClassAdder<T>
    define_native_class(const std::string& name)
    {
        auto native_klass = register_native_klass_impl
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
    define_native_class(const std::string& name, std::function<T (ArgumentHelper&)> constructor)
    {
        auto native_klass = register_native_klass_impl
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

    Scope&
    add_native_getter(const std::string& name, ObjectGenerator&& getter);
};


// ----------------------------------------------------------------------------

std::shared_ptr<Scope>
get_global_scope(Interpreter* interpreter);

std::shared_ptr<Scope>
get_package_scope_from_known_path(Interpreter* interpreter, const std::string& package_path);

// ----------------------------------------------------------------------------



}

