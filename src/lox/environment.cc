#include "lox/environment.h"


namespace lox
{


Environment::Environment(std::shared_ptr<Environment> parent)
    : enclosing(parent)
{
}


void
Environment::define(const std::string& name, std::shared_ptr<Object> value)
{
    values[name] = value;
}

// todo(Gustav): provide a list of all variables with their location so we
// can edit-distance search for potentiall misspelled vars when a miss occurs

std::shared_ptr<Object>
Environment::get_or_null(const std::string& name)
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


Environment*
get_ancestor_or_null(Environment* e, std::size_t dist)
{
    Environment* env = e;
    
    for(std::size_t count_to_dist = 0; count_to_dist < dist; count_to_dist += 1)
    {
        env = env->enclosing.get();
    }

    return env;
}


std::shared_ptr<Object>
Environment::get_at_or_null(std::size_t distance, const std::string& name)
{
    Environment* ancestor = get_ancestor_or_null(this, distance);
    assert(ancestor != nullptr);
    auto found = ancestor->values.find(name);
    if(found != values.end())
    {
        return found->second;
    }
    else
    {
        return nullptr;
    }
}


bool
Environment::set_or_false(const std::string& name, std::shared_ptr<Object> value)
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

bool
Environment::set_at_or_false(std::size_t distance, const std::string& name, std::shared_ptr<Object> value)
{
    Environment* ancestor = get_ancestor_or_null(this, distance);
    assert(ancestor != nullptr);

    auto found = ancestor->values.find(name);
    if(found != ancestor->values.end())
    {
        found->second = value;
        return true;
    }
    else
    {
        return false;
    }
}

}

