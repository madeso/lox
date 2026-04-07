#include "lax/token.h"


namespace lax
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


std::string string_from_asm(const AsmLiteral& literal)
{
    if (std::holds_alternative<Ti>(*literal))
    {
        return fmt::format("{}", std::get<Ti>(*literal));
    }
    else if (std::holds_alternative<Tf>(*literal))
    {
        return fmt::format("{}", std::get<Tf>(*literal));
    }
    else if (std::holds_alternative<std::string>(*literal))
    {
        return fmt::format("\"{}\"", std::get<std::string>(*literal));
    }
    else
    {
        assert(false && "invalid AsmLiteral");
        return "???";
    }
}

std::string AsmToken::to_debug_string() const
{
    const std::string of = offset.start != offset.end
        ? fmt::format("({} {})", offset.start, offset.end)
        : fmt::format("({})", offset.start)
        ;

    if (literal.has_value())
    {
        return fmt::format("{}({}) {} value=<{}>", tokentype_to_string(type), lexeme, of, string_from_asm(literal));
    }
    else
    {
        return fmt::format("{}({}) {}", tokentype_to_string(type), of, lexeme);
    }
}


}
