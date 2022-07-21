#include "lox/token.h"


namespace lox
{

    
Token::Token(TokenType the_type, std::string_view the_lexeme, std::shared_ptr<Object> the_literal, const Offset& the_offset)
    : type(the_type)
    , lexeme(the_lexeme)
    , literal(the_literal)
    , offset(the_offset)
{
}

std::string Token::toString() const
{
    return fmt::format("{0} {1} {2}", ToString(type), lexeme, literal != nullptr ? literal->toString() : "");
}


}
