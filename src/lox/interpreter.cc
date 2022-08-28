#include "lox/interpreter.h"

#include <cassert>


#include "lox/ast.h"
#include "lox/errorhandler.h"
#include "lox/program.h"
#include "lox/environment.h"
#include "lox/resolver.h"


namespace lox { namespace
{
struct MainInterpreter;


struct State
{
    std::unordered_map<u64, std::size_t> locals;

    explicit State(const Resolved& resolved)
        : locals(resolved.locals)
    {
    }
};

std::shared_ptr<Object>
interpret_initial_value(MainInterpreter* inter, const VarStatement&);

void
execute_statements_with_environment
(
    MainInterpreter* main,
    const std::vector<std::shared_ptr<Statement>>& statements,
    std::shared_ptr<Environment> environment,
    std::shared_ptr<State> state
);


struct CallError
{
    std::string error;
    explicit CallError(const std::string& err)
        : error(err)
    {
    }
};


struct ReturnValue
{
    std::shared_ptr<Object> value;
    explicit ReturnValue(std::shared_ptr<Object> v)
        : value(v)
    {
    }
};

struct InvalidArgumentType
{
    u64 argument_index;
    ObjectType expected_type;

    InvalidArgumentType
    (
        u64 aargument_index,
        ObjectType atype
    )
        : argument_index(aargument_index)
        , expected_type(atype)
    {
    }
};


struct RuntimeError
{
};


struct ScriptFunction : Callable
{
    MainInterpreter* interpreter;
    std::shared_ptr<Environment> closure;
    std::shared_ptr<State> state;
    std::string to_str;
    std::vector<std::string> params;
    std::vector<std::shared_ptr<Statement>> body;
    bool is_initializer;

    explicit ScriptFunction
    (
        MainInterpreter* i,
        std::shared_ptr<Environment> c,
        std::shared_ptr<State> s,
        const std::string& n,
        const std::vector<std::string>& p,
        const std::vector<std::shared_ptr<Statement>>& b,
        bool ii
    )
        : interpreter(i)
        , closure(c)
        , state(s)
        , to_str(n)
        , params(p)
        , body(b)
        , is_initializer(ii)
    {
    }

    std::shared_ptr<Callable>
    bind(std::shared_ptr<Object> instance) override
    {
        auto env = std::make_shared<Environment>(closure);
        env->define("this", instance);
        return std::make_shared<ScriptFunction>(interpreter, env, state, to_str, params, body, is_initializer);
    }

    std::string
    to_string() const override
    {
        return "<{}>"_format(to_str);
    }

    std::shared_ptr<Object>
    call(const Arguments& arguments) override
    {
        verify_number_of_arguments(arguments, params.size());
        
        auto environment = std::make_shared<Environment>(closure);

        for(std::size_t param_index = 0; param_index < params.size(); param_index += 1)
        {
            environment->define(params[param_index], arguments.arguments[param_index]);
        }

        try
        {
            execute_statements_with_environment(interpreter, body, environment, state);
        }
        catch(const ReturnValue& r)
        {
            if(is_initializer)
            {
                auto ini = closure->get_at_or_null(0, "this");
                assert(ini != nullptr);
                return ini;
            }
            return r.value;
        }

        if(is_initializer)
        {
            auto ini = closure->get_at_or_null(0, "this");
            assert(ini != nullptr);
            return ini;
        }

        return make_nil();
    }
};


bool
is_number(ObjectType t)
{
    return t == ObjectType::number_int || t == ObjectType::number_float;
}


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
    if(is_number(type)) { return; }

    error_handler->on_error(op_offset, "operand must be a int or a float");
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
    if(is_number(lhs_type) && is_number(rhs_type)) { return; }

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
        (is_number(lhs_type) && is_number(rhs_type))
        ||
        (lhs_type == ObjectType::string && rhs_type == ObjectType::string)
    )
    { return; }

    error_handler->on_error(op_offset, "operands must be numbers or strings");
    error_handler->on_note(lhs_offset, "left hand evaluated to {}"_format(objecttype_to_string(lhs_type)));
    error_handler->on_note(rhs_offset, "right hand evaluated to {}"_format(objecttype_to_string(rhs_type)));
    throw RuntimeError();
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
    case ObjectType::number_int:
        return get_int_or_ub(lhs) == get_int_or_ub(rhs);
    case ObjectType::boolean:
        return get_bool_or_ub(lhs) == get_bool_or_ub(rhs);
    case ObjectType::string:
        return get_string_or_ub(lhs) == get_string_or_ub(rhs);
    default:
        assert(false && "unhandled type in is_equal(...)");
        return false;
    }
}


