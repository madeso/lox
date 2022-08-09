#include "lox/lox.h"

#include "lox/printhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"
#include "lox/interpreter.h"
#include "lox/resolver.h"


namespace lox
{


// ----------------------------------------------------------------------------

void
define_native_function
(
    const std::string& name,
    Environment& env,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
)
{
    env.define(name, make_native_function(name, std::move(func)));
}


ArgumentHelper::ArgumentHelper(const lox::Arguments& aargs)
    : args(aargs)
    , next_argument(0)
    , has_read_all_arguments(false)
{
}

ArgumentHelper::~ArgumentHelper()
{
    assert(has_read_all_arguments && "complete() not called");
}

void
ArgumentHelper::complete()
{
    assert(has_read_all_arguments==false && "complete() called twice!");
    has_read_all_arguments = true;
    verify_number_of_arguments(args, next_argument);
}

std::string
ArgumentHelper::require_string()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return ""; }
    return get_string_from_arg(args, argument_index);
}

bool
ArgumentHelper::require_bool()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return false; }
    return get_bool_from_arg(args, argument_index);
}

float
ArgumentHelper::require_number()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return 0.0f; }
    return get_number_from_arg(args, argument_index);
}

std::shared_ptr<Callable>
ArgumentHelper::require_callable()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_callable_from_arg(args, argument_index);
}


// ----------------------------------------------------------------------------


Lox::Lox(std::unique_ptr<ErrorHandler> eh, std::function<void (const std::string&)> on_line)
    : error_handler(std::move(eh)),
    interpreter(lox::make_interpreter(error_handler.get(), std::move(on_line)))
{
}

Lox::~Lox() = default;


bool
Lox::run_string(const std::string& source)
{
    auto tokens = lox::scan_tokens(source, interpreter->get_error_handler());
    auto program = lox::parse_program(tokens.tokens, interpreter->get_error_handler());
    
    if(tokens.errors > 0 || program.errors > 0)
    {
        return false;
    }

    auto resolved = lox::resolve(*program.program, interpreter->get_error_handler());

    if(resolved.has_value() == false)
    {
        return false;
    }

    return interpreter->interpret(*program.program, *resolved);
}


void
Lox::define_global_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
)
{
    define_native_function(name, get_global_environment(), std::move(func));
}


Environment&
Lox::get_global_environment()
{
    return interpreter->get_global_environment();
}


}
