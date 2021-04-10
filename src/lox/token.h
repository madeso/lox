#pragma once

#include <memory>
#include <string>

#include "lox/object.h"
#include "lox/tokentype.h"

namespace lox
{
    struct Token
    {
        TokenType type;
        std::string lexeme;
        std::shared_ptr<Object> literal;
        int line;

        Token(TokenType type, const std::string& lexeme, std::shared_ptr<Object> literal, int line);

        std::string toString() const;
    };
}