template<typename T>
struct SharedptrRaii
{
    std::shared_ptr<T>* parent;
    std::shared_ptr<T> last_child;

    SharedptrRaii(std::shared_ptr<T>* p, std::shared_ptr<T> child) : parent(p), last_child(*p)
    {
        *parent = child;
    }

    ~SharedptrRaii()
    {
        *parent = last_child;
    }

    SharedptrRaii(SharedptrRaii&&) = delete;
    void operator=(SharedptrRaii&&) = delete;
    SharedptrRaii(const SharedptrRaii&) = delete;
    void operator=(const SharedptrRaii&) = delete;
};


struct ScriptInstance : Instance
{
    std::unordered_map<std::string, std::shared_ptr<Object>> fields;

    explicit ScriptInstance(std::shared_ptr<Klass> o)
        : Instance(o)
    {
    }

    ObjectType
    get_type() const override
    {
        return ObjectType::instance;
    }

    std::string
    to_string() const override
    {
        return "<instance {}>"_format(klass->klass_name);
    }

    std::shared_ptr<Object> get_field_or_null(const std::string& name) override
    {
        if(auto found = fields.find(name); found != fields.end())
        {
            return found->second;
        }

        return nullptr;
    }

    bool set_field_or_false(const std::string& name, std::shared_ptr<Object> value) override
    {
        if(auto found = fields.find(name); found != fields.end())
        {
            found->second = value;
            return true;
        }
        else
        {
            return false;
        }

    }

    bool add_member(const std::string& name, std::shared_ptr<Object> value)
    {
        if(auto found = fields.find(name); found != fields.end())
        {
            return false;
        }
        else
        {
            fields.insert({name, value});
            return true;
        }

    }
};


struct ScriptKlass : Klass
{
    MainInterpreter* inter;
    std::vector<std::shared_ptr<VarStatement>> members;

    ScriptKlass
    (
        const std::string& nm,
        std::shared_ptr<Klass> sk,
        MainInterpreter* mi,
        std::vector<std::shared_ptr<VarStatement>> mems
    )
        : Klass(nm, sk)
        , inter(mi)
        , members(mems)
    {
    }

    std::string
    to_string() const override
    {
        return "<class {}>"_format(klass_name);
    }

    std::shared_ptr<Object>
    constructor(const Arguments& arguments) override
    {
        auto self = shared_from_this();
        auto klass = std::static_pointer_cast<Klass>(self);

        auto instance = std::make_shared<ScriptInstance>(klass);

        for(const auto& m: members)
        {
            [[maybe_unused]] const auto was_added = instance->add_member(m->name, interpret_initial_value(inter, *m.get()));
            assert(was_added);
        }

        if (auto initializer = klass->find_method_or_null("init"); initializer != nullptr)
        {
            initializer->bind(instance)->call(arguments);
        }
        else
        {
            verify_number_of_arguments(arguments, 0);
        }

        return instance;
    }
};



void report_error_no_properties(const Offset& offset, ErrorHandler* error_handler, std::shared_ptr<Object> object)
{
    assert(error_handler);
    assert(object);
    const auto type = object->get_type();

    // special case for nil, no need to write the value of nil
    if(type == ObjectType::nil)
    {
        error_handler->on_error
        (
            offset, "{} is not capable of having any properties"_format
            (
                objecttype_to_string(type)
            )
        );
    }
    else
    {
        error_handler->on_error
        (
            offset, "{} is not capable of having any properties, has value {}"_format
            (
                objecttype_to_string(type),
                object->to_string()
            )
        );
    }
}



Tf
get_number_as_float(std::shared_ptr<Object> obj)
{
    const auto type = obj->get_type();
    if(type == ObjectType::number_float)
    {
        return get_float_or_ub(obj);
    }
    else
    {
        assert(type == ObjectType::number_int);
        return static_cast<Tf>(get_int_or_ub(obj));
    }
}



struct MainInterpreter : ExpressionObjectVisitor, StatementVoidVisitor
{
    ErrorHandler* error_handler;
    std::shared_ptr<Environment> global_environment;
    std::shared_ptr<Environment> current_environment;
    std::shared_ptr<State> current_state;
    std::function<void (std::string)> on_line;

