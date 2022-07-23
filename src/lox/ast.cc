#include "lox/ast.h"

#include <sstream>
#include <vector>

#include "lox/object.h"


namespace lox { namespace {

struct AstPrinter : ExprStringVisitor
{
    std::string parenthesize(const std::string& name, const std::vector<std::string>& par)
    {
        std::ostringstream ss;

        ss << "(" << name;
        for(const auto& value : par)
        {
            ss << " " << value;
        }
        ss << ")";

        return ss.str();
    }

    std::string visitBinary(const ExprBinary& expr) override
    {
        return parenthesize
        (
            std::string{tokentype_to_string(expr.op)},
            {
                expr.left->accept(this),
                expr.right->accept(this)
            }
        );
    }

    std::string visitGrouping(const ExprGrouping& expr) override
    {
        return parenthesize("group", {expr.expression->accept(this)});
    }

    std::string visitLiteral(const ExprLiteral& expr) override
    {
        return expr.value->to_string();
    }

    std::string visitUnary(const ExprUnary& expr) override
    {
        return parenthesize
        (
            std::string{tokentype_to_string(expr.op)},
            {expr.right->accept(this)}
        );
    }
};

}}

namespace lox
{
    std::string print_ast(const Expr& ast)
    {
        AstPrinter printer;
        return ast.accept(&printer);
    }
}
