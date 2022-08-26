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

namespace detail
{
    std::size_t create_type_id()
    {
        static std::size_t next_id = 1;
        return next_id++;
    }

    template<> std::string get_from_obj_or_error<std::string>(std::shared_ptr<Object> obj) { return lox::get_string_from_obj_or_error(obj); }
    template<> bool get_from_obj_or_error<bool>(std::shared_ptr<Object> obj) { return lox::get_bool_from_obj_or_error(obj); }
    template<> float get_from_obj_or_error<float>(std::shared_ptr<Object> obj) { return lox::get_number_from_obj_or_error(obj); }

    template<> std::shared_ptr<Object> make_object<std::string>(std::string str) { return lox::make_string(str); }
    template<> std::shared_ptr<Object> make_object<bool>(bool b) { return lox::make_bool(b); }
    template<> std::shared_ptr<Object> make_object<float>(float n) { return lox::make_number(n); }
}

// ----------------------------------------------------------------------------


struct NativeKlassImpl : NativeKlass
{
    std::function<std::shared_ptr<Object> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)> constr;

    NativeKlassImpl
    (
        const std::string& name,
        std::size_t id,
        std::function<std::shared_ptr<Object> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)>&& c    
    )
    : NativeKlass(name, id, nullptr)
    , constr(std::move(c))
    {
    }

    std::shared_ptr<Object> constructor(const Arguments& arguments) override
    {
        auto obj = shared_from_this();
        assert(obj.get() == this);
        auto self = std::static_pointer_cast<NativeKlassImpl>(obj);
        ArgumentHelper ah{arguments};
        return constr(self, ah);
    }
};


struct NativePackage : Object, WithProperties, Scope
{
    std::string package_name;
    std::unordered_map<std::string, std::shared_ptr<Object>> members;

    explicit NativePackage(const std::string& name)
        : package_name(name)
    {
    }

    ObjectType
    get_type() const override
    {
        return ObjectType::native_package;
    }

    std::string
    to_string() const override
    {
        return "<native pkg {}>"_format(package_name);
    }
    
    bool
    is_callable() const override
    {
        return false;
    }
    
    WithProperties*
    get_properties_or_null() override
    {
        return this;
    }

    std::shared_ptr<Object>
    get_property_or_null(const std::string& name) override
    {
        if(auto found = members.find(name); found != members.end())
        {
            return found->second;
        }
        else
        {
            return nullptr;
        }
    }

    bool
    set_property_or_false(const std::string&, std::shared_ptr<Object>) override
    {
        return false;
    }

    void
    set_property(const std::string& name, std::shared_ptr<Object> value) override
    {
        assert(members.find(name) == members.end() && "error: member already added");
        members.insert({name, value});
    }
};

void
Scope::define_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)>&& func
)
{
    set_property(name, make_native_function(name, std::move(func)));
}

std::shared_ptr<NativeKlass>
Scope::register_native_klass_impl
(
    const std::string& name,
    std::size_t id,
    std::function<std::shared_ptr<Object> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)>&& c
)
{
    auto new_klass = std::make_shared<NativeKlassImpl>(name, id, std::move(c));
    set_property(name, new_klass);
    return new_klass;
}

struct GlobalScope : Scope
{
    Environment& global;

    explicit GlobalScope(Environment& g)
        : global(g)
    {
    }

    void
    set_property(const std::string& name, std::shared_ptr<Object> value) override
    {
        global.define(name, value);
    }
};

struct LoxImpl
{
    std::unique_ptr<ErrorHandler> error_handler;
    std::shared_ptr<Interpreter> interpreter;
    std::unique_ptr<GlobalScope> global;

    LoxImpl(std::unique_ptr<ErrorHandler> eh, std::function<void (const std::string&)> on_line)
        : error_handler(std::move(eh))
        , interpreter(lox::make_interpreter(error_handler.get(), std::move(on_line)))
    {
        global = std::make_unique<GlobalScope>(interpreter->get_global_environment());
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



Scope*
Lox::in_global_scope()
{
    return impl->global.get();
}

std::vector<std::string>
parse_package_path(const std::string& path)
{
    const auto package_path = scan_tokens(path, nullptr);
    if(package_path.errors != 0) { return {}; }
    if(package_path.tokens.empty()) { return {}; }

    std::size_t token_index=0;
    const auto tok = [&]() -> const Token& { return package_path.tokens[token_index]; };
    const auto has_more = [&]() -> bool { return token_index <= package_path.tokens.size(); };

    if(tok().type != TokenType::IDENTIFIER) { return {}; }
    std::vector<std::string> ret = {std::string(tok().lexeme)};

    token_index += 1;
    while(has_more())
    {
        if(tok().type == TokenType::EOF) { return ret; }
        if(tok().type != TokenType::DOT) { return {}; }

        token_index += 1;
        
        if(tok().type != TokenType::IDENTIFIER) { return {}; }
        ret.emplace_back(std::string(tok().lexeme));

        token_index += 1;
    }

    assert(false && "parser error");
    return {};
}

std::shared_ptr<Scope>
Lox::in_package(const std::string& package_path)
{
    const auto path = parse_package_path(package_path);
    assert(path.empty()==false && "invalid path syntax");

    std::shared_ptr<NativePackage> package = nullptr;

    for(const auto& name: path)
    {
        const bool use_global = package == nullptr;
        std::shared_ptr<Object> object = use_global
            ? get_global_environment().get_at_or_null(0, name)
            : package->get_property_or_null(name)
            ;
        if(object == nullptr)
        {
            auto new_package = std::make_shared<NativePackage>(name);

            if(use_global)
            {
                get_global_environment().define(name, new_package);
            }
            else
            {
                package->members.insert({name, new_package});
            }

            package = new_package;
        }
        else if(object->get_type() == ObjectType::native_package)
        {
            package = std::static_pointer_cast<NativePackage>(object);
        }
        else
        {
            assert(false && "named package was set to something other than a package");
            return nullptr;
        }
    }
    assert(package != nullptr);
    return package;
}

Environment&
Lox::get_global_environment()
{
    return impl->interpreter->get_global_environment();
}


}

