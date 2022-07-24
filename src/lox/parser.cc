#include "lox/parser.h"

#include "lox/errorhandler.h"


namespace lox { namespace {


struct ParseError{};


struct Parser
{
    std::vector<Token>& tokens;
    ErrorHandler* error_handler;
    std::size_t current = 0;

    explicit Parser(std::vector<Token>& t, ErrorHandler* eh)
        : tokens(t)
        , error_handler(eh)
    {
    }

    std::unique_ptr<Expr>
    parse_expression()
    {
        return parse_equality();
    }

    std::unique_ptr<Expr>
    parse_equality()
    {
        auto expr = parse_comparison();

        while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL}))
        {
            auto op = previous().type;
            auto right = parse_comparison();
            expr = std::make_unique<ExprBinary>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expr>
    parse_comparison()
    {
        auto expr = parse_term();

        while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL}))
        {
            auto op = previous().type;
            auto right = parse_term();
            expr = std::make_unique<ExprBinary>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expr>
    parse_term()
    {
        auto expr = parse_factor();

        while (match({TokenType::MINUS, TokenType::PLUS})) {
            auto op = previous().type;
            auto right = parse_factor();
            expr = std::make_unique<ExprBinary>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expr>
    parse_factor()
    {
        auto expr = parse_unary();

        while (match({TokenType::SLASH, TokenType::STAR}))
        {
            auto op = previous().type;
            auto right = parse_unary();
            expr = std::make_unique<ExprBinary>(std::move(expr), op, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expr>
    parse_unary()
    {
        if (match({TokenType::BANG, TokenType::MINUS}))
        {
            auto op = previous().type;
            auto right = parse_unary();
            return std::make_unique<ExprUnary>(op, std::move(right));
        }

        return parse_primary();
    }

    std::unique_ptr<Expr>
    parse_primary()
    {
        if (match({TokenType::FALSE})) { return std::make_unique<ExprLiteral>(std::make_unique<Bool>(false)); }
        if (match({TokenType::TRUE})) { return std::make_unique<ExprLiteral>(std::make_unique<Bool>(true)); }
        if (match({TokenType::NIL})) { return std::make_unique<ExprLiteral>(std::make_unique<Nil>()); }

        if (match({TokenType::NUMBER, TokenType::STRING}))
        {
            return std::make_unique<ExprLiteral>(std::move(previous().literal));
        }

        if (match({TokenType::LEFT_PAREN}))
        {
            auto expr = parse_expression();
            consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
            return std::make_unique<ExprGrouping>(std::move(expr));
        }

        throw error(peek(), "Expected expression.");
    }


    /// This checks to see if the current token has any of the given types.
    /// If so, it consumes the token and returns true.
    // Otherwise, it returns false and leaves the current token alone.
    bool
    match(const std::vector<TokenType>& types)
    {
        for (TokenType type : types)
        {
            if (check(type))
            {
                advance();
                return true;
            }
        }

        return false;
    }

    bool
    check(TokenType type)
    {
        if (isAtEnd())
        {
            return false;
        }
        else
        {
            return peek().type == type;
        }
    }

    // consumes the current token and returns it
    Token&
    advance()
    {
        if (isAtEnd() == false)
        {
            current += 1;
        }

        return previous();
    }

    bool
    isAtEnd() 
    {
        return peek().type == TokenType::EOF;
    }

    Token&
    peek() 
    {
        return tokens[current];
    }

    Token&
    previous() 
    {
        return tokens[current - 1];
    }

    Token&
    consume(TokenType type, const std::string& message)
    {
        if(check(type))
        {
            return advance();
        }
        else
        {
            throw error(peek(), message);
        }
    }

    ParseError
    error(const Token& token, const std::string& message)
    {
        report_error(token, message);
        return ParseError{};
    }

    void
    synchronize_parser_state()
    {
        advance();

        while (isAtEnd() == false)
        {
            if (previous().type == TokenType::SEMICOLON)
            {
                return;
            }

            switch (peek().type)
            {
            case TokenType::CLASS:
            case TokenType::FUN:
            case TokenType::VAR:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::PRINT:
            case TokenType::RETURN:
                return;
            default:
                // ignore other types
                advance();
                break;
            }
        }
    }

    void
    report_error(const Token& token, const std::string& message)
    {
        if (token.type == TokenType::EOF)
        {
            error_handler->on_error({token.offset.start}, message);
        }
        else
        {
            error_handler->on_error(token.offset, message);
        }
    }
};


}}


namespace lox
{

std::unique_ptr<Expr>
parse_expression(std::vector<Token>& tokens, ErrorHandler* error_handler)
{
    try
    {
        auto parser = Parser{tokens, error_handler};
        return parser.parse_expression();
    }
    catch(const ParseError&)
    {
        return nullptr;
    }
}

}
