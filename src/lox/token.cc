#include "lox/token.h"


namespace lox
{
    Token::Token(TokenType the_type, const std::string& the_lexeme, std::shared_ptr<Object> the_literal, int the_line)
        : type(the_type)
        , lexeme(the_lexeme)
        , literal(the_literal)
        , line(the_line)
    {
    }

    std::string Token::toString() const
    {
        return fmt::format("{0} {1} {2}", ToString(type), lexeme, literal != nullptr ? literal->toString() : "");
    }
}
