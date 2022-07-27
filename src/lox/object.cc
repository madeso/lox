#include "lox/object.h"


namespace lox
{

// ----------------------------------------------------------------------------



std::shared_ptr<Object>
Nil::clone() const
{
    return std::make_shared<Nil>();
}

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

std::shared_ptr<Object>
String::clone() const
{
    return std::make_shared<String>(value);
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


std::shared_ptr<Object>
Bool::clone() const
{
    return std::make_shared<Bool>(value);
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

std::shared_ptr<Object>
Number::clone() const
{
    return std::make_shared<Number>(value);
}

ObjectType
Number::get_type() const
{
    return ObjectType::number;
}

std::string
Number::to_string() const
{
    return fmt::format("{0}", value);
}



// ----------------------------------------------------------------------------

}
