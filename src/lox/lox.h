#pragma once

#include "lox/object.h"


namespace lox
{


struct Interpreter;
struct ErrorHandler;


void
define_native_function
(
    const std::string& name,
    Environment& env,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
);


struct ArgumentHelper
{
    const lox::Arguments& args;
    u64 next_argument;
    bool has_read_all_arguments;

    explicit ArgumentHelper(const lox::Arguments& args);
    ~ArgumentHelper();

    // todo(Gustav): add some match/switch helper to handle overloads

    std::string                 require_string   ();
    bool                        require_bool     ();
    float                       require_number   ();
    std::shared_ptr<Callable>   require_callable ();

    void complete();

    ArgumentHelper(ArgumentHelper&&) = delete;
    ArgumentHelper(const ArgumentHelper&) = delete;
    void operator=(ArgumentHelper&&) = delete;
    void operator=(const ArgumentHelper&) = delete;
};


struct Lox
{
    Lox(std::unique_ptr<ErrorHandler> error_handler, std::function<void (const std::string&)> on_line);
    ~Lox();

    bool run_string(const std::string& source);

    void
    define_global_native_function
    (
        const std::string& name,
        std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
    );

    Environment& get_global_environment();

    std::unique_ptr<ErrorHandler> error_handler;
    std::shared_ptr<Interpreter> interpreter;
};


}