    //-------------------------------------------------------------------------
    // constructor

    explicit MainInterpreter
    (
        std::shared_ptr<Environment> ge,
        ErrorHandler* eh,
        const std::function<void (std::string)>& ol
    )
        : error_handler(eh)
        , global_environment(ge)
        , on_line(ol)
    {
    }

    //---------------------------------------------------------------------------------------------
    // util functions

    std::shared_ptr<Object>
    evaluate(std::shared_ptr<Expression> x)
    {
        return x->accept(this);
    }

    void
    execute(std::shared_ptr<Statement> x)
    {
        x->accept(this);
    }

    std::shared_ptr<Object>
    lookup_var(Environment& environment, const std::string& name, const Expression& x)
    {
        auto found = current_state->locals.find(x.uid.value);
        if(found != current_state->locals.end())
        {
            auto r = environment.get_at_or_null(found->second, name);
            assert(r != nullptr);
            return r;
        }
        else
        {
            auto r = global_environment->get_or_null(name);
            if(r == nullptr)
            {
                error_handler->on_error(x.offset, "Undefined variable {}"_format(name));
                throw RuntimeError{};
            }
            return r;
        }
    }

    std::optional<std::size_t>
    lookup_distance_for_var(const Expression& x)
    {
        auto found = current_state->locals.find(x.uid.value);
        if(found != current_state->locals.end())
        {
            return found->second;
        }
        else
        {
            return std::nullopt;
        }
    }

    void
    set_var_via_lookup(Environment& environment, const std::string& name, std::shared_ptr<Object> value, const Expression& x)
    {
        auto found = current_state->locals.find(x.uid.value);
        if(found != current_state->locals.end())
        {
            [[maybe_unused]] auto r = environment.set_at_or_false(found->second, name, value);
            assert(r == true);
        }
        else
        {
            const auto was_set = global_environment->set_or_false(name, value);
            if(was_set == false)
            {
                const bool is_in_global = global_environment.get() == current_environment.get();
                if(is_in_global)
                {
                    error_handler->on_error(x.offset, "Global variable {} was never declared"_format(name));
                }
                else
                {
                    error_handler->on_error(x.offset, "Variable {} was neither declared in global nor local scope"_format(name));
                }
                throw RuntimeError{};
            }
        }
    }

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
    on_class_statement(const ClassStatement& x) override
    {
        std::shared_ptr<Klass> superklass;
        if(x.parent != nullptr)
        {
            auto parent = evaluate(x.parent);
            if(parent != nullptr)
            {
                if(parent->get_type() != ObjectType::klass)
                {
                    error_handler->on_error
                    (
                        x.parent->offset,
                        "Superclass must be a class, was {}"_format
                        (
                            objecttype_to_string(parent->get_type())
                        )
                    );
                    throw RuntimeError{};
                }
                else
                {
                    superklass = std::static_pointer_cast<Klass>(parent);
                }
            }
        }

        auto new_klass = std::make_shared<ScriptKlass>(x.name, superklass, this, x.members);
        current_environment->define(x.name, new_klass);

        std::shared_ptr<Environment> backup_environment;
        if(superklass != nullptr)
        {
            backup_environment = current_environment;
            current_environment = std::make_shared<Environment>(current_environment);
            current_environment->define("super", superklass);
        }
        
        // std::unordered_map<std::string, std::shared_ptr<ScriptFunction>> method_list;
        for(const auto& method: x.methods)
        {
            auto function = std::make_shared<ScriptFunction>
            (
                this,
                current_environment,
                current_state,
                "mtd {}"_format(method->name), method->params, method->body,
                method->name == "init"
            );

            auto added = new_klass->add_method_or_false(method->name, std::move(function));
            if(added == false)
            {
                error_handler->on_error(method->offset, "method already defined in this class");
                assert(false && "refactor resolver so that it handles this with note to first definition");
                throw RuntimeError{};
            }
        }

        if(superklass != nullptr)
        {
            current_environment = backup_environment;
        }

        [[maybe_unused]] const auto set = current_environment->set_or_false
        (
            x.name,
            new_klass
        );
        assert(set);
    }
    
    void
    on_return_statement(const ReturnStatement& x) override
    {
        std::shared_ptr<Object> value;

        if(x.value != nullptr)
        {
            value = evaluate(x.value);
        }

        throw ReturnValue{value};
    }

