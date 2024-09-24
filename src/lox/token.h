#pragma once


#include "lox/object.h"
#include "lox/source.h"
#include "lox/tokentype.h"

namespace lox
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
