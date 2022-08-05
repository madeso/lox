#include "lox/object.h"



namespace lox { namespace
{



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
    std::function<std::shared_ptr<Object>(const Arguments& arguments)> func
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


}
