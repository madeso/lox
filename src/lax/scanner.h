#pragma once

#include "lax/token.h"

namespace lax
{

struct ErrorHandler;

struct ScanResult
{
    int errors = 0;
    std::vector<Token> tokens;
};

struct AsmScanResult
{
    int errors = 0;
    std::vector<AsmToken> tokens;
};

ScanResult
scan_lox_tokens(const std::string_view source, ErrorHandler* error_handler);

AsmScanResult
scan_asm_tokens(std::string_view source, ErrorHandler* error_handler);

std::vector<std::string>
parse_package_path(const std::string& path);

}
