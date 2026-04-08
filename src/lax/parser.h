#pragma once

#include <memory>
#include <vector>

#include "lax/ast.h"
#include "lax/token.h"
#include "lax/program.h"


namespace lax
{

struct ErrorHandler;


// return nullptr when parsing failed

struct LaxParseResult
{
    int errors;
    std::shared_ptr<Program> program;
};

struct ParsedAsmInstruction
{
    std::optional<std::string> label;
    AsmTokenType instruction;
    std::vector<AsmLiteralValue> arguments;
};

struct AsmParseResult
{
    int errors;
    std::vector<ParsedAsmInstruction> program;
};

LaxParseResult
parse_lax_program(std::vector<Token>& tokens, ErrorHandler* error_handler);

AsmParseResult
parse_asm_program(std::vector<AsmToken>& tokens, ErrorHandler* error_handler);

}

