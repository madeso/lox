#include "lox/token.h"


namespace lox
{

    
Token::Token(TokenType the_type, std::string_view the_lexeme, std::shared_ptr<Object> the_literal, const Offset& the_offset)
    : type(the_type)
    , lexeme(the_lexeme)
    , literal(std::move(the_literal))
    , offset(the_offset)
{
}


std::string Token::to_debug_string(Interpreter* inter) const
{
    const std::string of = offset.start != offset.end
        ? fmt::format("({} {})", offset.start, offset.end)
        : fmt::format("({})", offset.start)
        ;

    if(literal != nullptr)
    {
        return fmt::format("{}({}) {} value=<{}>", tokentype_to_string(type), lexeme, of, literal->to_flat_string(inter, nullptr, ToStringOptions::for_debug()));
    }
    else
    {
        return fmt::format("{}({}) {}", tokentype_to_string(type), of, lexeme);
    }
}


}
