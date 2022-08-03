#pragma once

#include <functional>

#include "lox/ints.h"
#include "lox/environment.h"


namespace lox
{

struct ErrorHandler;
struct Program;
struct Arguments;

// runtime errors are sent to the error handler
// return false on runtime error


struct Interpreter
{
    Interpreter();
    
    Environment global_environment;
};


void
verify_number_of_arguments(const Arguments& args, u64 arity);


bool
interpret
(
    Interpreter* interpreter,
    Program& expression,
    ErrorHandler* error_handler,
    const std::function<void (std::string)>& on_line
);


}
