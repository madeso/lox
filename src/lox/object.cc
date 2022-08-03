#include "lox/object.h"


namespace lox
{

// ----------------------------------------------------------------------------


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

std::shared_ptr<Object>
make_nil()
{
    return std::make_shared<Nil>();
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

std::shared_ptr<Object>
make_string(const std::string& str)
{
    return std::make_shared<String>(str);
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

std::shared_ptr<Object>
make_bool(bool b)
{
    return std::make_shared<Bool>(b);
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

std::shared_ptr<Object>
make_float(float f)
{
    return std::make_shared<Number>(f);
}

std::shared_ptr<Object>
make_int(int i)
{
    return std::make_shared<Number>(i);
}


// ----------------------------------------------------------------------------



ObjectType Callable::get_type() const
{
    return ObjectType::callable;
}



// ----------------------------------------------------------------------------


namespace
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


}
