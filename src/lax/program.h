#pragma once

#include <vector>

#include "lax/ast.h"


namespace lax
{

struct Program
{
    std::vector<std::shared_ptr<Statement>> statements;
};

}
