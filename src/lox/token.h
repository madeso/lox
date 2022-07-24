#pragma once


#include "lox/object.h"
#include "lox/offset.h"
#include "lox/tokentype.h"

namespace lox
{


struct Token
{
    TokenType type;
    std::string_view lexeme;
    std::unique_ptr<Object> literal;
    Offset offset;

    Token(TokenType type, std::string_view lexeme, std::unique_ptr<Object> literal, const Offset& offset);

    std::string
    to_string() const;
};


}
