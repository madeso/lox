#pragma once

#include "lox/token.h"

namespace lox
{

struct ErrorHandler;

struct ScanResult
{
    int errors = 0;
    std::vector<Token> tokens;
};

ScanResult scan_tokens(const std::string_view source, ErrorHandler* error_handler);

}
