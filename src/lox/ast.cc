#include "lox/ast.h"

#include <sstream>
#include <vector>

#include "lox/object.h"
#include "lox/program.h"


namespace lox { namespace {

struct AstPrinter : ExprStringVisitor, StmtStringVisitor
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
            std::string{tokentype_to_string_short(expr.op)},
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
            std::string{tokentype_to_string_short(expr.op)},
            {expr.right->accept(this)}
        );
    }

    std::string
    visitPrint(const StmtPrint& x) override
    {
        return parenthesize
        (
            "print", {x.expression->accept(this)}
        );
    }

    std::string
    visitExpression(const StmtExpression& x) override
    {
        return parenthesize
        (
            "expression", {x.expression->accept(this)}
        );
    }

    std::string run(const Program& p)
    {
        std::vector<std::string> statements;

        for(const auto& s: p.statements)
        {
            statements.emplace_back(s->accept(this));
        }

        return parenthesize("program", statements);
    }
};



struct GraphvizPrinter : ExprStringVisitor, StmtStringVisitor
{
    int next_node_index = 1;
    std::ostringstream nodes;
    std::ostringstream edges;

    std::string new_node(std::string_view label)
    {
        const auto self = fmt::format("node_{}", next_node_index++);
        nodes << self << "[label=\"" << label << "\"];\n";
        return self;
    }

    std::string visitBinary(const ExprBinary& expr) override
    {
        const auto self = new_node(tokentype_to_string_short(expr.op));
        const auto left = expr.left->accept(this);
        const auto right = expr.right->accept(this);

        edges << self << " -> " << left << ";\n";
        edges << self << " -> " << right << ";\n";
        return self;
    }

    std::string visitGrouping(const ExprGrouping& expr) override
    {
        const auto self = new_node("group");
        const auto node = expr.expression->accept(this);
        edges << self << " -> " << node << ";\n";
        return self;
    }

    std::string visitLiteral(const ExprLiteral& expr) override
    {
        return new_node(expr.value->to_string());
    }

    std::string visitUnary(const ExprUnary& expr) override
    {
        const auto self = new_node(tokentype_to_string_short(expr.op));
        const auto right = expr.right->accept(this);
        edges << self << " -> " << right << ";\n";
        return self;
    }

    std::string to_graphviz() const
    {
        std::ostringstream ss;
        ss << "digraph G {\n";
        ss << nodes.str();
        ss << "\n\n";
        ss << edges.str();
        ss << "}\n";
        return ss.str();
    }

    std::string
    visitPrint(const StmtPrint& x) override
    {
        const auto n = new_node("print");
        const auto r = x.expression->accept(this);
        edges << n << " -> " << r << ";\n";
        return n;
    }

    std::string
    visitExpression(const StmtExpression& x) override
    {
        const auto n = new_node("expr");
        const auto r = x.expression->accept(this);
        edges << n << " -> " << r << ";\n";
        return n;
    }

    void run(const Program& p)
    {
        const auto n = new_node("prog");
        for(const auto& s: p.statements)
        {
            const auto r = s->accept(this);
            edges << n << " -> " << r << ";\n";
        }
    }
};

}}

namespace lox
{
    std::string print_ast(const Program& ast)
    {
        AstPrinter printer;
        return printer.run(ast);
    }

    std::string ast_to_grapviz(const Program& ast)
    {
        GraphvizPrinter printer;
        printer.run(ast);
        return printer.to_graphviz();
    }
}