    void
    on_if_statement(const IfStatement& x) override
    {
        if(is_truthy(evaluate(x.condition)))
        {
            execute(x.then_branch);
        }
        else
        {
            if(x.else_branch != nullptr)
            {
                execute(x.else_branch);
            }
        }
    }

    void
    on_block_statement(const BlockStatement& x) override
    {
        auto block_env = std::make_shared<Environment>(current_environment);
        execute_statements_with_environment(x.statements, block_env, current_state);
    }

    void
    on_function_statement(const FunctionStatement& x) override
    {
        assert(current_environment);
        assert(current_state);
        current_environment->define
        (
            x.name, std::make_shared<ScriptFunction>
            (
                this,
                current_environment,
                current_state,
                "fn {}"_format(x.name), x.params, x.body, false
            )
        );
    }

    void
    execute_statements_with_environment
    (
        const std::vector<std::shared_ptr<Statement>>& statements,
        std::shared_ptr<Environment> environment,
        std::shared_ptr<State> state
    )
    {
        auto env_raii = SharedptrRaii<Environment>{&current_environment, environment};
        auto state_raii = SharedptrRaii<State>{&current_state, state};
        for(const auto& st: statements)
        {
            execute(st);
        }
    }

    void
    on_while_statement(const WhileStatement& x) override
    {
        while(is_truthy(evaluate(x.condition)))
        {
            execute(x.body);
        }
    }

    std::shared_ptr<Object>
    create_value(const VarStatement& x)
    {
        // todo(Gustav): make usage of unitialized value an error
        return x.initializer != nullptr
            ? evaluate(x.initializer)
            : make_nil()
            ;
    }

    void
    on_var_statement(const VarStatement& x) override
    {
        auto value = create_value(x);
        current_environment->define(x.name, value);
    }

    void
    on_print_statement(const PrintStatement& x) override
    {
        auto value = evaluate(x.expression);
        const auto to_print = value->to_string();
        on_line(to_print);
    }

    void
    on_expression_statement(const ExpressionStatement& x) override
    {
        evaluate(x.expression);
    }


    //---------------------------------------------------------------------------------------------
    // expressions

    std::shared_ptr<Object>
    on_call_expression(const CallExpression& x) override
    {
        auto callee = evaluate(x.callee);
        
        auto function = as_callable(callee);
        if(function == nullptr)
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
            if(callee->get_type() == ObjectType::klass)
            {
                error_handler->on_note(x.callee->offset, "did you forget to use new?");
            }
            error_handler->on_note(x.offset, "call occured here");
            throw RuntimeError{};
        }

        std::vector<std::shared_ptr<Object>> arguments;
        for(auto& argument : x.arguments)
        { 
            arguments.emplace_back(evaluate(argument));
        }

        try
        {
            auto return_value = function->call({arguments});
            return return_value;
        }
        catch(const InvalidArgumentType& invalid_arg_error)
        {
            auto invalid_arg_value = arguments[invalid_arg_error.argument_index];
            // special case for nil, no need to write the value of nil
            const auto actual_type = invalid_arg_value->get_type();
            if(actual_type == ObjectType::nil)
            {
                error_handler->on_error
                (
                    x.offset, "nil is not accepted for argument {}, expected {}"_format
                    (
                        invalid_arg_error.argument_index+1,
                        objecttype_to_string(invalid_arg_error.expected_type)
                    )
                );
            }
            else
            {
                error_handler->on_error
                (
                    x.offset, "{} ({}) is not accepted for argument {}, expected {}"_format
                    (
                        objecttype_to_string(actual_type),
                        invalid_arg_value->to_string(),
                        invalid_arg_error.argument_index+1,
                        objecttype_to_string(invalid_arg_error.expected_type)
                    )
                );
            }
            throw RuntimeError{};
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
        catch(RuntimeError&)
        {
            error_handler->on_note
            (
                x.offset,
                "called from here"
            );
            throw;
        }
    }


