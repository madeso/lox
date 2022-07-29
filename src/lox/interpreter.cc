#include "lox/interpreter.h"

#include <cassert>
#include <iostream>

#include "lox/ast.h"
#include "lox/errorhandler.h"
#include "lox/program.h"
#include "lox/enviroment.h"


namespace lox { namespace
{

struct RuntimeError {};


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
    error_handler->on_error(object_offset, fmt::format("This evaluated to {}", objecttype_to_string(type)));
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
    error_handler->on_note(lhs_offset, fmt::format("left hand evaluated to {}", objecttype_to_string(lhs_type)));
    error_handler->on_note(rhs_offset, fmt::format("right hand evaluated to {}", objecttype_to_string(rhs_type)));
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
    error_handler->on_note(lhs_offset, fmt::format("left hand evaluated to {}", objecttype_to_string(lhs_type)));
    error_handler->on_note(rhs_offset, fmt::format("right hand evaluated to {}", objecttype_to_string(rhs_type)));
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
        return false;
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


struct EnviromentRaii
{
    Enviroment** parent;
    Enviroment* last_child;

    EnviromentRaii(Enviroment** p, Enviroment* child) : parent(p), last_child(*p)
    {
        *parent = child;
    }

    ~EnviromentRaii()
    {
        *parent = last_child;
    }

    EnviromentRaii(EnviromentRaii&&) = delete;
    void operator=(EnviromentRaii&&) = delete;
    EnviromentRaii(const EnviromentRaii&) = delete;
    void operator=(const EnviromentRaii&) = delete;
};


struct MainInterpreter : ExpressionObjectVisitor, StatementVoidVisitor
{
    ErrorHandler* error_handler;
    Enviroment* global_enviroment;
    Enviroment* current_enviroment;

    //-------------------------------------------------------------------------
    // constructor

    explicit MainInterpreter(Enviroment* ge, ErrorHandler* eh)
        : error_handler(eh)
        , global_enviroment(ge)
    {
        current_enviroment = global_enviroment;
    }

    //---------------------------------------------------------------------------------------------
    // util functions

    std::shared_ptr<Object>
    get_var(Enviroment& enviroment, const std::string& name, const Offset& off)
    {
        auto var = enviroment.get_or_null(name);
        if(var == nullptr)
        {
            error_handler->on_error(off, "Undefined variable");
            throw RuntimeError{};
        }
        return var;
    }

    void
    set_var(Enviroment& enviroment, const std::string& name, const Offset& off, std::shared_ptr<Object> object)
    {
        const bool was_set = enviroment.set_or_false(name, object);
        if(was_set == false)
        {
            error_handler->on_error(off, fmt::format("Undefined variable {}", name));
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
        Enviroment block_env { current_enviroment };
        auto raii = EnviromentRaii{&current_enviroment, &block_env};
        for(const auto& st: x.statements)
        {
            st->accept(this);
        }
    }

    void
    on_var_statement(const VarStatement& x) override
    {
        // todo(Gustav): make usage of unitialized value an error
        std::shared_ptr<Object> value = x.initializer != nullptr
            ? x.initializer->accept(this)
            : std::make_unique<Nil>()
            ;
        
        current_enviroment->define(x.name, value);
    }

    void
    on_print_statement(const PrintStatement& x) override
    {
        auto value = x.expression->accept(this);
        std::cout << value->to_string() << "\n";
    }

    void
    on_expression_statement(const ExpressionStatement& x) override
    {
        x.expression->accept(this);
    }


    //---------------------------------------------------------------------------------------------
    // expressions

    std::shared_ptr<Object>
    on_assign_expression(const AssignExpression& x) override
    {
        auto value = x.value->accept(this);
        set_var(*current_enviroment, x.name, x.name_offset, value);
        return value;
    }

    std::shared_ptr<Object>
    on_variable_expression(const VariableExpression& x) override
    {
        return get_var(*current_enviroment, x.name, x.offset);
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
        return x.value->clone();
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


}}


namespace lox
{


Interpreter::Interpreter()
    : global_enviroment(nullptr)
{
}


bool
interpret(Interpreter* main_interpreter, Program& program, ErrorHandler* error_handler)
{
    auto interpreter = MainInterpreter{&main_interpreter->global_enviroment, error_handler};
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

