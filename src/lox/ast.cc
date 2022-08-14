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
    on_function_statement(const FunctionStatement& x) override
    {
        std::vector<std::string> body;
        for(const auto& stmt: x.body)
        {
            body.emplace_back(stmt->accept(this));
        }
        return parenthesize("fun", {x.name, parenthesize("params", x.params), parenthesize("{}", body)});
    }

    std::string
    on_block_statement(const BlockStatement& x) override
    {
        std::vector<std::string> blocks;
        for(const auto& stmt: x.statements)
        {
            blocks.emplace_back(stmt->accept(this));
        }
        return parenthesize("{}", blocks);
    }

    std::string
    on_class_statement(const ClassStatement& x) override
    {
        std::vector<std::string> blocks;
        blocks.emplace_back(x.name);

        if(x.parent)
        {
            blocks.emplace_back(parenthesize("parent", {x.parent->accept(this)}));
        }

        std::vector<std::string> methods;
        for(const auto& stmt: x.methods)
        {
            methods.emplace_back(stmt->accept(this));
        }
        if(methods.empty() == false)
        {
            blocks.emplace_back(parenthesize("methods", methods));
        }

        return parenthesize("class", blocks);
    }

    std::string
    on_while_statement(const WhileStatement& x) override
    {
        return parenthesize("while", {x.condition->accept(this), x.body->accept(this)});
    }

    std::string
    on_return_statement(const ReturnStatement& x) override
    {
        return parenthesize("return", {x.value->accept(this)});
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
            "expr", {x.expression->accept(this)}
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
    on_call_expression(const CallExpression& x) override
    {
        std::vector<std::string> args { x.callee->accept(this)};
        for(const auto& a: x.arguments)
        {
            args.emplace_back(a->accept(this));
        }
        return parenthesize("call", args);
    }

    std::string
    on_get_expression(const GetExpression& x) override
    {
        return parenthesize("get", {x.name, x.object->accept(this)});
    }

    std::string
    on_set_expression(const SetExpression& x) override
    {
        return parenthesize("set", {x.value->accept(this), x.name, x.object->accept(this)});
    }
    
    std::string
    on_super_expression(const SuperExpression& x) override
    {
        return parenthesize("super", {x.property});
    }
    
    std::string
    on_this_expression(const ThisExpression&) override
    {
        return parenthesize("this", {});
    }

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
            link(n, r);
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
        const auto self = "node_{}"_format(next_node_index++);
        nodes << self << "[label=\"" << label << "\"];\n";
        return self;
    }

    void link(const std::string& from, const std::string& to)
    {
        edges << from << " -> " << to << ";\n";
    }

    std::string link_from(const std::string& from, const std::string& to)
    {
        link(from, to);
        return from;
    }

    std::string link_to(const std::string& from, const std::string& to)
    {
        link(from, to);
        return to;
    }

    // --------------------------------------------------------------------------------------------
    // statements

    std::string
    on_function_statement(const FunctionStatement& x) override
    {
        const auto n = link_from(new_node("fun"), x.name);
        const auto body = link_to(n, new_node("body"));
        if(x.params.empty() == false)
        {
            const auto params = link_to(n, new_node("params"));
            for(auto p: x.params)
            {
                link(params, p);
            }
        }
        return n;
    }

    std::string
    on_return_statement(const ReturnStatement& x) override
    {
        return link_from(new_node("return"), x.value->accept(this));
    }

    std::string
    on_class_statement(const ClassStatement& x) override
    {
        const auto n = new_node("class");
        const auto name = link_to(n, new_node(x.name));
        std::optional<std::string> methods;
        for(const auto& stmt: x.methods)
        {
            if(methods.has_value() == false)
            {
                methods = link_to(name, new_node("methods"));
            }
            link(*methods, stmt->accept(this));
        }
        if(x.parent)
        {
            link(n, link_from(new_node("parent"), x.parent->accept(this)));
        }
        return n;
    }

    std::string
    on_block_statement(const BlockStatement& x) override
    {
        const auto n = new_node("{}");
        for(const auto& stmt: x.statements)
        {
            link(n, stmt->accept(this));
        }
        return n;
    }

    std::string
    on_while_statement(const WhileStatement& x) override
    {
        const auto n = new_node("while");
        link(n, x.condition->accept(this));
        link(n, x.body->accept(this));
        return n;
    }

    std::string
    on_print_statement(const PrintStatement& x) override
    {
        return link_from(new_node("print"), x.expression->accept(this));
    }

    std::string
    on_expression_statement(const ExpressionStatement& x) override
    {
        return link_from(new_node("expr"), x.expression->accept(this));
    }

    std::string
    on_var_statement(const VarStatement& x) override
    {
        const auto n = new_node("decl");
        const auto r = new_node(x.name);
        link(n, r);

        if(x.initializer != nullptr)
        {
            link(r, x.initializer->accept(this));
        }

        return n;
    }

    std::string
    on_if_statement(const IfStatement& x) override
    {
        const auto n = new_node("if");
        link(n, x.condition->accept(this));
        link(n, x.then_branch->accept(this));
        if(x.else_branch != nullptr)
        {
            link(n, x.else_branch->accept(this));
        }
        return n;
    }


    // --------------------------------------------------------------------------------------------
    // expressions

    std::string
    on_call_expression(const CallExpression& x) override
    {
        const auto n = new_node("call");
        link(n, x.callee->accept(this));

        if(x.arguments.empty() == false)
        {
            const auto args = new_node("args");
            link(n, args);
            for(const auto& a: x.arguments)
            {
                link(args, a->accept(this));
            }
        }
        return n;
    }

    std::string
    on_get_expression(const GetExpression& x) override
    {
        const auto n = new_node("get");
        const auto v = new_node(x.name);
        link(n, v);
        link(v, x.object->accept(this));
        return n;
    }

    std::string
    on_set_expression(const SetExpression& x) override
    {
        const auto n = new_node("get");
        const auto v = new_node(x.name);
        link(n, v);
        link(n, x.value->accept(this));
        link(v, x.object->accept(this));
        return n;
    }
    
    std::string
    on_super_expression(const SuperExpression& x) override
    {
        const auto n = new_node("super");
        link(n, new_node(x.property));
        return n;
    }

    std::string
    on_this_expression(const ThisExpression&) override
    {
        const auto n = new_node("this");
        return n;
    }

    std::string
    on_binary_expression(const BinaryExpression& expr) override
    {
        const auto self = new_node(tokentype_to_string_short(expr.op));
        link(self, expr.left->accept(this));
        link(self, expr.right->accept(this));
        return self;
    }

    std::string
    on_grouping_expression(const GroupingExpression& expr) override
    {
        return link_from(new_node("group"), expr.expression->accept(this));
    }

    std::string
    on_literal_expression(const LiteralExpression& expr) override
    {
        return new_node(expr.value->to_string());
    }

    std::string
    on_unary_expression(const UnaryExpression& expr) override
    {
        return link_from(new_node(tokentype_to_string_short(expr.op)), expr.right->accept(this));
    }

    std::string
    on_variable_expression(const VariableExpression& x) override
    {
        return link_from(new_node("get"), new_node(x.name));
    }

    std::string
    on_assign_expression(const AssignExpression& x) override
    {
        const auto n = new_node("=");
        const auto r = new_node(x.name);
        link(n, r);
        link(r, x.value->accept(this));
        return n;
    }

    std::string
    on_logical_expression(const LogicalExpression& x) override
    {
        const auto n = new_node(tokentype_to_string_short(x.op));
        
        link(n, x.left->accept(this));
        link(n, x.right->accept(this));

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
