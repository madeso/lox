#include "lox/object.h"

#include "interpreter.h"


namespace lox { namespace
{


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

struct NativeFunction : Callable
{
    std::string name;
    std::function<std::shared_ptr<Object>(const Arguments& arguments)> func;

    NativeFunction
    (
        const std::string& n,
        std::function<std::shared_ptr<Object>(const Arguments& arguments)> f
    )
        : name(n)
        , func(f)
    {
    }

    std::string
    to_string() const override
    {
        return "<native fun {}>"_format(name);
    }

    std::shared_ptr<Object>
    call(const Arguments& arguments) override
    {
        return func(arguments);
    }
};


}}




// ----------------------------------------------------------------------------



namespace lox
{



ObjectType
Nil::get_type() const
{
    return ObjectType::nil;
}

std::string
Nil::to_string() const
{
    return "nil";
}


// ----------------------------------------------------------------------------



String::String(const std::string& s)
    : value(s)
{
}

String::String(const std::string_view& s)
    : value(s)
{
}

ObjectType
String::get_type() const
{
    return ObjectType::string;
}

std::string
String::to_string() const
{
    return value;
}



// ----------------------------------------------------------------------------



Bool::Bool(bool b)
    : value(b)
{
}

ObjectType
Bool::get_type() const
{
    return ObjectType::boolean;
}

std::string
Bool::to_string() const
{
    if(value) { return "true"; }
    else { return "false"; }
}



// ----------------------------------------------------------------------------



Number::Number(float f)
    : value(f)
{
}

ObjectType
Number::get_type() const
{
    return ObjectType::number;
}

std::string
Number::to_string() const
{
    return "{0}"_format(value);
}


// ----------------------------------------------------------------------------



ObjectType Callable::get_type() const
{
    return ObjectType::callable;
}



// ----------------------------------------------------------------------------


std::shared_ptr<Object>
make_nil()
{
    return std::make_shared<Nil>();
}


std::shared_ptr<Object>
make_string(const std::string& str)
{
    return std::make_shared<String>(str);
}


std::shared_ptr<Object>
make_bool(bool b)
{
    return std::make_shared<Bool>(b);
}


std::shared_ptr<Object>
make_number(float f)
{
    return std::make_shared<Number>(f);
}


std::shared_ptr<Object>
make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
)
{
    return std::make_shared<NativeFunction>(name, func);
}


// ----------------------------------------------------------------------------


bool
is_nil(std::shared_ptr<Object> o)
{
    return o->get_type() == ObjectType::nil;
}

std::optional<std::string>
as_string(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::string) { return std::nullopt; }
    auto* ptr = static_cast<String*>(o.get());
    return ptr->value;
}

std::optional<bool>
as_bool(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::boolean) { return std::nullopt; }
    auto* ptr = static_cast<Bool*>(o.get());
    return ptr->value;
}

std::optional<float>
as_number(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::number) { return std::nullopt; }
    auto* ptr = static_cast<Number*>(o.get());
    return ptr->value;
}

std::shared_ptr<Callable>
as_callable(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::callable) { return nullptr; }
    return std::static_pointer_cast<Callable>(o);
}


// ----------------------------------------------------------------------------


float
get_number_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::number);
    return static_cast<Number*>(o.get())->value;
}


std::string
get_string_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::string);
    return static_cast<String*>(o.get())->value;
}


bool
get_bool_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::boolean);
    return static_cast<Bool*>(o.get())->value;
}


bool
is_truthy(std::shared_ptr<Object> o)
{
    switch(o->get_type())
    {
    case ObjectType::nil:
        return false;
    case ObjectType::boolean:
        return static_cast<const Bool*>(o.get())->value;
    default:
        return true;
    }
}


// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------


void
define_native_function
(
    const std::string& name,
    Environment& env,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
)
{
    env.define(name, make_native_function(name, std::move(func)));
}


ArgumentHelper::ArgumentHelper(const lox::Arguments& aargs)
    : args(aargs)
    , next_argument(0)
    , has_read_all_arguments(false)
{
}

ArgumentHelper::~ArgumentHelper()
{
    assert(has_read_all_arguments && "complete() not called");
}

void
ArgumentHelper::complete()
{
    assert(has_read_all_arguments==false && "complete() called twice!");
    has_read_all_arguments = true;
    verify_number_of_arguments(args, next_argument);
}

std::string
ArgumentHelper::require_string()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return ""; }
    return get_string_from_arg(args, argument_index);
}

bool
ArgumentHelper::require_bool()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return false; }
    return get_bool_from_arg(args, argument_index);
}

float
ArgumentHelper::require_number()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return 0.0f; }
    return get_number_from_arg(args, argument_index);
}

std::shared_ptr<Callable>
ArgumentHelper::require_callable()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_callable_from_arg(args, argument_index);
}


// ----------------------------------------------------------------------------

}
