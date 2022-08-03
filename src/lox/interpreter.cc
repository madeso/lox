#include "lox/interpreter.h"

#include <cassert>


#include "lox/ast.h"
#include "lox/errorhandler.h"
#include "lox/program.h"
#include "lox/environment.h"


namespace lox { namespace
{
struct MainInterpreter;

void
execute_statements_with_environment
(
    MainInterpreter* main,
    const std::vector<std::shared_ptr<Statement>>& statements,
    Environment* environment
);

Environment*
get_global_environment(MainInterpreter* main);
}}







namespace lox
{
struct Arguments
{
    MainInterpreter* interpreter;
    std::vector<std::shared_ptr<Object>> arguments;
};
}





namespace lox { namespace
{

struct CallError
{
    std::string error;
    explicit CallError(const std::string& e);
};


struct RuntimeError
{
};


struct ScriptFunction : Callable
{
    FunctionStatement declaration;

    explicit ScriptFunction(const FunctionStatement& f) : declaration(f) {}

    std::string
    to_string() const override
    {
        return "<fn {}>"_format(declaration.name);
    }

    std::shared_ptr<Object>
    call(const Arguments& arguments) override
    {
        verify_number_of_arguments(arguments, declaration.params.size());
        
        auto environment = Environment{ get_global_environment(arguments.interpreter) };

        for(std::size_t param_index = 0; param_index < declaration.params.size(); param_index += 1)
        {
            environment.define(declaration.params[param_index], arguments.arguments[param_index]);
        }

        execute_statements_with_environment(arguments.interpreter, declaration.body, &environment);
        return make_nil();
    }
};


void
check_single_number_operand
(
    ErrorHandler* error_handler,
    const Offset& op_offset,
    std::shared_ptr<Object> object,
    const Offset& object_offset
)
{
    const auto type = object->get_type();
    if(type == ObjectType::number) { return; }

    error_handler->on_error(op_offset, "operand must be a number");
    error_handler->on_error(object_offset, "This evaluated to {}"_format(objecttype_to_string(type)));
    throw RuntimeError();
}


void
check_binary_number_operand
(
    ErrorHandler* error_handler,
    const Offset& op_offset,
    std::shared_ptr<Object> lhs,
    std::shared_ptr<Object> rhs,
    const Offset& lhs_offset,
    const Offset& rhs_offset
)
{
    const auto lhs_type = lhs->get_type();
    const auto rhs_type = rhs->get_type();
    if(lhs_type == ObjectType::number && rhs_type == ObjectType::number) { return; }

    error_handler->on_error(op_offset, "operands must be a numbers");
    error_handler->on_note(lhs_offset, "left hand evaluated to {}"_format(objecttype_to_string(lhs_type)));
    error_handler->on_note(rhs_offset, "right hand evaluated to {}"_format(objecttype_to_string(rhs_type)));
    throw RuntimeError();
}


void
check_binary_number_or_string_operands
(
    ErrorHandler* error_handler,
    const Offset& op_offset,
    std::shared_ptr<Object> lhs,
    std::shared_ptr<Object> rhs,
    const Offset& lhs_offset,
    const Offset& rhs_offset
)
{
    const auto lhs_type = lhs->get_type();
    const auto rhs_type = rhs->get_type();
    if
    (
        (lhs_type == ObjectType::number && rhs_type == ObjectType::number)
        ||
        (lhs_type == ObjectType::string && rhs_type == ObjectType::string)
    )
    { return; }

    error_handler->on_error(op_offset, "operands must be a numbers or strings");
    error_handler->on_note(lhs_offset, "left hand evaluated to {}"_format(objecttype_to_string(lhs_type)));
    error_handler->on_note(rhs_offset, "right hand evaluated to {}"_format(objecttype_to_string(rhs_type)));
    throw RuntimeError();
}


float
get_number(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::number);
    return static_cast<Number*>(o.get())->value;
}


std::string
get_string(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::string);
    return static_cast<String*>(o.get())->value;
}


bool
get_bool(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::boolean);
    return static_cast<Bool*>(o.get())->value;
}


bool
is_truthy(const Object& o)
{
    switch(o.get_type())
    {
    case ObjectType::nil:
        return false;
    case ObjectType::boolean:
        return static_cast<const Bool&>(o).value;
    default:
        return true;
    }
}


bool
is_equal(std::shared_ptr<Object> lhs, std::shared_ptr<Object> rhs)
{
    if(lhs->get_type() != rhs->get_type())
    {
        return false;
    }
    
    switch(lhs->get_type())
    {
    case ObjectType::nil: return true;
    case ObjectType::number:
        return get_number(lhs) == get_number(rhs);
    case ObjectType::boolean:
        return get_bool(lhs) == get_bool(rhs);
    case ObjectType::string:
        return get_string(lhs) == get_string(rhs);
    default:
        assert(false && "unhandled type in is_equal(...)");
        return false;
    }
}


