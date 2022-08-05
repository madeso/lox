#pragma once

#include "lox/ints.h"


namespace lox
{


enum class ObjectType
{
    nil, string, boolean, number, callable
};


struct Arguments;
struct Environment;


constexpr std::string_view objecttype_to_string(ObjectType ot)
{
    switch (ot)
    {
    case ObjectType::nil:     return "nil";
    case ObjectType::string:  return "string";
    case ObjectType::boolean: return "boolean";
    case ObjectType::number:  return "number";
    default:                  return "???";
    }
}


// ----------------------------------------------------------------------------


struct Object
{
    Object() = default;
    virtual ~Object() = default;

    virtual ObjectType get_type() const = 0;
    virtual std::string to_string() const = 0;
};

// ----------------------------------------------------------------------------

struct Nil : public Object
{
    Nil() = default;
    virtual ~Nil() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
};


struct String : public Object
{
    std::string value;

    explicit String(const std::string& s);
    explicit String(const std::string_view& s);
    virtual ~String() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
};


struct Bool : public Object
{
    bool value;

    explicit Bool(bool b);
    virtual ~Bool() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
};


struct Number : public Object
{
    float value;

    explicit Number(float f);
    virtual ~Number() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
};


struct Callable : public Object
{
    virtual std::shared_ptr<Object> call(const Arguments& arguments) = 0;
    ObjectType get_type() const override;
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




// ----------------------------------------------------------------------------

// not sure where to place this... in a integration header? for now it's here!


void
define_native_function
(
    const std::string& name,
    Environment& env,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
);

struct ArgumentHelper
{
    const lox::Arguments& args;
    u64 next_argument;
    bool has_read_all_arguments;

    explicit ArgumentHelper(const lox::Arguments& args);
    ~ArgumentHelper();

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


// ----------------------------------------------------------------------------



}

