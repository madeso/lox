#pragma once


#include "lox/ast.h"


namespace lox
{

struct ErrorHandler;

struct Resolved
{
    std::unordered_map<u64, std::size_t> locals;
};

std::optional<Resolved>
resolve(Program& program, ErrorHandler* err);


}