    std::shared_ptr<Object>
    on_constructor_expression(const ConstructorExpression& x) override
    {
        auto klass_object = evaluate(x.klass);
        
        auto klass = as_klass(klass_object);
        if(klass == nullptr)
        {
            error_handler->on_error
            (
                x.klass->offset,
                "{} is not a klass, evaluates to {}"_format
                (
                    objecttype_to_string(klass_object->get_type()),
                    klass_object->to_string()
                )
            );
            error_handler->on_note(x.offset, "constructor occured here");
            throw RuntimeError{};
        }

        std::vector<std::shared_ptr<Object>> arguments;
        for(auto& argument : x.arguments)
        { 
            arguments.emplace_back(evaluate(argument));
        }

        try
        {
            auto return_value = klass->constructor({arguments});
            return return_value;
        }
        catch(const InvalidArgumentType& invalid_arg_error)
        {
            auto invalid_arg_value = arguments[invalid_arg_error.argument_index];
            // special case for nil, no need to write the value of nil
            const auto actual_type = invalid_arg_value->get_type();
            if(actual_type == ObjectType::nil)
            {
                error_handler->on_error
                (
                    x.offset, "nil is not accepted for constructor argument {}, expected {}"_format
                    (
                        invalid_arg_error.argument_index+1,
                        objecttype_to_string(invalid_arg_error.expected_type)
                    )
                );
            }
            else
            {
                error_handler->on_error
                (
                    x.offset, "{} ({}) is not accepted for argument {}, expected {}"_format
                    (
                        objecttype_to_string(actual_type),
                        invalid_arg_value->to_string(),
                        invalid_arg_error.argument_index+1,
                        objecttype_to_string(invalid_arg_error.expected_type)
                    )
                );
            }
            throw RuntimeError{};
        }
        catch(const CallError& err)
        {
            error_handler->on_error(x.offset, err.error);
            error_handler->on_note(x.klass->offset, "called with {} arguments"_format(x.arguments.size()));
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
        catch(RuntimeError&)
        {
            error_handler->on_note
            (
                x.offset,
                "called from here"
            );
            throw;
        }
    }

    std::shared_ptr<Object>
    on_get_expression(const GetExpression& x) override
    {
        auto object = evaluate(x.object);
        if(WithProperties* props = object->get_properties_or_null(); props != nullptr)
        {
            auto r = props->get_property_or_null(x.name);

            if(r == nullptr)
            {
                // todo(Gustav): edit distance search for best named property
                error_handler->on_error
                (
                    x.offset, "{} doesn't have a property named {}"_format
                    (
                        object->to_string(),
                        x.name
                    )
                );
                throw RuntimeError{};
            }

            return r;
        }

        report_error_no_properties(x.offset, error_handler, object);
        throw RuntimeError{};
    }

    std::shared_ptr<Object>
    on_set_expression(const SetExpression& x) override
    {
        auto object = evaluate(x.object);

        WithProperties* props = object->get_properties_or_null();
        if(props == nullptr)
        {
            report_error_no_properties(x.offset, error_handler, object);
            throw RuntimeError{};
        }

        auto value = evaluate(x.value);
        try
        {
            const auto was_set = props->set_property_or_false(x.name, value);

            if(was_set == false)
            {
                // todo(Gustav): edit distance + custom error message?
                error_handler->on_error
                (
                    x.offset, "{} doesn't have a property named {}"_format
                    (
                        object->to_string(),
                        x.name
                    )
                );
                throw RuntimeError{};
            }
        }
        catch(const InvalidArgumentType& invalid_arg_error)
        {
            // special case for nil, no need to write the value of nil
            const auto actual_type = value->get_type();
            if(actual_type == ObjectType::nil)
            {
                error_handler->on_error
                (
                    x.offset, "nil is not accepted for property '{}' on {}, expected {}"_format
                    (
                        x.name,
                        object->to_string(),
                        objecttype_to_string(invalid_arg_error.expected_type)
                    )
                );
            }
            else
            {
                error_handler->on_error
                (
                    x.offset, "{} ({}) is not accepted for property '{}' on {}, expected {}"_format
                    (
                        objecttype_to_string(actual_type),
                        value->to_string(),
                        x.name,
                        object->to_string(),
                        objecttype_to_string(invalid_arg_error.expected_type)
                    )
                );
            }
            throw RuntimeError{};
        }

        return value;
    }

    std::shared_ptr<Object>
    on_super_expression(const SuperExpression& x) override
    {
        auto op_distance = lookup_distance_for_var(x);
        assert(op_distance);
        auto distance = *op_distance;
        assert(distance > 0);
        
        auto base_super = current_environment->get_at_or_null(distance, "super");
        assert(base_super != nullptr);
        assert(base_super->get_type() == ObjectType::klass);
        auto superklass = std::static_pointer_cast<Klass>(base_super);

        auto object = current_environment->get_at_or_null(distance-1, "this");
        assert(object != nullptr);
        assert(object->get_type() == ObjectType::instance);

        auto method = superklass->find_method_or_null(x.property);

        if(method == nullptr)
        {
            error_handler->on_error
            (
                x.offset, "{} doesn't have a property named {}"_format
                (
                    superklass->to_string(),
                    x.property
                )
            );
            throw RuntimeError{};
        }

        return method->bind(object);
    }
    
    std::shared_ptr<Object>
    on_this_expression(const ThisExpression& x) override
    {
        return lookup_var(*current_environment, "this", x);
    }

    std::shared_ptr<Object>
    on_logical_expression(const LogicalExpression& x) override
    {
        auto left = evaluate(x.left);

        switch(x.op)
        {
        case TokenType::OR:
            if (is_truthy(left))
            {
                return left;
            }
            break;
        case TokenType::AND:
            if (!is_truthy(left))
            {
                return left;
            }
            break;
        default:
            assert(false && "unknown logical operator in on_logical_expression(...)");
            break;
        }

        return evaluate(x.right);
    }

    std::shared_ptr<Object>
    on_assign_expression(const AssignExpression& x) override
    {
        auto value = evaluate(x.value);

        set_var_via_lookup(*current_environment, x.name, value, x);
        return value;
    }

    std::shared_ptr<Object>
    on_variable_expression(const VariableExpression& x) override
    {
        return lookup_var(*current_environment, x.name, x);
        // return get_var(*current_environment, x.name, x.offset);
    }

    std::shared_ptr<Object>
    on_binary_expression(const BinaryExpression& x) override
    {
        // todo(Gustav): make binary operators more flexible string*int should duplicate string
        // todo(Gustav): string + (other) should stringify other?
        auto left = evaluate(x.left);
        auto right = evaluate(x.right);

        switch(x.op)
        {
        case TokenType::MINUS:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            if(left->get_type() == ObjectType::number_float || right->get_type() == ObjectType::number_float)
            {
                return make_number_float( get_number_as_float(left) - get_number_as_float(right) );
            }
            else
            {
                return make_number_int( get_int_or_ub(left) - get_int_or_ub(right) );
            }
        case TokenType::SLASH:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return make_number_float( get_number_as_float(left) / get_number_as_float(right) );
        case TokenType::STAR:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return make_number_float( get_number_as_float(left) * get_number_as_float(right) );
        case TokenType::PLUS:
            check_binary_number_or_string_operands(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            if(is_number(left->get_type()) && is_number(right->get_type()))
            {
                if(left->get_type() == ObjectType::number_float || right->get_type() == ObjectType::number_float)
                {
                    return make_number_float( get_number_as_float(left) + get_number_as_float(right) );
                }
                else
                {
                    return make_number_int( get_int_or_ub(left) + get_int_or_ub(right) );
                }
            }
            else if(left->get_type() == ObjectType::string && right->get_type() == ObjectType::string)
            {
                return make_string( get_string_or_ub(left) + get_string_or_ub(right) );
            }
            else
            {
                assert(false && "interpreter error");
                return nullptr;
            }
        case TokenType::LESS:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return make_bool( get_number_as_float(left) < get_number_as_float(right) );
        case TokenType::LESS_EQUAL:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return make_bool( get_number_as_float(left) <= get_number_as_float(right) );
        case TokenType::GREATER:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return make_bool( get_number_as_float(left) > get_number_as_float(right) );
        case TokenType::GREATER_EQUAL:
            check_binary_number_operand(error_handler, x.op_offset, left, right, x.left->offset, x.right->offset);
            return make_bool( get_number_as_float(left) >= get_number_as_float(right) );
        case TokenType::BANG_EQUAL:
            return make_bool( !is_equal(left, right) );
        case TokenType::EQUAL_EQUAL:
            return make_bool( is_equal(left, right) );
        default:
            assert(false && "unhandled token in Interpreter::on_binary_expression(...)");
            return nullptr;
        }
    }

    std::shared_ptr<Object>
    on_grouping_expression(const GroupingExpression& x) override
    {
        return evaluate(x.expression);
    }

    std::shared_ptr<Object>
    on_literal_expression(const LiteralExpression& x) override
    {
        return x.value;
    }

    std::shared_ptr<Object>
    on_unary_expression(const UnaryExpression& x) override
    {
        auto right = evaluate(x.right);
        switch(x.op)
        {
        case TokenType::BANG:
            return make_bool(!is_truthy(right));
        case TokenType::MINUS:
            check_single_number_operand(error_handler, x.op_offset, right, x.right->offset);
            if(right->get_type() == ObjectType::number_float)
            {
                return make_number_float(-get_float_or_ub(right));
            }
            else
            {
                assert(right->get_type() == ObjectType::number_int);
                return make_number_int(-get_int_or_ub(right));
            }
        default:
            assert(false && "unreachable");
            return make_nil();
        }
    }


};


void
execute_statements_with_environment(MainInterpreter* main, const std::vector<std::shared_ptr<Statement>>& statements, std::shared_ptr<Environment> environment, std::shared_ptr<State> state)
{
    main->execute_statements_with_environment(statements, environment, state);
}

std::shared_ptr<Object>
interpret_initial_value(MainInterpreter* inter, const VarStatement& v)
{
    return inter->create_value(v);
}


struct PublicInterpreter : Interpreter
{
    ErrorHandler* error_handler;
    std::shared_ptr<Environment> global_environment;
    MainInterpreter interpreter;

    PublicInterpreter(ErrorHandler* eh, const std::function<void (std::string)>& on_line)
        : error_handler(eh)
        , global_environment(std::make_shared<Environment>(nullptr))
        , interpreter(global_environment, error_handler, on_line)
    {
    }

    Environment& get_global_environment() override
    {
        return *global_environment;
    }

    ErrorHandler* get_error_handler() override
    {
        return error_handler;
    }

    bool
    interpret(Program& program, const Resolved& resolved) override
    {
        try
        {
            auto state = std::make_shared<State>(resolved);
            interpreter.execute_statements_with_environment
            (
                program.statements, global_environment, state
            );
            return true;
        }
        catch (const RuntimeError&)
        {
            return false;
        }
    }
};


}}


