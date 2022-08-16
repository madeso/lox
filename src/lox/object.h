#pragma once

#include <memory>

#include "lox/ints.h"


namespace lox
{


enum class ObjectType
{
    nil, string, boolean, number, callable, klass, instance, native_instance
};


struct Environment;


constexpr std::string_view objecttype_to_string(ObjectType ot)
{
    switch (ot)
    {
    case ObjectType::nil:             return "nil";
    case ObjectType::string:          return "string";
    case ObjectType::boolean:         return "boolean";
    case ObjectType::number:          return "number";
    case ObjectType::klass:           return "class";
    case ObjectType::instance:        return "instance";
    case ObjectType::native_instance: return "native instance";
    default:                          assert(false && "invalid type"); return "???";
    }
}


// ----------------------------------------------------------------------------

struct Object;
struct NativeFunction;

struct WithProperties
{
    virtual ~WithProperties() = default;
    virtual std::shared_ptr<Object> get_property_or_null(const std::string& name) = 0;
    virtual bool set_property_or_false(const std::string& name, std::shared_ptr<Object> value) = 0;
};


struct Object : std::enable_shared_from_this<Object>
{
    Object() = default;
    virtual ~Object() = default;

    virtual ObjectType get_type() const = 0;
    virtual std::string to_string() const = 0;
    virtual bool is_callable() const = 0;
    
    virtual WithProperties* get_properties_or_null();
};



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
    std::string to_string() const override;
    std::shared_ptr<Object> call(const Arguments& arguments) override;
    std::shared_ptr<Callable> bind(std::shared_ptr<Object> instance) override;
    bool is_bound() const override;
};

// ----------------------------------------------------------------------------

// todo(Gustav): remove callable and transform to new constructor
struct Klass : Callable
{
    std::string klass_name;
    std::shared_ptr<Klass> superklass;
    std::unordered_map<std::string, std::shared_ptr<Callable>> methods;

    Klass(const std::string& n, std::shared_ptr<Klass> sk);
    

    ObjectType
    get_type() const override;

    std::shared_ptr<Object>
    call(const Arguments& arguments) override;

    std::shared_ptr<Callable>
    bind(std::shared_ptr<Object> instance) override;

    virtual std::shared_ptr<Object> constructor(const Arguments& arguments) = 0;

    bool add_method_or_false(const std::string& name, std::shared_ptr<Callable> method);
    std::shared_ptr<Callable> find_method_or_null(const std::string& name);
};

// ----------------------------------------------------------------------------


struct Instance : Object, WithProperties
{
    std::shared_ptr<Klass> klass;

    explicit Instance(std::shared_ptr<Klass> o);
    bool is_callable() const override;
    WithProperties* get_properties_or_null() override;
    std::shared_ptr<Object> get_property_or_null(const std::string& name) override;
    bool set_property_or_false(const std::string& name, std::shared_ptr<Object> value) override;

    virtual std::shared_ptr<Object> get_field_or_null(const std::string& name) = 0;
    virtual bool set_field_or_false(const std::string& name, std::shared_ptr<Object> value) = 0;
};

// ----------------------------------------------------------------------------

struct NativeInstance;

struct Property
{
    virtual ~Property() = default;

    virtual std::shared_ptr<Object> get_value(NativeInstance* instance) = 0;
    virtual void set_value(NativeInstance* instance, std::shared_ptr<Object> value) = 0;
};

struct NativeKlass : Klass
{
    std::size_t native_id;
    std::unordered_map<std::string, std::unique_ptr<Property>> properties;

    NativeKlass(const std::string& n, std::size_t id, std::shared_ptr<Klass> sk);

    std::string to_string() const override;

    void add_property(const std::string& name, std::unique_ptr<Property> prop);
};

struct NativeInstance : Instance
{
    NativeInstance(std::shared_ptr<NativeKlass> o);

    ObjectType get_type() const override;
    std::string to_string() const override;

    std::shared_ptr<Object> get_field_or_null(const std::string& name) override;
    bool set_field_or_false(const std::string& name, std::shared_ptr<Object> value) override;
};

// ----------------------------------------------------------------------------


std::shared_ptr<Object>     make_nil              ();
std::shared_ptr<Object>     make_string           (const std::string& str);
std::shared_ptr<Object>     make_bool             (bool b);
std::shared_ptr<Object>     make_number           (float f);
std::shared_ptr<Callable>   make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(Callable*, const Arguments& arguments)>&& func
);

// ----------------------------------------------------------------------------


bool                        is_nil      (std::shared_ptr<Object> o);
std::optional<std::string>  as_string   (std::shared_ptr<Object> o);
std::optional<bool>         as_bool     (std::shared_ptr<Object> o);
std::optional<float>        as_number   (std::shared_ptr<Object> o);
std::shared_ptr<Callable>   as_callable (std::shared_ptr<Object> o);


// ----------------------------------------------------------------------------


std::string  get_string_or_ub  (std::shared_ptr<Object> o);
bool         get_bool_or_ub    (std::shared_ptr<Object> o);
float        get_number_or_ub  (std::shared_ptr<Object> o);


bool is_truthy(std::shared_ptr<Object> o);


// ----------------------------------------------------------------------------



}

