#include "lox/resolver.h"

#include "lox/program.h"
#include "lox/errorhandler.h"

#include <algorithm>

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
    none, function, method, initializer
};

enum class ClassType
{
    none, klass, derived
};


struct MainResolver : ExpressionVoidVisitor, StatementVoidVisitor
{
    using Scope = std::unordered_map<std::string, Var>;
    ErrorHandler* error_handler;
    bool has_errors = false;
    std::vector<Scope> scopes;
    FunctionType current_function = FunctionType::none;
    ClassType current_class = ClassType::none;
    bool inside_static_method = false;

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

        // todo(Gustav): this only means that the error in class Test : Test {} is a undefined variable
        // obviously not good but also not terrible
        // if(x.parent != nullptr && x.parent->name == x.name)
        // {
        //     error_handler->on_error(x.parent->offset, "A class can't inherit from itself");
        //     has_errors = true;
        // }

        inside_static_method = true;
        for(auto& method: x.static_methods)
        {
            resolve_function(*method, FunctionType::method);
        }
        inside_static_method = false;

        if(x.parent != nullptr)
        {
            current_class = ClassType::derived;
            resolve(x.parent);

            begin_scope();
            declare_var("super", x.offset);
            define_var("super");
        }

        struct NamedOffset {std::string name; Offset offset;};
        using NamedOffsetList = std::vector<NamedOffset>;
        std::unordered_map<std::string, NamedOffsetList> defined_properties;

        auto add_property = [&defined_properties](const std::string& name, const std::string& type, Offset offset)
        {
            if(auto found = defined_properties.find(name); found != defined_properties.end())
            {
                found->second.emplace_back(NamedOffset{type, offset});
            }
            else
            {
                defined_properties.insert({name, {NamedOffset{type, offset}}});
            }
            
        };

        for(auto& mem: x.members)
        {
            add_property(mem->name, "var", mem->offset);
            if(mem->initializer != nullptr)
            {
                resolve(mem->initializer);
            }
        }

        begin_scope();
        declare_var("this", x.offset);
        define_var("this");

        for(auto& method: x.methods)
        {
            add_property(method->name, "fun", method->offset);
            const auto function_type = method->name == "init"
                ? FunctionType::initializer
                : FunctionType::method
                ;
            resolve_function(*method, function_type);
        }

        end_scope();

        if(x.parent != nullptr)
        {
            end_scope();
        }

        for(auto& entry: defined_properties)
        {
            const auto name = entry.first;
            NamedOffsetList& offsets = entry.second;
            if(offsets.size() <= 1) { continue; }
            std::sort(offsets.begin(), offsets.end(), [](const NamedOffset& lhs, const NamedOffset& rhs)
            {
                return lhs.offset.start < rhs.offset.start;
            });

            error_handler->on_error(offsets.rbegin()->offset, fmt::format("'{}' declared multiple times", name));
            for(const auto& o: offsets)
            {
                error_handler->on_note(o.offset, fmt::format("as {} {} here", o.name, name));
            }
            has_errors = true;
        }

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
            if(current_function == FunctionType::initializer)
            {
                error_handler->on_error(s.offset, "Can't return value from initializer");
                has_errors = true;
            }
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

    void on_array_expression(const ArrayExpression& x) override
    {
        for(const auto& a: x.values)
        {
            resolve(a);
        }
    }

    void on_constructor_expression(const ConstructorExpression& x) override
    {
        resolve(x.klass);
        for(const auto& a: x.arguments)
        {
            resolve(a);
        }
    }
    
    void on_superconstructorcall_expression(const SuperConstructorCallExpression& x) override
    {
        // todo(Gustav): allow calling super ctor in member functions or should this be blocked too?
        error_for_incorrect_super_usage(x.offset);

        resolve_local(x, "super");
        for(const auto& a: x.arguments)
        {
            resolve(a);
        }
    }

    void on_getproperty_expression(const GetPropertyExpression& x) override
    {
        resolve(x.object);
    }

    void on_setproperty_expression(const SetPropertyExpression& x) override
    {
        resolve(x.value);
        resolve(x.object);
    }

    void on_getindex_expression(const GetIndexExpression& x) override
    {
        resolve(x.index);
        resolve(x.object);
    }

    void on_setindex_expression(const SetIndexExpression& x) override
    {
        resolve(x.value);
        resolve(x.index);
        resolve(x.object);
    }

    void error_for_incorrect_super_usage(const Offset& off)
    {
        if(inside_static_method)
        {
            error_handler->on_error(off, "Can't use 'super' in a static method");
            has_errors = true;
        }
        else
        {
            switch(current_class)
            {
            case ClassType::none:
                error_handler->on_error(off, "Can't use 'super' outside of class");
                has_errors = true;
                break;

            case ClassType::klass:
                error_handler->on_error(off, "Can't use 'super' in class with no superclass");
                has_errors = true;
                break;

            case ClassType::derived:
                break;

            default: assert(false && "unhandled case"); break;
            }
        }
    }

    void on_super_expression(const SuperExpression& x) override
    {
        error_for_incorrect_super_usage(x.offset);
        resolve_local(x, "super");
    }

    void on_this_expression(const ThisExpression& x) override
    {
        if(inside_static_method)
        {
            error_handler->on_error(x.offset, "Can't use 'this' in a static method");
            has_errors = true;
        }
        else if(current_class == ClassType::none)
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
