#include "lox/resolver.h"

#include "lox/program.h"
#include "lox/errorhandler.h"


namespace lox { namespace
{



enum class VarStatus
{
    declared, defined
};

struct Var
{
    Offset offset;
    VarStatus status;
};

enum class FunctionType
{
    none, function, method
};

enum class ClassType
{
    none, klass
};


struct MainResolver : ExpressionVoidVisitor, StatementVoidVisitor
{
    using Scope = std::unordered_map<std::string, Var>;
    ErrorHandler* error_handler;
    bool has_errors = false;
    std::vector<Scope> scopes;
    FunctionType current_function = FunctionType::none;
    ClassType current_class = ClassType::none;

    //-------------------------------------------------------------------------
    // constructor

    explicit MainResolver(ErrorHandler* eh)
        : error_handler(eh)
    {
    }

    //---------------------------------------------------------------------------------------------
    // util functions

    std::unordered_map<u64, std::size_t> locals;
    void interpreter_resolve(u64 id, std::size_t dist)
    {
        locals.insert({id, dist});
    }

    void resolve(const std::vector<std::shared_ptr<Statement>>& statements)
    {
        for(const auto& st: statements)
        {
            resolve(st);
        }
    }

    void resolve(std::shared_ptr<Statement> statement)
    {
        statement->accept(this);
    }

    void resolve(std::shared_ptr<Expression> statement)
    {
        statement->accept(this);
    }

    void begin_scope()
    {
        scopes.emplace_back();
    }

    void end_scope()
    {
        scopes.pop_back();
    }

    void declare_var(const std::string& name, const Offset& off)
    {
        if(scopes.empty()) { return; }

        Scope& scope = scopes.back();
        if(auto found = scope.find(name); found != scope.end())
        {
            error_handler->on_error(off, "There is already a variable with this name in this scope");
            error_handler->on_note(found->second.offset, "declared here");
            has_errors = true;
            return;
        }

        scope.insert({name, Var{off, VarStatus::declared}});
    }

    void define_var(const std::string& name)
    {
        if(scopes.empty()) { return; }
        Scope& scope = scopes.back();
        auto found = scope.find(name);
        assert(found != scope.end());
        found->second.status = VarStatus::defined;
    }

    void resolve_local(const Expression& x, const std::string& name)
    {
        if(scopes.empty()) { return; }

        for (std::size_t scope_index = scopes.size() - 1; true; scope_index -= 1)
        {
            Scope& scope = scopes[scope_index];
            if(scopes[scope_index].find(name) != scope.end())
            {
                interpreter_resolve(x.uid.value, scopes.size() - 1 - scope_index);
                return;
            }

            if(scope_index == 0)
            {
                return;
            }
        }
    }

    void resolve_function(const FunctionStatement& s, FunctionType type)
    {
        auto enclosing_function = current_function;
        current_function = type;
        begin_scope();

        for(const auto& p: s.params)
        {
            // todo(Gustav): set offset to param
            declare_var(p, s.offset);
            define_var(p);
        }
        resolve(s.body);

        end_scope();
        current_function = enclosing_function;
    }


    //---------------------------------------------------------------------------------------------
    // statements

    void on_block_statement(const BlockStatement& s) override
    {
        begin_scope();
        resolve(s.statements);
        end_scope();
    }
    
    void on_class_statement(const ClassStatement& x) override
    {
        auto enclosing_class = current_class;
        current_class = ClassType::klass;

        declare_var(x.name, x.offset);
        define_var(x.name);

        begin_scope();
        declare_var("this", x.offset);
        define_var("this");

        for(auto& method: x.methods)
        {
            resolve_function(*method, FunctionType::method);
        }

        end_scope();

        current_class = enclosing_class;
    }

    void on_var_statement(const VarStatement& s) override
    {
        declare_var(s.name, s.offset);
        if(s.initializer != nullptr)
        {
            resolve(s.initializer);
        }
        define_var(s.name);
    }

    void on_function_statement(const FunctionStatement& s) override
    {
        // todo(Gustav): only function "header"?
        declare_var(s.name, s.offset);
        define_var(s.name);
        resolve_function(s, FunctionType::function);
    }

    void on_expression_statement(const ExpressionStatement& s) override
    {
        resolve(s.expression);
    }

    void on_if_statement(const IfStatement& s) override
    {
        resolve(s.condition);
        resolve(s.then_branch);
        if(s.else_branch != nullptr)
        {
            resolve(s.else_branch);
        }
    }

    void on_print_statement(const PrintStatement& s) override
    {
        resolve(s.expression);
    }

    void on_return_statement(const ReturnStatement& s) override
    {
        if(current_function == FunctionType::none)
        {
            error_handler->on_error(s.offset, "Can't return from top-level code");
            has_errors = true;
        }

        if(s.value != nullptr)
        {
            resolve(s.value);
        }
    }

    void on_while_statement(const WhileStatement& s) override
    {
        resolve(s.condition);
        resolve(s.body);
    }


    //---------------------------------------------------------------------------------------------
    // expressions

    void on_variable_expression(const VariableExpression& x) override
    {
        // check self assignment (current scope) (var a = a + 3;)
        if(scopes.empty() == false)
        {
             Scope& current_scope = scopes.back();
             auto found = current_scope.find(x.name);
             const auto var_is_in_current_scope = found != current_scope.end();
             if(var_is_in_current_scope && found->second.status != VarStatus::defined)
             {
                error_handler->on_error(x.offset, "Can't read local variable in its own initializer");
                // todo(Gustav): reduce or remove note? it should only be the var statement not the whole statement
                error_handler->on_note(found->second.offset, "declared here");
                has_errors = true;
                // todo(Gustav): edit distance search upwards for similar looking variables?
             }
        }

        resolve_local(x, x.name);
    }

    void on_assign_expression(const AssignExpression& x) override
    {
        resolve(x.value);
        resolve_local(x, x.name);
    }

    void on_binary_expression(const BinaryExpression& x) override
    {
        resolve(x.left);
        resolve(x.right);
    }

    void on_call_expression(const CallExpression& x) override
    {
        resolve(x.callee);
        for(const auto& a: x.arguments)
        {
            resolve(a);
        }
    }

    void on_get_expression(const GetExpression& x) override
    {
        resolve(x.object);
    }

    void on_set_expression(const SetExpression& x) override
    {
        resolve(x.value);
        resolve(x.object);
    }

    void on_this_expression(const ThisExpression& x) override
    {
        if(current_class != ClassType::klass)
        {
            error_handler->on_error(x.offset, "Can't use 'this' outside of a class");
            has_errors = true;
        }

        resolve_local(x, "this");
    }

    void on_grouping_expression(const GroupingExpression& x) override
    {
        resolve(x.expression);
    }

    void on_literal_expression(const LiteralExpression&) override
    {
        // nop
    }

    void on_logical_expression(const LogicalExpression& x) override
    {
        resolve(x.left);
        resolve(x.right);
    }

    void on_unary_expression(const UnaryExpression& x) override
    {
        resolve(x.right);
    }
};



}}



namespace lox
{

std::optional<Resolved>
resolve(Program& program, ErrorHandler* eh)
{
    auto main = MainResolver{eh};

    for(auto& s: program.statements)
    {
        s->accept(&main);
    }

    if(main.has_errors) { return std::nullopt; }

    return Resolved{main.locals};
}

}
