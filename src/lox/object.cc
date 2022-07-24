#include "lox/object.h"


namespace lox
{


std::string
Nil::to_string() const
{
    return "nil";
}


// ----------------------------------------------------------------------------


String::String(const std::string_view& s)
    : value(s)
{
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

std::string
Number::to_string() const
{
    return fmt::format("{0}", value);
}


}
