#include "lox/object.h"


namespace lox
{


String::String(const std::string_view& s)
    : string(s)
{
}

std::string String::toString() const
{
    return string;
}

Number::Number(float f)
    : number(f)
{
}

std::string Number::toString() const
{
    return fmt::format("{0}", number);
}


}
