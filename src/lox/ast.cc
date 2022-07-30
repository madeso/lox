#include "lox/ast.h"

#include <sstream>
#include <vector>

#include "lox/object.h"
#include "lox/program.h"


namespace lox { namespace {

struct AstPrinter : ExpressionStringVisitor, StatementStringVisitor
{
    // --------------------------------------------------------------------------------------------
    // entry functions

    std::string run(const Program& p)
    {
        std::vector<std::string> statements;

        for(const auto& s: p.statements)
        {
            statements.emplace_back(s->accept(this));
        }

        return parenthesize("program", statements);
    }

    // --------------------------------------------------------------------------------------------
    // util functions

    std::string parenthesize(std::string_view name, const std::vector<std::string>& par)
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

    // --------------------------------------------------------------------------------------------
    // statements

    std::string
    on_block_statement(const BlockStatement& x) override
    {
        std::vector<std::string> blocks;
        for(const auto& stmt: x.statements)
        {
            blocks.emplace_back(stmt->accept(this));
        }
        return parenthesize("block", blocks);
    }

    std::string
    on_while_statement(const WhileStatement& x) override
    {
        return parenthesize("while", {x.condition->accept(this), x.body->accept(this)});
    }

    std::string
    on_print_statement(const PrintStatement& x) override
    {
        return parenthesize
        (
            "print", {x.expression->accept(this)}
        );
    }

    std::string
    on_expression_statement(const ExpressionStatement& x) override
    {
        return parenthesize
        (
            "expression", {x.expression->accept(this)}
        );
    }

    std::string
    on_var_statement(const VarStatement& x) override
    {
        std::vector<std::string> vars = {x.name};
        if(x.initializer != nullptr)
        {
            vars.emplace_back(x.initializer->accept(this));
        }
        return parenthesize("decl", vars);
    }

    std::string
    on_if_statement(const IfStatement& x) override
    {
        std::vector<std::string> vars = {x.condition->accept(this), x.then_branch->accept(this)};
        if(x.else_branch != nullptr)
        {
            vars.emplace_back(x.else_branch->accept(this));
        }
        return parenthesize("if", vars);
    }

    // --------------------------------------------------------------------------------------------
    // expressions

    std::string
    on_binary_expression(const BinaryExpression& expr) override
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

    std::string
    on_grouping_expression(const GroupingExpression& expr) override
    {
        return parenthesize("group", {expr.expression->accept(this)});
    }

    std::string
    on_literal_expression(const LiteralExpression& expr) override
    {
        return expr.value->to_string();
    }

    std::string
    on_unary_expression(const UnaryExpression& expr) override
    {
        return parenthesize
        (
            std::string{tokentype_to_string_short(expr.op)},
            {expr.right->accept(this)}
        );
    }

    std::string
    on_variable_expression(const VariableExpression& x) override
    {
        return parenthesize("get", {x.name});
    }

    std::string
    on_assign_expression(const AssignExpression& x) override
    {
        const auto v = x.value->accept(this);
        return parenthesize("=", {x.name, v});
    }

    std::string
    on_logical_expression(const LogicalExpression& x) override
    {
        auto left = x.left->accept(this);
        auto right = x.right->accept(this);
        return parenthesize(tokentype_to_string_short(x.op), {left, right});
    }
};



struct GraphvizPrinter : ExpressionStringVisitor, StatementStringVisitor
{
    int next_node_index = 1;
    std::ostringstream nodes;
    std::ostringstream edges;

    // --------------------------------------------------------------------------------------------
    // main functions

