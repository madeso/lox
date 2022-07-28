#include "lox/enviroment.h"


namespace lox
{



void
Enviroment::define(const std::string& name, std::shared_ptr<Object> value)
{
    values[name] = value;
}

std::shared_ptr<Object>
Enviroment::get_or_null(const std::string& name)
{
    auto found = values.find(name);
    if(found == values.end()) { return nullptr; }

    return found->second;
}


}

