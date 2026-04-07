#include "lax/lax.h"

#include "lax/printhandler.h"
#include "lax/scanner.h"
#include "lax/ast.h"
#include "lax/parser.h"
#include "lax/interpreter.h"
#include "lax/resolver.h"


namespace lax
{

struct LaxImpl
{
    std::unique_ptr<ErrorHandler> error_handler;
    std::shared_ptr<Interpreter> interpreter;

    LaxImpl(std::unique_ptr<ErrorHandler> eh, std::function<void (const std::string&)> on_line)
        : error_handler(std::move(eh))
        , interpreter(lax::make_interpreter(error_handler.get(), std::move(on_line)))
    {
    }
};


Lax::Lax(std::unique_ptr<ErrorHandler> eh, std::function<void (const std::string&)> on_line)
    : impl(std::make_unique<LaxImpl>(std::move(eh), std::move(on_line)))
{
}

Lax::~Lax() = default;


bool
Lax::run_string(const std::string& source)
{
    auto tokens = lax::scan_lox_tokens(source, impl->interpreter->get_error_handler());
    auto program = lax::parse_program(tokens.tokens, impl->interpreter->get_error_handler());
    
    if(tokens.errors > 0 || program.errors > 0)
    {
        return false;
    }

    auto resolved = lax::resolve(*program.program, impl->interpreter->get_error_handler());

    if(resolved.has_value() == false)
    {
        return false;
    }

    return impl->interpreter->interpret(*program.program, *resolved);
}


std::shared_ptr<Scope>
Lax::in_global_scope()
{
    return get_global_scope(impl->interpreter.get());
}


std::shared_ptr<Scope>
Lax::in_package(const std::string& package_path)
{
    return get_package_scope_from_known_path(impl->interpreter.get(), package_path);
}


Environment&
Lax::get_global_environment()
{
    return impl->interpreter->get_global_environment();
}

std::shared_ptr<NativeKlass>
Lax::get_native_klass_or_null(std::size_t id)
{
    return impl->interpreter->get_native_klass_or_null(id);
}

Interpreter*
Lax::get_interpreter()
{
    return impl->interpreter.get();
}

}