    void run(const Program& p)
    {
        const auto n = new_node("prog");
        for(const auto& s: p.statements)
        {
            const auto r = s->accept(this);
            edges << n << " -> " << r << ";\n";
        }
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

    // --------------------------------------------------------------------------------------------
    // util functions

    std::string new_node(std::string_view label)
    {
        const auto self = fmt::format("node_{}", next_node_index++);
        nodes << self << "[label=\"" << label << "\"];\n";
        return self;
    }

    // --------------------------------------------------------------------------------------------
    // statements

    std::string
    on_block_statement(const BlockStatement& x) override
    {
        const auto n = new_node("{}");
        for(const auto& stmt: x.statements)
        {
            const auto r = stmt->accept(this);
            edges << n << " -> " << r << ";\n";
        }
        return n;
    }

    std::string
    on_while_statement(const WhileStatement& x) override
    {
        const auto n = new_node("while");
        const auto c = x.condition->accept(this);
        const auto b = x.body->accept(this);
        edges << n << " -> " << c << ";\n";
        edges << n << " -> " << b << ";\n";
        return n;
    }

    std::string
    on_print_statement(const PrintStatement& x) override
    {
        const auto n = new_node("print");
        const auto r = x.expression->accept(this);
        edges << n << " -> " << r << ";\n";
        return n;
    }

    std::string
    on_expression_statement(const ExpressionStatement& x) override
    {
        const auto n = new_node("expr");
        const auto r = x.expression->accept(this);
        edges << n << " -> " << r << ";\n";
        return n;
    }

    std::string
    on_var_statement(const VarStatement& x) override
    {
        const auto n = new_node("decl");
        const auto r = new_node(x.name);
        edges << n << " -> " << r << ";\n";

        if(x.initializer != nullptr)
        {
            const auto v = x.initializer->accept(this);
            edges << r << " -> " << v << ";\n";
        }

        return n;
    }

    std::string
    on_if_statement(const IfStatement& x) override
    {
        const auto n = new_node("if");
        const auto c = x.condition->accept(this);
        edges << n << " -> " << c << ";\n";
        const auto t = x.then_branch->accept(this);
        edges << n << " -> " << t << ";\n";
        if(x.else_branch != nullptr)
        {
            const auto e = x.else_branch->accept(this);
            edges << n << " -> " << e << ";\n";
        }
        return n;
    }


    // --------------------------------------------------------------------------------------------
    // expressions

    std::string
    on_binary_expression(const BinaryExpression& expr) override
    {
        const auto self = new_node(tokentype_to_string_short(expr.op));
        const auto left = expr.left->accept(this);
        const auto right = expr.right->accept(this);

        edges << self << " -> " << left << ";\n";
        edges << self << " -> " << right << ";\n";
        return self;
    }

    std::string
    on_grouping_expression(const GroupingExpression& expr) override
    {
        const auto self = new_node("group");
        const auto node = expr.expression->accept(this);
        edges << self << " -> " << node << ";\n";
        return self;
    }

    std::string
    on_literal_expression(const LiteralExpression& expr) override
    {
        return new_node(expr.value->to_string());
    }

    std::string
    on_unary_expression(const UnaryExpression& expr) override
    {
        const auto self = new_node(tokentype_to_string_short(expr.op));
        const auto right = expr.right->accept(this);
        edges << self << " -> " << right << ";\n";
        return self;
    }

    std::string
    on_variable_expression(const VariableExpression& x) override
    {
        const auto n = new_node("get");
        const auto r = new_node(x.name);
        edges << n << " -> " << r << ";\n";
        return n;
    }

    std::string
    on_assign_expression(const AssignExpression& x) override
    {
        const auto n = new_node("=");
        const auto r = new_node(x.name);
        const auto v = x.value->accept(this);
        edges << n << " -> " << r << ";\n";
        edges << r << " -> " << v << ";\n";
        return n;
    }

    std::string
    on_logical_expression(const LogicalExpression& x) override
    {
        const auto n = new_node(tokentype_to_string_short(x.op));
        auto left = x.left->accept(this);
        auto right = x.right->accept(this);

        edges << n << " -> " << left << ";\n";
        edges << n << " -> " << right << ";\n";

        return n;
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
