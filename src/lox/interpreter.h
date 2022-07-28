#pragma once

#include "lox/enviroment.h"


namespace lox
{

struct ErrorHandler;
struct Program;


// runtime errors are sent to the error handler
// return false on runtime error

struct Interpreter
{
    Interpreter();
    
    Enviroment global_enviroment;
};

bool
interpret(Interpreter* interpreter, Program& expression, ErrorHandler* error_handler);


}