struct EnvironmentRaii
{
    Environment** parent;
    Environment* last_child;

    EnvironmentRaii(Environment** p, Environment* child) : parent(p), last_child(*p)
    {
        *parent = child;
    }

    ~EnvironmentRaii()
    {
        *parent = last_child;
    }

    EnvironmentRaii(EnvironmentRaii&&) = delete;
    void operator=(EnvironmentRaii&&) = delete;
    EnvironmentRaii(const EnvironmentRaii&) = delete;
    void operator=(const EnvironmentRaii&) = delete;
};


struct MainInterpreter : ExpressionObjectVisitor, StatementVoidVisitor
{
    ErrorHandler* error_handler;
    Environment* global_environment;
    Environment* current_environment;
    const std::function<void (std::string)>& on_line;

    //-------------------------------------------------------------------------
    // constructor

    explicit MainInterpreter(Environment* ge, ErrorHandler* eh, const std::function<void (std::string)>& ol)
        : error_handler(eh)
        , global_environment(ge)
        , on_line(ol)
    {
        current_environment = global_environment;
    }

    //---------------------------------------------------------------------------------------------
    // util functions

    std::shared_ptr<Object>
    get_var(Environment& environment, const std::string& name, const Offset& off)
    {
        auto var = environment.get_or_null(name);
        if(var == nullptr)
        {
            error_handler->on_error(off, "Undefined variable");
            throw RuntimeError{};
        }
        return var;
    }

    void
    set_var(Environment& environment, const std::string& name, const Offset& off, std::shared_ptr<Object> object)
    {
        const bool was_set = environment.set_or_false(name, object);
        if(was_set == false)
        {
            error_handler->on_error(off, "Undefined variable {}"_format(name));
            throw RuntimeError{};
        }
    }

    //---------------------------------------------------------------------------------------------
    // statements

    void
    on_if_statement(const IfStatement& x) override
    {
        if(is_truthy(*x.condition->accept(this)))
        {
            x.then_branch->accept(this);
        }
        else
        {
            if(x.else_branch != nullptr)
            {
                x.else_branch->accept(this);
            }
        }
    }

    void
    on_block_statement(const BlockStatement& x) override
    {
        Environment block_env { current_environment };
        execute_statements_with_environment(x.statements, &block_env);
    }

    void
    on_function_statement(const FunctionStatement& x) override
    {
        current_environment->define(x.name, std::make_shared<ScriptFunction>(x));
    }

    void
    execute_statements_with_environment(const std::vector<std::shared_ptr<Statement>>& statements, Environment* environment)
    {
        auto raii = EnvironmentRaii{&current_environment, environment};
        for(const auto& st: statements)
        {
            st->accept(this);
        }
    }

    void
    on_while_statement(const WhileStatement& x) override
    {
        while(is_truthy(*x.condition->accept(this)))
        {
            x.body->accept(this);
        }
    }

    void
    on_var_statement(const VarStatement& x) override
    {
        // todo(Gustav): make usage of unitialized value an error
        std::shared_ptr<Object> value = x.initializer != nullptr
            ? x.initializer->accept(this)
            : std::make_shared<Nil>()
            ;
        
        current_environment->define(x.name, value);
    }

    void
    on_print_statement(const PrintStatement& x) override
    {
        auto value = x.expression->accept(this);
        on_line(value->to_string());
    }

    void
    on_expression_statement(const ExpressionStatement& x) override
    {
        x.expression->accept(this);
    }


    //---------------------------------------------------------------------------------------------
    // expressions

    std::shared_ptr<Object>
    on_call_expression(const CallExpression& x) override
    {
        auto callee = x.callee->accept(this);

        if(callee->get_type() != ObjectType::callable)
        {
            error_handler->on_error
            (
                x.callee->offset,
                "{} is not a callable, evaluates to {}"_format
                (
                    objecttype_to_string(callee->get_type()),
                    callee->to_string()
                )
            );
            error_handler->on_note(x.offset, "call occured here");
            throw RuntimeError{};
        }

        std::vector<std::shared_ptr<Object>> arguments;
        for(auto& argument : x.arguments)
        { 
            arguments.emplace_back(argument->accept(this));
        }

        Callable* function = static_cast<Callable*>(callee.get());
        try
        {
            auto return_value = function->call({this, arguments});
            return return_value;
        }
        catch(const CallError& err)
        {
            error_handler->on_error(x.offset, err.error);
            error_handler->on_note(x.callee->offset, "called with {} arguments"_format(x.arguments.size()));
            for(std::size_t argument_index = 0; argument_index < x.arguments.size(); argument_index += 1)
            {

                error_handler->on_note
                (
                    x.arguments[argument_index]->offset,
                    "argument {} evaluated to {}: {}"_format
                    (
                        argument_index + 1,
                        objecttype_to_string(arguments[argument_index]->get_type()),
                        arguments[argument_index]->to_string()
                    )
                );
            }
            throw RuntimeError{};
        }
    }

