#pragma once

#include "lox/token.h"

namespace lox
{

struct ErrorHandler;
std::vector<Token> ScanTokens(const std::string& source, ErrorHandler* error_handler);

}
