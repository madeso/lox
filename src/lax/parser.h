#pragma once

#include <memory>
#include <vector>

#include "lax/ast.h"
#include "lax/token.h"
#include "lax/program.h"


namespace lax
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

