#pragma once

#include <functional>

#include "lox/enviroment.h"


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
    
    Enviroment global_enviroment;
};


void
verify_number_of_arguments(const Arguments& args, unsigned int arity);


bool
interpret
(
    Interpreter* interpreter,
    Program& expression,
    ErrorHandler* error_handler,
    const std::function<void (std::string)>& on_line
);


}
