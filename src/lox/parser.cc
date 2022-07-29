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

    std::unique_ptr<Program>
    parse_program()
    {
        auto program = std::make_unique<Program>();

        while(!is_at_end())
        {
            auto dec = parse_declaration_or_null();
            if(dec != nullptr)
            {
                program->statements.emplace_back(std::move(dec));
            }
        }

        return program;
    }

    std::unique_ptr<Statement>
    parse_declaration_or_null()
    {
        try
        {
            if(match({TokenType::VAR}))
            {
                return parse_var_declaration();
            }

            return parse_statement();
        }
        catch(const ParseError&)
        {
            synchronize_parser_state();
            return nullptr;
        }
    }

    std::unique_ptr<Statement>
    parse_var_declaration()
    {
        const auto var = previous().offset;
        auto& name = consume(TokenType::IDENTIFIER, "Expected variable name");

        std::unique_ptr<Expression> initializer = nullptr;

        if(match({TokenType::EQUAL}))
        {
            initializer = parse_expression();
        }

        consume(TokenType::SEMICOLON, "Missing ';' after print statement");
        const auto end = previous().offset;
        return std::make_unique<VarStatement>(Offset{var.start, end.end}, std::string(name.lexeme), std::move(initializer));
    }

    std::unique_ptr<Statement>
    parse_statement()
    {
        if(match({TokenType::PRINT}))
        {
            return parse_print_statement();
        }
        if(match({TokenType::LEFT_BRACE}))
        {
            return parse_block_statement();
        }

        return parse_expression_statement();
    }

    std::unique_ptr<Statement>
    parse_block_statement()
    {
        auto start = previous().offset;
        std::vector<std::shared_ptr<Statement>> statements;

        while(check(TokenType::RIGHT_BRACE)==false && is_at_end() == false)
        {
            auto st = parse_declaration_or_null();
            if(st != nullptr)
            {
                statements.emplace_back(std::move(st));
            }
        }

        auto& end = consume(TokenType::RIGHT_BRACE, "Expected '}' after block.").offset;
        return std::make_unique<BlockStatement>(Offset{start.start, end.end}, statements);
    }

    std::unique_ptr<Statement>
    parse_print_statement()
    {
        const auto print = previous().offset;
        auto value = parse_expression();
        consume(TokenType::SEMICOLON, "Missing ';' after print statement");
        const auto end = previous().offset;
        return std::make_unique<PrintStatement>(Offset{print.start, end.end}, std::move(value));
    }

    std::unique_ptr<Statement>
    parse_expression_statement()
    {
        auto value = parse_expression();
        const auto start = value->offset;
        consume(TokenType::SEMICOLON, "Missing ';' after expression");
        const auto end = previous().offset;
        return std::make_unique<ExpressionStatement>(Offset{start.start, end.end}, std::move(value));
    }

    std::unique_ptr<Expression>
    parse_expression()
    {
        return parse_assignment();
    }

    std::unique_ptr<Expression>
    parse_assignment()
    {
        auto expr = parse_equality();

        if(match({TokenType::EQUAL}))
        {
            auto& equals = previous();
            auto rhs = parse_assignment();

            if(expr->get_type() == ExpressionType::variable_expression)
            {
                const auto name = static_cast<VariableExpression*>(expr.get())->name;
                return std::make_unique<AssignExpression>(Offset{expr->offset.start, rhs->offset.end}, name, expr->offset, std::move(rhs));
            }

            error(equals, "Invalid assignment target.");
        }

        return expr;
    }

    std::unique_ptr<Expression>
    parse_equality()
    {
        auto expr = parse_comparison();

        while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_comparison();
            const auto end = right->offset;
            expr = std::make_unique<BinaryExpression>(Offset{start.start, end.end}, std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expression>
    parse_comparison()
    {
        auto expr = parse_term();

        while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_term();
            const auto end = right->offset;
            expr = std::make_unique<BinaryExpression>(Offset{start.start, end.end}, std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expression>
    parse_term()
    {
        auto expr = parse_factor();

        while (match({TokenType::MINUS, TokenType::PLUS}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_factor();
            const auto end = right->offset;
            expr = std::make_unique<BinaryExpression>(Offset{start.start, end.end}, std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expression>
    parse_factor()
    {
        auto expr = parse_unary();

        while (match({TokenType::SLASH, TokenType::STAR}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_unary();
            const auto end = right->offset;
            expr = std::make_unique<BinaryExpression>(Offset{start.start, end.end}, std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::unique_ptr<Expression>
    parse_unary()
    {
        if (match({TokenType::BANG, TokenType::MINUS}))
        {
            auto& op = previous();
            auto right = parse_unary();
            return std::make_unique<UnaryExpression>(Offset{op.offset.start, right->offset.end}, op.type, op.offset, std::move(right));
        }

        return parse_primary();
    }

    std::unique_ptr<Expression>
    parse_primary()
    {
        if (match({TokenType::FALSE})) { return std::make_unique<LiteralExpression>(previous().offset, std::make_unique<Bool>(false)); }
        if (match({TokenType::TRUE})) { return std::make_unique<LiteralExpression>(previous().offset, std::make_unique<Bool>(true)); }
        if (match({TokenType::NIL})) { return std::make_unique<LiteralExpression>(previous().offset, std::make_unique<Nil>()); }

        if (match({TokenType::NUMBER, TokenType::STRING}))
        {
            auto& prev = previous();
            return std::make_unique<LiteralExpression>(prev.offset, std::move(prev.literal));
        }

        if (match({TokenType::IDENTIFIER}))
        {
            auto& prev = previous();
            return std::make_unique<VariableExpression>(prev.offset, std::string(prev.lexeme));
        }

        if (match({TokenType::LEFT_PAREN}))
        {
            const Offset left_paren = previous().offset;
            auto expr = parse_expression();
            consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
            const Offset right_paren = previous().offset;
            return std::make_unique<GroupingExpression>(Offset{left_paren.start, right_paren.end}, std::move(expr));
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
        if (is_at_end())
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
        if (is_at_end() == false)
        {
            current += 1;
        }

        return previous();
    }

    bool
    is_at_end() 
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

        while (is_at_end() == false)
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

std::unique_ptr<Program>
parse_program(std::vector<Token>& tokens, ErrorHandler* error_handler)
{
    try
    {
        auto parser = Parser{tokens, error_handler};
        return parser.parse_program();
    }
    catch(const ParseError&)
    {
        return nullptr;
    }
}

}
