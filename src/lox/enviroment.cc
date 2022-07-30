#include "lox/enviroment.h"


namespace lox
{


Enviroment::Enviroment(Enviroment* parent)
    : enclosing(parent)
{
}


void
Enviroment::define(const std::string& name, std::shared_ptr<Object> value)
{
    values[name] = value;
}

// todo(Gustav): provide a list of all variables with their location so we
// can edit-distance search for potentiall misspelled vars when a miss occurs

std::shared_ptr<Object>
Enviroment::get_or_null(const std::string& name)
{
    auto found = values.find(name);
    if(found != values.end())
    {
        return found->second;
    }
    else
    {
        if(enclosing != nullptr)
        {
            return enclosing->get_or_null(name);
        }
        else
        {
            return nullptr;
        }
    }
}


bool
Enviroment::set_or_false(const std::string& name, std::shared_ptr<Object> value)
{
    auto found = values.find(name);
    if(found == values.end())
    {
        if(enclosing != nullptr)
        {
            return enclosing->set_or_false(name, value);
        }
        else
        {
            return false;
        }
    }
    found->second = value;
    return true;
}


}

