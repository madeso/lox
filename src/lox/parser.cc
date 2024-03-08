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


std::string
token_to_string(const Token& tok)
{
    return "{}"_format
    (
        tokentype_to_string(tok.type)
    );
}


struct Parser
{
    std::vector<Token>& tokens;
    ErrorHandler* error_handler;
    std::size_t current = 0;
    int error_count = 0;

    u64 next_statement_uid = 0;
    u64 next_expression_uid = 0;


    // --------------------------------------------------------------------------------------------
    // constructor

    explicit Parser(std::vector<Token>& t, ErrorHandler* eh)
        : tokens(t)
        , error_handler(eh)
    {
    }

    // --------------------------------------------------------------------------------------------
    // util functions

    StatementId new_stmt() { return {next_statement_uid++}; }
    ExpressionId new_expr() { return {next_expression_uid++}; }


    // --------------------------------------------------------------------------------------------
    // parser

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
            if(match({TokenType::CLASS}))
            {
                return parse_class_declaration();
            }
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
    parse_class_declaration()
    {
        const auto name = consume(TokenType::IDENTIFIER, "Expected class name").lexeme;
        const auto start = previous_offset();

        std::shared_ptr<VariableExpression> superclass;
        if(match({TokenType::COLON}))
        {
            auto& id = consume(TokenType::IDENTIFIER, "Expected superclass name");
            superclass = std::make_shared<VariableExpression>(id.offset, new_expr(), std::string(id.lexeme));
        }

        consume(TokenType::LEFT_BRACE, "Expected { before class body");


        std::vector<std::shared_ptr<FunctionStatement>> methods;
        std::vector<std::shared_ptr<FunctionStatement>> static_methods;
        std::vector<std::shared_ptr<VarStatement>> members;
        while(check(TokenType::RIGHT_BRACE) == false && is_at_end() == false)
        {
            const auto is_static = match({TokenType::STATIC});

            if(match({TokenType::FUN}))
            {
                auto method = parse_function_or_method("method");
                if(is_static)
                {
                    static_methods.emplace_back(std::move(method));
                }
                else
                {
                    methods.emplace_back(std::move(method));
                }
            }
            else if(match({TokenType::VAR}))
            {
                assert(is_static == false && "todo: support static variables");
                auto mem = parse_var_declaration();
                members.emplace_back(std::move(mem));
            }
            else
            {
                auto& next = peek();
                throw error(next.offset, "Expected fun or var but found {}"_format(token_to_string(next)));
            }
        }

        consume(TokenType::RIGHT_BRACE, "Expected } after class body");

        const auto end = previous_offset();
        return std::make_shared<ClassStatement>
        (
            offset_start_end(start, end),
            new_stmt(),
            std::string(name),
            std::move(superclass),
            std::move(members),
            std::move(methods),
            std::move(static_methods)
        );
    }

