#pragma once


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


}
