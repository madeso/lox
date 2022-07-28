#pragma once

namespace lox
{


struct ErrorHandler;
struct Program;


// runtime errors are sent to the error handler
// return false on runtime error

bool
interpret(Program& expression, ErrorHandler* error_handler);


}
