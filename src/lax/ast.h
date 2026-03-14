#pragma once

#include <string>

#include "lax_expression.h"


namespace lax
{

struct Program;

std::string print_ast(const Program& ast);
std::string ast_to_grapviz(const Program& ast);

}

