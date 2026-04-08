#pragma once

#include <variant>

#include "lax/object.h"
#include "lax/source.h"
#include "lax/tokentype.h"

namespace lax
{

struct Interpreter;

struct Token
{
    TokenType type;
    std::string_view lexeme;
    std::shared_ptr<Object> literal;
    Offset offset;

    Token(TokenType type, std::string_view lexeme, std::shared_ptr<Object> literal, const Offset& offset);

    std::string
    to_debug_string(Interpreter* inter) const;
};

using AsmLiteralValue = std::variant<Ti, Tf, std::string>;
using AsmLiteral = std::optional<AsmLiteralValue>;
struct AsmToken
{
    AsmTokenType type;
    std::string_view lexeme;
    AsmLiteral literal;
    Offset offset;

    std::string
    to_debug_string() const;
};

std::string string_from_asm(const AsmLiteral& literal);


}
