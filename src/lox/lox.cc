#include "lox/lox.h"

#include "lox/printhandler.h"
#include "lox/scanner.h"
#include "lox/ast.h"
#include "lox/parser.h"
#include "lox/interpreter.h"
#include "lox/resolver.h"


namespace lox
{

struct LoxImpl
{
    std::unique_ptr<ErrorHandler> error_handler;
    std::shared_ptr<Interpreter> interpreter;

    LoxImpl(std::unique_ptr<ErrorHandler> eh, std::function<void (const std::string&)> on_line)
        : error_handler(std::move(eh))
        , interpreter(lox::make_interpreter(error_handler.get(), std::move(on_line)))
    {
    }
};


Lox::Lox(std::unique_ptr<ErrorHandler> eh, std::function<void (const std::string&)> on_line)
    : impl(std::make_unique<LoxImpl>(std::move(eh), std::move(on_line)))
{
}

Lox::~Lox() = default;


bool
Lox::run_string(const std::string& source)
{
    auto tokens = lox::scan_tokens(source, impl->interpreter->get_error_handler());
    auto program = lox::parse_program(tokens.tokens, impl->interpreter->get_error_handler());
    
    if(tokens.errors > 0 || program.errors > 0)
    {
        return false;
    }

    auto resolved = lox::resolve(*program.program, impl->interpreter->get_error_handler());

    if(resolved.has_value() == false)
    {
        return false;
    }

    return impl->interpreter->interpret(*program.program, *resolved);
}


std::shared_ptr<Scope>
Lox::in_global_scope()
{
    return get_global_scope(impl->interpreter.get());
}


std::shared_ptr<Scope>
Lox::in_package(const std::string& package_path)
{
    return get_package_scope_from_known_path(impl->interpreter.get(), package_path);
}


Environment&
Lox::get_global_environment()
{
    return impl->interpreter->get_global_environment();
}


}

