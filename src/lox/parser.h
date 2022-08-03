#pragma once

#include <memory>
#include <vector>

#include "lox/ast.h"
#include "lox/token.h"
#include "lox/program.h"


namespace lox
{

struct ErrorHandler;


// return nullptr when parsing failed

struct ParseResult
{
    int errors;
    std::shared_ptr<Program> program;
};

ParseResult
parse_program(std::vector<Token>& tokens, ErrorHandler* error_handler);

}

