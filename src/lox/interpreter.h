#pragma once

#include <functional>

#include "lox/ints.h"
#include "lox/environment.h"


namespace lox
{

struct ErrorHandler;
struct Program;
struct Resolved;


// runtime errors are sent to the error handler
// return false on runtime error


struct Interpreter
{
    Interpreter() = default;
    virtual ~Interpreter() = default;
    
    virtual Environment& get_global_environment() = 0;
    virtual ErrorHandler* get_error_handler() = 0;

    virtual bool
    interpret
    (
        Program& expression, const Resolved& resolved
    ) = 0;

    virtual std::shared_ptr<NativeKlass> get_native_klass_or_null(std::size_t id) = 0;
    virtual void register_native_klass(std::size_t id, std::shared_ptr<NativeKlass> klass) = 0;
};


std::shared_ptr<Interpreter>
make_interpreter
(
    ErrorHandler* error_handler,
    const std::function<void (std::string)>& on_line
);


void
verify_number_of_arguments(const Arguments& args, u64 arity);


std::shared_ptr<Object>   get_object_from_arg    (const Arguments& args, u64 argument_index);
std::shared_ptr<Instance> get_instance_from_arg  (const Arguments& args, u64 argument_index);
std::string               get_string_from_arg    (const Arguments& args, u64 argument_index);
bool                      get_bool_from_arg      (const Arguments& args, u64 argument_index);
Ti                        get_int_from_arg       (const Arguments& args, u64 argument_index);
Tf                        get_float_from_arg     (const Arguments& args, u64 argument_index);
std::shared_ptr<Callable> get_callable_from_arg  (const Arguments& args, u64 argument_index);
std::shared_ptr<NativeInstance> get_native_instance_from_arg(const Arguments& args, u64 argument_index, std::size_t klass);

std::string               get_string_from_obj_or_error    (std::shared_ptr<Object> obj);
bool                      get_bool_from_obj_or_error      (std::shared_ptr<Object> obj);
Tf                        get_float_from_obj_or_error     (std::shared_ptr<Object> obj);
Ti                        get_int_from_obj_or_error       (std::shared_ptr<Object> obj);
std::shared_ptr<Callable> get_callable_from_obj_or_error  (std::shared_ptr<Object> obj);

}
