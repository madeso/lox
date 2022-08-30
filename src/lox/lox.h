#pragma once

#include "lox/object.h"

namespace lox
{

struct LoxImpl;

struct Lox
{
    Lox(std::unique_ptr<ErrorHandler> error_handler, std::function<void (const std::string&)> on_line);
    ~Lox();

    bool run_string(const std::string& source);

    std::shared_ptr<Scope> in_global_scope();
    std::shared_ptr<Scope> in_package(const std::string& name);

    Environment& get_global_environment();

    std::unique_ptr<LoxImpl> impl;
};


}