namespace lox
{


std::shared_ptr<Interpreter>
make_interpreter
(
    ErrorHandler* error_handler,
    const std::function<void (std::string)>& on_line
)
{
    return std::make_shared<PublicInterpreter>(error_handler, on_line);
}



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



std::string
get_string_from_arg(const Arguments& args, u64 argument_index)
{
    assert(argument_index < args.arguments.size());
    auto value = as_string(args.arguments[argument_index]);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(argument_index, ObjectType::string);
    }
    return *value;
}


bool
get_bool_from_arg(const Arguments& args, u64 argument_index)
{
    assert(argument_index < args.arguments.size());
    auto value = as_bool(args.arguments[argument_index]);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(argument_index, ObjectType::boolean);
    }
    return *value;
}


Ti
get_int_from_arg(const Arguments& args, u64 argument_index)
{
    assert(argument_index < args.arguments.size());
    auto value = as_int(args.arguments[argument_index]);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(argument_index, ObjectType::number_int);
    }
    return *value;
}


Tf
get_float_from_arg(const Arguments& args, u64 argument_index)
{
    assert(argument_index < args.arguments.size());
    auto value = as_float(args.arguments[argument_index]);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(argument_index, ObjectType::number_float);
    }
    return *value;
}


std::shared_ptr<Callable>
get_callable_from_arg(const Arguments& args, u64 argument_index)
{
    assert(argument_index < args.arguments.size());
    auto value = as_callable(args.arguments[argument_index]);
    if(value == nullptr)
    {
        throw InvalidArgumentType(argument_index, ObjectType::callable);
    }
    return value;
}


std::string get_string_from_obj_or_error(std::shared_ptr<Object> obj)
{
    auto value = as_string(obj);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(0, ObjectType::string);
    }
    return *value;
}

bool get_bool_from_obj_or_error(std::shared_ptr<Object> obj)
{
    auto value = as_bool(obj);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(0, ObjectType::boolean);
    }
    return *value;
}

Tf get_float_from_obj_or_error(std::shared_ptr<Object> obj)
{
    auto value = as_float(obj);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(0, ObjectType::number_float);
    }
    return *value;
}

Ti get_int_from_obj_or_error(std::shared_ptr<Object> obj)
{
    auto value = as_int(obj);
    if(value.has_value() == false)
    {
        throw InvalidArgumentType(0, ObjectType::number_int);
    }
    return *value;
}


std::shared_ptr<Callable>get_callable_from_obj_or_error(std::shared_ptr<Object> obj)
{
    auto value = as_callable(obj);
    if(value != nullptr)
    {
        throw InvalidArgumentType(0, ObjectType::callable);
    }
    return value;
}



}

