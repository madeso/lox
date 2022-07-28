#include "lox/interpreter.h"

#include <cassert>
#include <iostream>

#include "lox/ast.h"
#include "lox/errorhandler.h"
#include "lox/program.h"


namespace lox{ namespace
{

struct RuntimeError {};


void
check_single_number_operand(ErrorHandler* error_handler, const Offset& op_offset, std::shared_ptr<Object> object, const Offset& object_offset)
{
    const auto type = object->get_type();
    if(type == ObjectType::number) { return; }

    error_handler->on_error(op_offset, "operand must be a number");
    error_handler->on_error(object_offset, fmt::format("This evaluated to {}", objecttype_to_string(type)));
    throw RuntimeError();
}


void
check_binary_number_operand(ErrorHandler* error_handler, const Offset& op_offset, std::shared_ptr<Object> lhs, std::shared_ptr<Object> rhs, const Offset& lhs_offset, const Offset& rhs_offset)
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
check_binary_number_or_string_operands(ErrorHandler* error_handler, const Offset& op_offset, std::shared_ptr<Object> lhs, std::shared_ptr<Object> rhs, const Offset& lhs_offset, const Offset& rhs_offset)
{
    const auto lhs_type = lhs->get_type();
    const auto rhs_type = rhs->get_type();
    if(
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


float get_number(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::number);
    return static_cast<Number*>(o.get())->value;
}


std::string get_string(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::string);
    return static_cast<String*>(o.get())->value;
}

bool get_bool(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::boolean);
    return static_cast<Bool*>(o.get())->value;
}


bool is_truthy(const Object& o)
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


struct Interpreter : ExprObjectVisitor, StmtVoidVisitor
{
    ErrorHandler* error_handler;

    explicit Interpreter(ErrorHandler* eh) : error_handler(eh) {}


    void
    visitPrint(const StmtPrint& x) override
    {
        auto value = x.expression->accept(this);
        std::cout << value->to_string() << "\n";
    }

    void
    visitExpression(const StmtExpression& x) override
    {
        x.expression->accept(this);
    }

    std::shared_ptr<Object>
    visitBinary(const ExprBinary& x) override
    {
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
            assert(false && "unhandled token in Interpreter::visitBinary(...)");
            return nullptr;
        }
    }


    std::shared_ptr<Object>
    visitGrouping(const ExprGrouping& x) override
    {
        return x.expression->accept(this);
    }


    std::shared_ptr<Object>
    visitLiteral(const ExprLiteral& x) override
    {
        return x.value->clone();
    }


    std::shared_ptr<Object>
    visitUnary(const ExprUnary& x) override
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


bool
interpret(Program& program, ErrorHandler* error_handler)
{
    auto interpreter = Interpreter{error_handler};
    try
    {
        for(auto& s: program.statements)
        {
            s->accept(&interpreter);
        }
        return true;
    }
    catch (RuntimeError error)
    {
        return false;
    }
}


}

