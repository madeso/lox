#pragma once

#include "lox/object.h"

namespace lox
{

struct LoxImpl;
struct Interpreter;

struct Lox
{
    Lox(std::unique_ptr<ErrorHandler> error_handler, std::function<void (const std::string&)> on_line);
    ~Lox();

    bool run_string(const std::string& source);

    std::shared_ptr<Scope> in_global_scope();
    std::shared_ptr<Scope> in_package(const std::string& name);

    Environment& get_global_environment();

    std::shared_ptr<NativeKlass> get_native_klass_or_null(std::size_t id);

    template<typename T>
    std::shared_ptr<Object> make_native(T t)
    {
        auto klass = get_native_klass_or_null(detail::get_unique_id<T>());
        assert(klass != nullptr && "klass not registered!");
        return std::make_shared<detail::NativeInstanceT<T>>(std::move(t), klass);
    }

    Interpreter* get_interpreter();

    std::unique_ptr<LoxImpl> impl;
};


}
