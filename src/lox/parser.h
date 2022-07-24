#pragma once

#include <memory>
#include <vector>

#include "lox/ast.h"
#include "lox/token.h"


namespace lox
{

struct ErrorHandler;


// return nullptr when parsing failed

std::unique_ptr<Expr>
parse_expression(std::vector<Token>& tokens, ErrorHandler* error_handler);

}

