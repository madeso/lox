#pragma once

#include <memory>

#include "lox/ints.h"


namespace lox
{


enum class ObjectType
{
    nil, string, boolean, number, callable, klass, instance
};


struct Environment;


constexpr std::string_view objecttype_to_string(ObjectType ot)
{
    switch (ot)
    {
    case ObjectType::nil:      return "nil";
    case ObjectType::string:   return "string";
    case ObjectType::boolean:  return "boolean";
    case ObjectType::number:   return "number";
    case ObjectType::klass:    return "class";
    case ObjectType::instance: return "instance";
    default:                   assert(false && "invalid type"); return "???";
    }
}


// ----------------------------------------------------------------------------

struct Object;


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

    virtual std::shared_ptr<Callable> bind(std::shared_ptr<Object> instance) = 0;
};

// ----------------------------------------------------------------------------


struct Klass : Callable
{
    std::string name;

    explicit Klass(const std::string& n);

    ObjectType
    get_type() const override;

    std::string
    to_string() const override;

    virtual std::shared_ptr<Object> constructor(std::shared_ptr<Klass> klass, const Arguments& arguments) = 0;

    std::shared_ptr<Object>
    call(const Arguments& arguments) override;

    std::shared_ptr<Callable>
    bind(std::shared_ptr<Object> instance) override;
};

// ----------------------------------------------------------------------------


std::shared_ptr<Object>   make_nil              ();
std::shared_ptr<Object>   make_string           (const std::string& str);
std::shared_ptr<Object>   make_bool             (bool b);
std::shared_ptr<Object>   make_number           (float f);
std::shared_ptr<Object>   make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
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

