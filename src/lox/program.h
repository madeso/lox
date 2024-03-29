#pragma once

#include <vector>

#include "lox/ast.h"


namespace lox
{

struct Program
{
    std::vector<std::shared_ptr<Statement>> statements;
};

}