    std::shared_ptr<FunctionStatement>
    parse_function_or_method(std::string_view kind)
    {
        const auto name = consume(TokenType::IDENTIFIER, "Expected {} name"_format(kind)).lexeme;
        const auto start = previous_offset();

        consume(TokenType::LEFT_PAREN, "Expect '(' after {} name"_format(kind));
        const auto params_start = previous_offset();
        std::vector<std::string> params;
        if(check(TokenType::RIGHT_PAREN) == false)
        {
            do
            {
                params.emplace_back(consume(TokenType::IDENTIFIER, "Expect parameter name").lexeme);
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters");
        const auto params_end = previous_offset();

        if (params.size() >= 255)
        {
            error(offset_start_end(params_start, params_end), "Can't have more than 255 parameters.");
        }

        consume(TokenType::LEFT_BRACE, "Expect '{{' before {} body"_format(kind));
        auto body = parse_block_to_statements();
        const auto end = previous_offset();
        return std::make_shared<FunctionStatement>(offset_start_end(start, end), new_stmt(), std::string(name), std::move(params), std::move(body));
    }

    std::shared_ptr<VarStatement>
    parse_var_declaration()
    {
        const auto start = previous_offset();
        auto& name = consume(TokenType::IDENTIFIER, "Expected variable name");

        std::shared_ptr<Expression> initializer = nullptr;

        if(match({TokenType::EQUAL}))
        {
            initializer = parse_expression();
        }

        consume_semicolon("print statement");
        const auto end = previous_offset();
        return std::make_shared<VarStatement>(offset_start_end(start, end), new_stmt(), std::string(name.lexeme), std::move(initializer));
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
        const auto start = previous_offset();

        std::shared_ptr<Expression> value;
        if(check(TokenType::SEMICOLON) == false)
        {
            value = parse_expression();
        }

        consume(TokenType::SEMICOLON, "Expected ';' after return value");
        const auto end = previous_offset();
        return std::make_shared<ReturnStatement>(offset_start_end(start, end), new_stmt(), std::move(value));
    }

    std::shared_ptr<Statement>
    parse_for_statement()
    {
        const auto start = previous_offset();
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
        const auto end = previous_offset();

        if(increment != nullptr)
        {
            const auto io = increment->offset;
            std::vector<std::shared_ptr<Statement>> statements;
            statements.emplace_back(std::move(body));
            statements.emplace_back(std::make_shared<ExpressionStatement>(io, new_stmt(), std::move(increment)));
            body = std::make_shared<BlockStatement>(offset_start_end(io, end), new_stmt(), std::move(statements));
        }

        {
            const auto co_start = condition == nullptr ? body->offset.start : condition->offset.start;
            if(condition == nullptr)
            {
                condition = std::make_shared<LiteralExpression>(Offset{nullptr, 0}, new_expr(), make_bool(true));
            }
            body = std::make_shared<WhileStatement>(Offset{end.source, co_start, end.end}, new_stmt(), std::move(condition), std::move(body));
        }

        if(initializer != nullptr)
        {
            std::vector<std::shared_ptr<Statement>> statements;
            statements.emplace_back(std::move(initializer));
            statements.emplace_back(std::move(body));
            body = std::make_shared<BlockStatement>(offset_start_end(start, end), new_stmt(), std::move(statements));
        }
        
        return body;
    }

    std::shared_ptr<Statement>
    parse_while_statement()
    {
        const auto start = previous_offset();
        consume(TokenType::LEFT_PAREN, "Expected '(' after for");
        
        auto condition = parse_expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after for condition");

        auto body = parse_statement();
        const auto end = previous_offset();
        
        return std::make_shared<WhileStatement>(offset_start_end(start, end), new_stmt(), std::move(condition), std::move(body));
    }

    std::shared_ptr<Statement>
    parse_if_statement()
    {
        const auto start = previous_offset();
        consume(TokenType::LEFT_PAREN, "Expected '(' after if");
        auto condition = parse_expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after if condition");

        auto then_branch = parse_statement();
        std::shared_ptr<Statement> else_branch = nullptr;

        if(match({TokenType::ELSE}))
        {
            else_branch = parse_statement();
        }

        const auto end = previous_offset();
        
        return std::make_shared<IfStatement>(offset_start_end(start, end), new_stmt(), std::move(condition), std::move(then_branch), std::move(else_branch));
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
        auto start = previous_offset();

        auto statements = parse_block_to_statements();

        auto end = previous_offset();
        return std::make_shared<BlockStatement>(offset_start_end(start, end), new_stmt(), std::move(statements));
    }

    std::shared_ptr<Statement>
    parse_print_statement()
    {
        const auto print = previous_offset();
        auto value = parse_expression();
        consume_semicolon("print statement");
        const auto end = previous_offset();
        return std::make_shared<PrintStatement>(offset_start_end(print, end), new_stmt(), std::move(value));
    }

    std::shared_ptr<Statement>
    parse_expression_statement()
    {
        auto value = parse_expression();
        const auto start = value->offset;
        consume_semicolon("expression");
        const auto end = previous_offset();
        return std::make_shared<ExpressionStatement>(offset_start_end(start, end), new_stmt(), std::move(value));
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

            // todo(Gustav): is new_expr needed here or can we reuse the existing expression id?

            if(expr->get_type() == ExpressionType::variable_expression)
            {
                const auto name = static_cast<VariableExpression*>(expr.get())->name;
                return std::make_shared<AssignExpression>
                (
                    offset_start_end(expr->offset, rhs->offset), new_expr(),
                    name, expr->offset, std::move(rhs)
                );
            }
            else if(expr->get_type() == ExpressionType::getproperty_expression)
            {
                auto* set = static_cast<GetPropertyExpression*>(expr.get());
                return std::make_shared<SetPropertyExpression>
                (
                    offset_start_end(expr->offset, rhs->offset), new_expr(), 
                    std::move(set->object), set->name, std::move(rhs)
                );
            }
            else if(expr->get_type() == ExpressionType::getindex_expression)
            {
                auto* set = static_cast<GetIndexExpression*>(expr.get());
                return std::make_shared<SetIndexExpression>
                (
                    offset_start_end(expr->offset, rhs->offset), new_expr(), 
                    std::move(set->object), std::move(set->index), std::move(rhs)
                );
            }

            error(offset_for_error(equals), "Invalid assignment target.");
        }
        else if(match({TokenType::PLUSEQ, TokenType::MINUSEQ, TokenType::STAREQ, TokenType::SLASHEQ}))
        {
            auto& op = previous();
            auto rhs = parse_assignment();

            const TokenType bin_op = [op]() {
                switch(op.type)
                {
                case TokenType::PLUSEQ:  return TokenType::PLUS;
                case TokenType::MINUSEQ: return TokenType::MINUS;
                case TokenType::STAREQ:  return TokenType::STAR;
                case TokenType::SLASHEQ: return TokenType::SLASH;
                default: assert(false && "unhandled eq"); return TokenType::PLUS;
                }
            } ();

            const auto full_offset = offset_start_end(expr->offset, rhs->offset);
            auto exprc = expr;
            rhs = std::make_shared<BinaryExpression>(full_offset, new_expr(), std::move(exprc), bin_op, op.offset, std::move(rhs));

            if(expr->get_type() == ExpressionType::variable_expression)
            {
                const auto name = static_cast<VariableExpression*>(expr.get())->name;
                return std::make_shared<AssignExpression>
                (
                    full_offset, new_expr(),
                    name, expr->offset, std::move(rhs)
                );
            }
            else if(expr->get_type() == ExpressionType::getproperty_expression)
            {
                auto* set = static_cast<GetPropertyExpression*>(expr.get());
                return std::make_shared<SetPropertyExpression>
                (
                    full_offset, new_expr(), 
                    std::move(set->object), set->name, std::move(rhs)
                );
            }
            else if(expr->get_type() == ExpressionType::getindex_expression)
            {
                auto* set = static_cast<GetIndexExpression*>(expr.get());
                return std::make_shared<SetIndexExpression>
                (
                    full_offset, new_expr(), 
                    std::move(set->object), std::move(set->index), std::move(rhs)
                );
            }

            error(offset_for_error(op), "Invalid assignment target.");
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
            left = std::make_shared<LogicalExpression>(offset_start_end(start, end), new_expr(), std::move(left), op.type, std::move(right));
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
            left = std::make_shared<LogicalExpression>(offset_start_end(start, end), new_expr(), std::move(left), op.type, std::move(right));
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
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), new_expr(), std::move(expr), op.type, op.offset, std::move(right));
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
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), new_expr(), std::move(expr), op.type, op.offset, std::move(right));
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
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), new_expr(), std::move(expr), op.type, op.offset, std::move(right));
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
            expr = std::make_shared<BinaryExpression>(offset_start_end(start, end), new_expr(), std::move(expr), op.type, op.offset, std::move(right));
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
            return std::make_shared<UnaryExpression>(offset_start_end(op.offset, right->offset), new_expr(), op.type, op.offset, std::move(right));
        }

        return parse_call();
    }

    std::shared_ptr<Expression>
    parse_call()
    {
        std::shared_ptr<Expression> expr;
        
        if(match({TokenType::NEW}))
        {
            consume(TokenType::IDENTIFIER, "expected name of class");
            auto& prev = previous();
            expr = std::make_shared<VariableExpression>(prev.offset, new_expr(), std::string(prev.lexeme));

            while (true)
            {
                if (match({TokenType::LEFT_PAREN}))
                {
                    auto call_ptr = finish_parsing_of_call(std::move(expr));
                    assert(call_ptr->get_type() == ExpressionType::call_expression);
                    auto call = std::static_pointer_cast<CallExpression>(call_ptr);
                    // transform from CallExpression to ConstructorExpression
                    expr = std::make_shared<ConstructorExpression>
                    (
                        call->offset, call->uid, std::move(call->callee), std::move(call->arguments)
                    );
                    break;
                }
                else if(match({TokenType::DOT}))
                {
                    const auto& name = consume(TokenType::IDENTIFIER, "Expected property name after '.'");
                    expr = std::make_shared<GetPropertyExpression>(name.offset, new_expr(), std::move(expr), std::string(name.lexeme));
                }
                else
                {
                    error_count += 1;
                    error_handler->on_error(peek().offset, "Invalid token in package evaluation");
                    break;
                }
            }
        }
        else
        {
            expr = parse_primary();
        }

        while (true)
        {
            if (match({TokenType::LEFT_PAREN}))
            {
                expr = finish_parsing_of_call(std::move(expr));
            }
            else if(match({TokenType::DOT}))
            {
                const auto& name = consume(TokenType::IDENTIFIER, "Expected property name after '.'");
                expr = std::make_shared<GetPropertyExpression>(name.offset, new_expr(), std::move(expr), std::string(name.lexeme));
            }
            else if(match({TokenType::LEFT_BRACKET}))
            {
                // todo(Gustav): copy to new parsing
                const auto start = previous_offset();
                auto index = parse_expression();
                consume(TokenType::RIGHT_BRACKET, "expected ']' after array indexer");
                const auto end = previous_offset();
                expr = std::make_shared<GetIndexExpression>(offset_start_end(start, end), new_expr(), std::move(expr), std::move(index));
            }
            else
            {
                break;
            }
        }

        return expr;
    }

    std::vector<std::shared_ptr<Expression>>
    parse_arguments()
    {
        std::vector<std::shared_ptr<Expression>> arguments;
        if (check(TokenType::RIGHT_PAREN) == false)
        {
            do
            {
                arguments.emplace_back(parse_expression());
            } while(match({TokenType::COMMA}));
        }
        consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments");

        return arguments;
    }

    void
    validate_argument_size(std::size_t count, const Offset& off)
    {
        if(count > max_number_of_arguments)
        {
            error_count += 1;
            error_handler->on_error(off, "More than {} number of arguments, passed {}"_format(max_number_of_arguments, count));
        }
    }

    std::shared_ptr<Expression>
    finish_parsing_of_call(std::shared_ptr<Expression>&& callee)
    {
        const auto start = previous_offset();
        
        auto arguments = parse_arguments();
        const auto end = previous_offset();

        const auto off = offset_start_end(start, end);

        validate_argument_size(arguments.size(), off);

        return std::make_shared<CallExpression>(off, new_expr(), std::move(callee), std::move(arguments));
    }

    std::shared_ptr<Expression>
    parse_primary()
    {
        if (match({TokenType::FALSE})) { return std::make_shared<LiteralExpression>(previous_offset(), new_expr(), make_bool(false)); }
        if (match({TokenType::TRUE})) { return std::make_shared<LiteralExpression>(previous_offset(), new_expr(), make_bool(true)); }
        if (match({TokenType::NIL})) { return std::make_shared<LiteralExpression>(previous_offset(), new_expr(), make_nil()); }

        if(match({TokenType::LEFT_BRACKET}))
        {
            return parse_array();
        }

        if (match({TokenType::NUMBER_INT, TokenType::NUMBER_FLOAT, TokenType::STRING}))
        {
            auto& prev = previous();
            return std::make_shared<LiteralExpression>(prev.offset, new_expr(), std::move(prev.literal));
        }

        if(match({TokenType::SUPER}))
        {
            const auto start = previous_offset();

            if(match({TokenType::LEFT_PAREN}))
            {
                auto arguments = parse_arguments();
                const auto end = previous_offset();
                const auto off = offset_start_end(start, end);
                validate_argument_size(arguments.size(), off);

                return std::make_shared<SuperConstructorCallExpression>(off, new_expr(), std::move(arguments));
            }

            consume(TokenType::DOT, "Expected '.' after 'super' keyword");
            auto name = consume(TokenType::IDENTIFIER, "Expected superclass property").lexeme;
            const auto end = previous_offset();
            return std::make_shared<SuperExpression>(offset_start_end(start, end), new_expr(), std::string(name));
        }

        if( match({TokenType::THIS}))
        {
            auto& prev = previous();
            return std::make_shared<ThisExpression>(prev.offset, new_expr());
        }

        if (match({TokenType::IDENTIFIER}))
        {
            auto& prev = previous();
            return std::make_shared<VariableExpression>(prev.offset, new_expr(), std::string(prev.lexeme));
        }

        if (match({TokenType::LEFT_PAREN}))
        {
            const Offset left_paren = previous_offset();
            auto expr = parse_expression();
            consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
            const Offset right_paren = previous_offset();
            return std::make_shared<GroupingExpression>(offset_start_end(left_paren, right_paren), new_expr(), std::move(expr));
        }

        throw error(offset_for_range_error(previous_offset(), peek()), "Expected expression.");
    }

    std::shared_ptr<Expression>
    parse_array()
    {
        const Offset start = previous_offset();
        std::vector<std::shared_ptr<Expression>> values;
        if (check(TokenType::RIGHT_BRACKET) == false)
        {
            do
            {
                values.emplace_back(parse_expression());
            } while(match({TokenType::COMMA}));
        }

        const auto end = consume(TokenType::RIGHT_BRACKET, "Expect ']' to end array").offset;

        return std::make_shared<ArrayExpression>(offset_start_end(start, end), new_expr(), std::move(values));
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
        assert(current != 0);
        return tokens[current - 1];
    }

    Offset
    previous_offset()
    {
        if(current == 0)
        {
            return {tokens[current].offset.source, 0};
        }
        else
        {
            return previous().offset;
        }
    }

    void
    consume_semicolon(const std::string& after)
    {
        consume
        (
            TokenType::SEMICOLON,
            "Missing ';' after {}"_format(after),
            {previous_offset()}
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