    std::shared_ptr<Object>
    on_logical_expression(const LogicalExpression& x) override
    {
        auto left = x.left->accept(this);

        switch(x.op)
        {
        case TokenType::OR:
            if (is_truthy(*left))
            {
                return left;
            }
            break;
        case TokenType::AND:
            if (!is_truthy(*left))
            {
                return left;
            }
            break;
        default:
            assert(false && "unknown logical operator in on_logical_expression(...)");
            break;
        }

        return x.right->accept(this);
    }

    std::shared_ptr<Object>
    on_assign_expression(const AssignExpression& x) override
    {
        auto value = x.value->accept(this);
        set_var(*current_environment, x.name, x.name_offset, value);
        return value;
    }

    std::shared_ptr<Object>
    on_variable_expression(const VariableExpression& x) override
    {
        return get_var(*current_environment, x.name, x.offset);
    }

    std::shared_ptr<Object>
    on_binary_expression(const BinaryExpression& x) override
    {
        // todo(Gustav): make binary operators more flexible string*int should duplicate string
        // todo(Gustav): string + (other) should stringify other?
        auto left = x.left->accept(this);
        auto right = x.right->accept(this);

        switch(x.op)
        {
        case TokenType::MINUS:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Number>( get_number(left) - get_number(right) );
        case TokenType::SLASH:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Number>( get_number(left) / get_number(right) );
        case TokenType::STAR:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Number>( get_number(left) * get_number(right) );
        case TokenType::PLUS:
            check_binary_number_or_string_operands(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            if(left->get_type() == ObjectType::number && right->get_type() == ObjectType::number)
            {
                return std::make_shared<Number>( get_number(left) + get_number(right) );
            }
            else if(left->get_type() == ObjectType::string && right->get_type() == ObjectType::string)
            {
                return std::make_shared<String>( get_string(left) + get_string(right) );
            }
            else
            {
                assert(false && "interpreter error");
                return nullptr;
            }
        case TokenType::LESS:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Bool>( get_number(left) < get_number(right) );
        case TokenType::LESS_EQUAL:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Bool>( get_number(left) <= get_number(right) );
        case TokenType::GREATER:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Bool>( get_number(left) > get_number(right) );
        case TokenType::GREATER_EQUAL:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return std::make_shared<Bool>( get_number(left) >= get_number(right) );
        case TokenType::BANG_EQUAL:
            return std::make_shared<Bool>( !is_equal(left, right) );
        case TokenType::EQUAL_EQUAL:
            return std::make_shared<Bool>( is_equal(left, right) );
        default:
            assert(false && "unhandled token in Interpreter::on_binary_expression(...)");
            return nullptr;
        }
    }

    std::shared_ptr<Object>
    on_grouping_expression(const GroupingExpression& x) override
    {
        return x.expression->accept(this);
    }

    std::shared_ptr<Object>
    on_literal_expression(const LiteralExpression& x) override
    {
        return x.value;
    }

    std::shared_ptr<Object>
    on_unary_expression(const UnaryExpression& x) override
    {
        auto right = x.right->accept(this);
        switch(x.op)
        {
        case TokenType::BANG:
            return std::make_shared<Bool>(!is_truthy(*right));
        case TokenType::MINUS:
            check_single_number_operand(error_handler, x.op_offset, right, x.right->offset);
            return std::make_shared<Number>(-get_number(right));
        default:
            assert(false && "unreachable");
            return std::make_shared<Nil>();
        }
    }


};


void
execute_statements_with_environment(MainInterpreter* main, const std::vector<std::shared_ptr<Statement>>& statements, Environment* environment)
{
    main->execute_statements_with_environment(statements, environment);
}


Environment*
get_global_environment(MainInterpreter* main)
{
    return main->global_environment;
}


}}


namespace lox
{


void
verify_number_of_arguments(const Arguments& args, u64 arity)
{
    if(arity != args.arguments.size())
    {
        throw CallError
        {
            "Expected {} arguments but got {}"_format
            (
                arity,
                args.arguments.size()
            )
        };
    }
}


CallError::CallError(const std::string& err)
    : error(err)
{
}



Interpreter::Interpreter()
    : global_environment(nullptr)
{
}


bool
interpret(Interpreter* main_interpreter, Program& program, ErrorHandler* error_handler, const std::function<void (std::string)>& on_line)
{
    auto interpreter = MainInterpreter{&main_interpreter->global_environment, error_handler, on_line};
    try
    {
        for(auto& s: program.statements)
        {
            s->accept(&interpreter);
        }
        return true;
    }
    catch (const RuntimeError&)
    {
        return false;
    }
}


}

