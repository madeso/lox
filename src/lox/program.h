#pragma once

#include <vector>

#include "lox/ast.h"


namespace lox
{

struct Program
{
    std::vector<std::unique_ptr<Stmt>> statements;
};

}
