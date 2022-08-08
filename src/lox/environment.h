#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "lox/object.h"


namespace lox
{

struct ErrorHandler;


struct Environment
{
    std::shared_ptr<Environment> enclosing;
    std::unordered_map<std::string, std::shared_ptr<Object>> values;

    explicit Environment(std::shared_ptr<Environment> parent);

    void define(const std::string& name, std::shared_ptr<Object> value);
    std::shared_ptr<Object> get_or_null(const std::string& name);
    std::shared_ptr<Object> get_at_or_null(std::size_t distance, const std::string& name);
    bool set_at_or_false(std::size_t distance, const std::string& name, std::shared_ptr<Object> value);
    bool set_or_false(const std::string& name, std::shared_ptr<Object> value);
};

}
