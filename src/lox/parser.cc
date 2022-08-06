#include "lox/parser.h"

#include "lox/errorhandler.h"
#include "lox/config.h"


namespace lox { namespace {


struct ParseError{};



Offset
offset_start_end(const Offset& start, const Offset& end)
{
    assert(start.source == end.source);
    return {start.source, start.start, end.end};
}


Offset
offset_end_start(const Offset& start, const Offset& end)
{
    assert(start.source == end.source);
    return {start.source, start.end, end.start};
}


Offset
offset_for_error(const Token& token)
{
    if (token.type == TokenType::EOF)
    {
        return {token.offset.source, token.offset.start};
    }
    else
    {
        return token.offset;
    }
}


Offset
offset_for_range_error(const Offset& previous, const Token& token)
{
    if (token.type == TokenType::EOF)
    {
        return {token.offset.source, token.offset.start};
    }
    else
    {
        return offset_end_start(previous, token.offset);
    }
}


struct Parser
{
    std::vector<Token>& tokens;
    ErrorHandler* error_handler;
    std::size_t current = 0;
    int error_count = 0;

    explicit Parser(std::vector<Token>& t, ErrorHandler* eh)
        : tokens(t)
        , error_handler(eh)
    {
    }

    std::shared_ptr<Program>
    parse_program()
    {
        auto program = std::make_shared<Program>();

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

    std::shared_ptr<Statement>
    parse_declaration_or_null()
    {
        try
        {
            if(match({TokenType::FUN}))
            {
                return parse_function_or_method("function");
            }
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

    std::shared_ptr<Statement>
    parse_function_or_method(std::string_view kind)
    {
        const auto name = consume(TokenType::IDENTIFIER, "Expected {} name"_format(kind)).lexeme;
        const auto start = previous().offset;

        consume(TokenType::LEFT_PAREN, "Expect '(' after {} name"_format(kind));
        const auto params_start = previous().offset;
        std::vector<std::string> params;
        if(check(TokenType::RIGHT_PAREN) == false)
        {
            do
            {
                params.emplace_back(consume(TokenType::IDENTIFIER, "Expect parameter name").lexeme);
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters");
        const auto params_end = previous().offset;

        if (params.size() >= 255)
        {
            error(offset_start_end(params_start, params_end), "Can't have more than 255 parameters.");
        }

        consume(TokenType::LEFT_BRACE, "Expect '{{' before {} body"_format(kind));
        auto body = parse_block_to_statements();
        const auto end = previous().offset;
        return std::make_shared<FunctionStatement>(offset_start_end(start, end), std::string(name), std::move(params), std::move(body));
    }

    std::shared_ptr<Statement>
    parse_var_declaration()
    {
        const auto start = previous().offset;
        auto& name = consume(TokenType::IDENTIFIER, "Expected variable name");

        std::shared_ptr<Expression> initializer = nullptr;

        if(match({TokenType::EQUAL}))
        {
            initializer = parse_expression();
        }

        consume_semicolon("print statement");
        const auto end = previous().offset;
        return std::make_shared<VarStatement>(offset_start_end(start, end), std::string(name.lexeme), std::move(initializer));
    }

    std::shared_ptr<Statement>
    parse_statement()
    {
        if(match({TokenType::IF})) { return parse_if_statement(); }
        if(match({TokenType::PRINT})) { return parse_print_statement(); }
        if(match({TokenType::RETURN})) { return parse_return_statement(); }
        if(match({TokenType::WHILE})) { return parse_while_statement(); }
        if(match({TokenType::FOR})) { return parse_for_statement(); }
        if(match({TokenType::LEFT_BRACE})) { return parse_block_statement(); }

        return parse_expression_statement();
    }

    std::shared_ptr<Statement>
    parse_return_statement()
    {
        const auto start = previous().offset;

        std::shared_ptr<Expression> value;
        if(check(TokenType::SEMICOLON) == false)
        {
            value = parse_expression();
        }

        consume(TokenType::SEMICOLON, "Expected ';' after return value");
        const auto end = previous().offset;
        return std::make_shared<ReturnStatement>(offset_start_end(start, end), std::move(value));
    }

    std::shared_ptr<Statement>
    parse_for_statement()
    {
        const auto start = previous().offset;
        consume(TokenType::LEFT_PAREN, "Expected '(' after for");
        
        std::shared_ptr<Statement> initializer;

        if(match({TokenType::SEMICOLON}))
        {
            // pass
        }
        else if(match({TokenType::VAR}))
        {
            initializer = parse_var_declaration();
        }
        else
        {
            initializer = parse_expression_statement();
        }

        std::shared_ptr<Expression> condition;
        if(check(TokenType::SEMICOLON) == false)
        {
            condition = parse_expression();
        }
        consume(TokenType::SEMICOLON, "Expected ';' after loop confition");


        std::shared_ptr<Expression> increment;
        if(check(TokenType::RIGHT_PAREN) == false)
        {
            increment = parse_expression();
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' after for condition");

        auto body = parse_statement();
        const auto end = previous().offset;

        if(increment != nullptr)
        {
            const auto io = increment->offset;
            std::vector<std::shared_ptr<Statement>> statements;
            statements.emplace_back(std::move(body));
            statements.emplace_back(std::make_shared<ExpressionStatement>(io, std::move(increment)));
            body = std::make_shared<BlockStatement>(offset_start_end(io, end), std::move(statements));
        }

        {
            const auto co_start = condition == nullptr ? body->offset.start : condition->offset.start;
            if(condition == nullptr)
            {
                condition = std::make_shared<LiteralExpression>(Offset{nullptr, 0}, make_bool(true));
            }
            body = std::make_shared<WhileStatement>(Offset{end.source, co_start, end.end}, std::move(condition), std::move(body));
        }

        if(initializer != nullptr)
        {
            std::vector<std::shared_ptr<Statement>> statements;
            statements.emplace_back(std::move(initializer));
            statements.emplace_back(std::move(body));
            body = std::make_shared<BlockStatement>(offset_start_end(start, end), std::move(statements));
        }
        
        return body;
    }

    std::shared_ptr<Statement>
    parse_while_statement()
    {
        const auto start = previous().offset;
        consume(TokenType::LEFT_PAREN, "Expected '(' after for");
        
        auto condition = parse_expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after for condition");

        auto body = parse_statement();
        const auto end = previous().offset;
        
        return std::make_shared<WhileStatement>(offset_start_end(start, end), std::move(condition), std::move(body));
    }

    std::shared_ptr<Statement>
    parse_if_statement()
    {
        const auto start = previous().offset;
        consume(TokenType::LEFT_PAREN, "Expected '(' after if");
        auto condition = parse_expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after if condition");

        auto then_branch = parse_statement();
        std::shared_ptr<Statement> else_branch = nullptr;

        if(match({TokenType::ELSE}))
        {
            else_branch = parse_statement();
        }

        const auto end = previous().offset;
        
        return std::make_shared<IfStatement>(offset_start_end(start, end), std::move(condition), std::move(then_branch), std::move(else_branch));
    }

    std::vector<std::shared_ptr<Statement>>
    parse_block_to_statements()
    {
        std::vector<std::shared_ptr<Statement>> statements;
        while(check(TokenType::RIGHT_BRACE)==false && is_at_end() == false)
        {
            auto st = parse_declaration_or_null();
            if(st != nullptr)
            {
                statements.emplace_back(std::move(st));
            }
        }
        consume(TokenType::RIGHT_BRACE, "Expected '}' after block.");
        return statements;
    }

    std::shared_ptr<Statement>
    parse_block_statement()
    {
        auto start = previous().offset;

        auto statements = parse_block_to_statements();

        auto& end = previous().offset;
        return std::make_shared<BlockStatement>(offset_start_end(start, end), std::move(statements));
    }

    std::shared_ptr<Statement>
    parse_print_statement()
    {
        const auto print = previous().offset;
        auto value = parse_expression();
        consume_semicolon("print statement");
        const auto end = previous().offset;
        return std::make_shared<PrintStatement>(offset_start_end(print, end), std::move(value));
    }

    std::shared_ptr<Statement>
    parse_expression_statement()
    {
        auto value = parse_expression();
        const auto start = value->offset;
        consume_semicolon("expression");
        const auto end = previous().offset;
        return std::make_shared<ExpressionStatement>(offset_start_end(start, end), std::move(value));
    }

    std::shared_ptr<Expression>
    parse_expression()
    {
        return parse_assignment();
    }

    std::shared_ptr<Expression>
    parse_assignment()
    {
        auto expr = parse_or();

        if(match({TokenType::EQUAL}))
        {
            auto& equals = previous();
            auto rhs = parse_assignment();

            if(expr->get_type() == ExpressionType::variable_expression)
            {
                const auto name = static_cast<VariableExpression*>(expr.get())->name;
                return std::make_shared<AssignExpression>(offset_start_end(expr->offset, rhs->offset), name, expr->offset, std::move(rhs));
            }

            error(offset_for_error(equals), "Invalid assignment target.");
        }

        return expr;
    }

    std::shared_ptr<Expression>
    parse_or()
    {
        auto left = parse_and();
        const auto start = left->offset;

        while(match({TokenType::OR}))
        {
            auto& op = previous();
            auto right = parse_and();
            const auto end = right->offset;
            left = std::make_shared<LogicalExpression>(offset_start_end(start, end), std::move(left), op.type, std::move(right));
        }

        return left;
    }

    std::shared_ptr<Expression>
    parse_and()
    {
        auto left = parse_equality();
        const auto start = left->offset;

        while(match({TokenType::AND}))
        {
            auto& op = previous();
            auto right = parse_equality();
            const auto end = right->offset;
            left = std::make_shared<LogicalExpression>(offset_start_end(start, end), std::move(left), op.type, std::move(right));
        }

        return left;
    }

    std::shared_ptr<Expression>
    parse_equality()
    {
        auto expr = parse_comparison();

        while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_comparison();
            const auto end = right->offset;
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::shared_ptr<Expression>
    parse_comparison()
    {
        auto expr = parse_term();

        while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_term();
            const auto end = right->offset;
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::shared_ptr<Expression>
    parse_term()
    {
        auto expr = parse_factor();

        while (match({TokenType::MINUS, TokenType::PLUS}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_factor();
            const auto end = right->offset;
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::shared_ptr<Expression>
    parse_factor()
    {
        auto expr = parse_unary();

        while (match({TokenType::SLASH, TokenType::STAR}))
        {
            const auto start = expr->offset;
            auto& op = previous();
            auto right = parse_unary();
            const auto end = right->offset;
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), std::move(expr), op.type, op.offset, std::move(right));
        }

        return expr;
    }

    std::shared_ptr<Expression>
    parse_unary()
    {
        if (match({TokenType::BANG, TokenType::MINUS}))
        {
            auto& op = previous();
            auto right = parse_unary();
            return std::make_shared<UnaryExpression>(offset_start_end(op.offset, right->offset), op.type, op.offset, std::move(right));
        }

        return parse_call();
    }

    std::shared_ptr<Expression>
    parse_call()
    {
        auto expr = parse_primary();

        while (true)
        {
            if (match({TokenType::LEFT_PAREN}))
            {
                expr = finish_parsing_of_call(std::move(expr));
            }
            else
            {
                break;
            }
        }

        return expr;
    }

    std::shared_ptr<Expression>
    finish_parsing_of_call(std::shared_ptr<Expression>&& callee)
    {
        const auto start = previous().offset;
        std::vector<std::shared_ptr<Expression>> arguments;
        if (check(TokenType::RIGHT_PAREN) == false)
        {
            do
            {
                arguments.emplace_back(parse_expression());
            } while(match({TokenType::COMMA}));
        }

        const auto end = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments").offset;

        const auto off = offset_start_end(start, end);

        if(arguments.size() > max_number_of_arguments)
        {
            error_count += 1;
            error_handler->on_error(off, "More than {} number of arguments, passed {}"_format(max_number_of_arguments, arguments.size()));
        }

        return std::make_shared<CallExpression>(off, std::move(callee), std::move(arguments));
    }

    std::shared_ptr<Expression>
    parse_primary()
    {
        if (match({TokenType::FALSE})) { return std::make_shared<LiteralExpression>(previous().offset, make_bool(false)); }
        if (match({TokenType::TRUE})) { return std::make_shared<LiteralExpression>(previous().offset, make_bool(true)); }
        if (match({TokenType::NIL})) { return std::make_shared<LiteralExpression>(previous().offset, make_nil()); }

        if (match({TokenType::NUMBER, TokenType::STRING}))
        {
            auto& prev = previous();
            return std::make_shared<LiteralExpression>(prev.offset, std::move(prev.literal));
        }

        if (match({TokenType::IDENTIFIER}))
        {
            auto& prev = previous();
            return std::make_shared<VariableExpression>(prev.offset, std::string(prev.lexeme));
        }

        if (match({TokenType::LEFT_PAREN}))
        {
            const Offset left_paren = previous().offset;
            auto expr = parse_expression();
            consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
            const Offset right_paren = previous().offset;
            return std::make_shared<GroupingExpression>(offset_start_end(left_paren, right_paren), std::move(expr));
        }

        throw error(offset_for_range_error(previous().offset, peek()), "Expected expression.");
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

    void
    consume_semicolon(const std::string& after)
    {
        consume
        (
            TokenType::SEMICOLON,
            "Missing ';' after {}"_format(after),
            {previous().offset}
        );
    }

    Token&
    consume(TokenType type, const std::string& message, std::optional<Offset> offset = std::nullopt)
    {
        if(check(type))
        {
            return advance();
        }
        else
        {
            if(offset)
            {
                throw error(*offset, message);
            }
            else
            {
                throw error(offset_for_error(peek()), message);
            }
        }
    }

    ParseError
    error(const Offset& offset, const std::string& message)
    {
        report_error(offset, message);
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
    report_error(const Offset& offset, const std::string& message)
    {
        error_count += 1;
        error_handler->on_error(offset, message);
    }
};


}}


namespace lox
{

ParseResult
parse_program(std::vector<Token>& tokens, ErrorHandler* error_handler)
{
    auto parser = Parser{tokens, error_handler};
    try
    {
        auto prog = parser.parse_program();
        return {parser.error_count, prog};
    }
    catch(const ParseError&)
    {
        return {parser.error_count, nullptr};
    }
}

}
