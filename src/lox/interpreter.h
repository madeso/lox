#pragma once

#include <functional>

#include "lox/ints.h"
#include "lox/environment.h"


namespace lox
{

struct ErrorHandler;
struct Program;


struct Arguments
{
    std::vector<std::shared_ptr<Object>> arguments;
};


// runtime errors are sent to the error handler
// return false on runtime error


struct Interpreter
{
    Interpreter() = default;
    virtual ~Interpreter() = default;
    
    virtual Environment& get_global_environment() = 0;
    virtual ErrorHandler* get_error_handler() = 0;

    virtual bool
    interpret
    (
        Program& expression
    ) = 0;
};


std::shared_ptr<Interpreter>
make_interpreter
(
    ErrorHandler* error_handler,
    const std::function<void (std::string)>& on_line
);


void
verify_number_of_arguments(const Arguments& args, u64 arity);


}
