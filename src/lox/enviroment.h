#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "lox/object.h"


namespace lox
{

struct ErrorHandler;


struct Enviroment
{
    std::unordered_map<std::string, std::shared_ptr<Object>> values;

    void define(const std::string& name, std::shared_ptr<Object> value);
    std::shared_ptr<Object> get_or_null(const std::string& name);
    bool set_or_false(const std::string& name, std::shared_ptr<Object> value);
};

}
