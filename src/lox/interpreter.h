#pragma once


#include "lox/ast.h"


namespace lox
{


struct ErrorHandler;


// runtime errors are sent to the error handler
// return false on runtime error

bool
interpret(Expr& expression, ErrorHandler* error_handler);


}
