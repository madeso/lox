#pragma once

#include <string>

#include "lox_expression.h"


namespace lox
{

struct Program;

std::string print_ast(const Program& ast);
std::string ast_to_grapviz(const Program& ast);

}

