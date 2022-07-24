#pragma once

#include <string>

#include "lox_expression.h"


namespace lox
{

std::string print_ast(const Expr& ast);
std::string ast_to_grapviz(const Expr& ast);

}

