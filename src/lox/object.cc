#include "lox/object.h"

#include "lox/interpreter.h"
#include "lox/scanner.h"


namespace lox { namespace
{


// ----------------------------------------------------------------------------

struct Nil : public Object
{
    Nil() = default;
    virtual ~Nil() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
    bool is_callable() const override { return false; }
};


struct String : public Object
{
    std::string value;

    explicit String(const std::string& s);
    virtual ~String() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
    bool is_callable() const override { return false; }
};


struct Bool : public Object
{
    bool value;

    explicit Bool(bool b);
    virtual ~Bool() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
    bool is_callable() const override { return false; }
};


struct NumberInt : public Object
{
    Ti value;

    explicit NumberInt(Ti f);
    virtual ~NumberInt() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
    bool is_callable() const override { return false; }
};

struct NumberFloat : public Object
{
    Tf value;

    explicit NumberFloat(Tf f);
    virtual ~NumberFloat() = default;

    ObjectType get_type() const override;
    std::string to_string() const override;
    bool is_callable() const override { return false; }
};


}}




// ----------------------------------------------------------------------------



namespace lox
{


struct NativeFunction : Callable
{
    std::string name;
    std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)> func;

    NativeFunction
    (
        const std::string& n,
        std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)> f
    )
        : name(n)
        , func(f)
    {
    }

    std::string
    to_string() const override
    {
        return "<native fun {}>"_format(name);
    }

    std::shared_ptr<Object>
    perform_call(Callable* callable, const Arguments& arguments)
    {
        ArgumentHelper helper{arguments};

        auto ret = func(callable, helper);
        helper.verify_completion();
        
        return ret;
    }

    std::shared_ptr<Object>
    call(const Arguments& arguments) override
    {
        return perform_call(this, arguments);
    }

    std::shared_ptr<Callable>
    bind(std::shared_ptr<Object> bound) override
    {
        auto self = std::static_pointer_cast<NativeFunction>(shared_from_this());
        return std::make_shared<BoundCallable>(bound, self);
    }
};

// ----------------------------------------------------------------------------



ObjectType
Nil::get_type() const
{
    return ObjectType::nil;
}

std::string
Nil::to_string() const
{
    return "nil";
}


// ----------------------------------------------------------------------------



String::String(const std::string& s)
    : value(s)
{
}

ObjectType
String::get_type() const
{
    return ObjectType::string;
}

std::string
String::to_string() const
{
    return value;
}



// ----------------------------------------------------------------------------



Bool::Bool(bool b)
    : value(b)
{
}

ObjectType
Bool::get_type() const
{
    return ObjectType::boolean;
}

std::string
Bool::to_string() const
{
    if(value) { return "true"; }
    else { return "false"; }
}



// ----------------------------------------------------------------------------



NumberInt::NumberInt(Ti f)
    : value(f)
{
}

ObjectType
NumberInt::get_type() const
{
    return ObjectType::number_int;
}

std::string
NumberInt::to_string() const
{
    return "{0}"_format(value);
}


// ----------------------------------------------------------------------------



NumberFloat::NumberFloat(Tf f)
    : value(f)
{
}

ObjectType
NumberFloat::get_type() const
{
    return ObjectType::number_float;
}

std::string
NumberFloat::to_string() const
{
    return "{0}"_format(value);
}



// ----------------------------------------------------------------------------


WithProperties*
Object::get_properties_or_null()
{
    return nullptr;
}



// ----------------------------------------------------------------------------



ObjectType Callable::get_type() const
{
    return ObjectType::callable;
}


bool Callable::is_callable() const
{
    return true;
}


bool Callable::is_bound() const
{
    return false;
}

// ----------------------------------------------------------------------------


BoundCallable::BoundCallable(std::shared_ptr<Object> b, std::shared_ptr<NativeFunction> c)
    : bound(b)
    , callable(c)
{
    assert(bound);
    assert(callable);
}

BoundCallable::~BoundCallable() = default;

std::string
BoundCallable::to_string() const
{
    return "<{} bound to {}>"_format(bound->to_string(), callable->to_string());
}

std::shared_ptr<Object>
BoundCallable::call(const Arguments& arguments)
{
    return callable->perform_call(this, arguments);
}

std::shared_ptr<Callable> BoundCallable::bind(std::shared_ptr<Object>)
{
    assert(false && "unable to find a already bound object");
    auto self = std::static_pointer_cast<Callable>(shared_from_this());
    return self;
}

bool BoundCallable::is_bound() const
{
    return true;
}



// ----------------------------------------------------------------------------



Klass::Klass
(
    const std::string& n,
    std::shared_ptr<Klass> s
)
    : klass_name(n)
    , superklass(s)
{
}

ObjectType
Klass::get_type() const
{
    return ObjectType::klass;
}

bool
Klass::is_callable() const
{
    return false;
}

bool
Klass::add_method_or_false(const std::string& name, std::shared_ptr<Callable> method)
{
    if(methods.find(name) == methods.end())
    {
        methods.insert({name, method});
        return true;
    }
    else
    {
        return false;
    }
}

std::shared_ptr<Callable>
Klass::find_method_or_null(const std::string& method_name)
{
    if(auto found = methods.find(method_name); found != methods.end())
    {
        return found->second;
    }

    if(superklass != nullptr)
    {
        return superklass->find_method_or_null(method_name);
    }

    return nullptr;
}


bool
Klass::add_static_method_or_false(const std::string& name, std::shared_ptr<Callable> method)
{
    if(static_methods.find(name) == static_methods.end())
    {
        // static_methods.insert({name, method});
        static_methods.insert({name, std::move(method)});
        return true;
    }
    else
    {
        return false;
    }
}


WithProperties*
Klass::get_properties_or_null()
{
    return this;
}


std::shared_ptr<Object>
Klass::get_property_or_null(const std::string& method_name)
{
    if(auto found = static_methods.find(method_name); found != static_methods.end())
    {
        return found->second;
    }

    return nullptr;
}


bool
Klass::set_property_or_false(const std::string&, std::shared_ptr<Object>)
{
    return false;
}



// ----------------------------------------------------------------------------


Instance::Instance(std::shared_ptr<Klass> o)
    : klass(o)
{
}

bool
Instance::is_callable() const
{
    return false;
}

WithProperties* Instance::get_properties_or_null()
{
    return this;
}

std::shared_ptr<Object> Instance::get_property_or_null(const std::string& name)
{
    if(auto found = get_field_or_null(name); found != nullptr)
    {
        return found;
    }

    if(auto method = klass->find_method_or_null(name); method != nullptr)
    {
        auto self = shared_from_this();
        assert(self != nullptr);
        return method->bind(self);
    }

    return nullptr;
}

bool Instance::set_property_or_false(const std::string& name, std::shared_ptr<Object> value)
{
    return set_field_or_false(name, value);
}


// ----------------------------------------------------------------------------


NativeKlass::NativeKlass(const std::string& n, std::size_t id, std::shared_ptr<Klass> sk)
        : Klass(n, sk)
        , native_id(id)
{
}

std::string
NativeKlass::to_string() const
{
    return "<native class {}>"_format(klass_name);
}


void
NativeKlass::add_property(const std::string& name, std::unique_ptr<Property> prop)
{
    assert(properties.find(name) == properties.end());
    properties.insert({name, std::move(prop)});
}


NativeInstance::NativeInstance(std::shared_ptr<NativeKlass> o)
    : Instance(o)
{
}

ObjectType NativeInstance::get_type() const
{
    return ObjectType::native_instance;
}

std::string NativeInstance::to_string() const
{
    return "<native instance {}>"_format(klass->klass_name);
}


std::shared_ptr<Object> NativeInstance::get_field_or_null(const std::string& name)
{
    NativeKlass* nk = static_cast<NativeKlass*>(klass.get());
    if(auto found = nk->properties.find(name); found != nk->properties.end())
    {
        return found->second->get_value(this);
    }
    else
    {
        return nullptr;
    }
}

bool NativeInstance::set_field_or_false(const std::string& name, std::shared_ptr<Object> value)
{
    NativeKlass* nk = static_cast<NativeKlass*>(klass.get());
    if(auto found = nk->properties.find(name); found != nk->properties.end())
    {
        // todo(Gustav): what happens if the value is a not-supported type... error how?
        found->second->set_value(this, value);
        return true;
    }
    else
    {
        return false;
    }
}


// ----------------------------------------------------------------------------


std::shared_ptr<Object>
make_nil()
{
    return std::make_shared<Nil>();
}


std::shared_ptr<Object>
make_string(const std::string& str)
{
    return std::make_shared<String>(str);
}


std::shared_ptr<Object>
make_bool(bool b)
{
    return std::make_shared<Bool>(b);
}


std::shared_ptr<Object>
make_number_int(Ti f)
{
    return std::make_shared<NumberInt>(f);
}

std::shared_ptr<Object>
make_number_float(Tf f)
{
    return std::make_shared<NumberFloat>(f);
}


std::shared_ptr<Callable>
make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)>&& func
)
{
    return std::make_shared<NativeFunction>(name, func);
}


// ----------------------------------------------------------------------------


bool
is_nil(std::shared_ptr<Object> o)
{
    return o->get_type() == ObjectType::nil;
}

std::optional<std::string>
as_string(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::string) { return std::nullopt; }
    auto* ptr = static_cast<String*>(o.get());
    return ptr->value;
}

std::optional<bool>
as_bool(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::boolean) { return std::nullopt; }
    auto* ptr = static_cast<Bool*>(o.get());
    return ptr->value;
}

std::optional<Ti>
as_int(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::number_int) { return std::nullopt; }
    auto* ptr = static_cast<NumberInt*>(o.get());
    return ptr->value;
}

std::optional<Tf>
as_float(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::number_float) { return std::nullopt; }
    auto* ptr = static_cast<NumberFloat*>(o.get());
    return ptr->value;
}

std::shared_ptr<Callable>
as_callable(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->is_callable() == false) { return nullptr; }
    return std::static_pointer_cast<Callable>(o);
}

std::shared_ptr<Klass>
as_klass(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::klass) { return nullptr; }
    return std::static_pointer_cast<Klass>(o);
}

std::shared_ptr<NativeInstance>
as_native_instance_of_type(std::shared_ptr<Object> o, std::size_t type)
{
    assert(o != nullptr);
    
    if(o->get_type() != ObjectType::native_instance) { return nullptr; }
    
    auto instance = std::static_pointer_cast<NativeInstance>(o);
    auto klass = std::static_pointer_cast<NativeKlass>(instance->klass);
    
    if(klass->native_id != type) { return nullptr; }
    
    return instance;
}

// ----------------------------------------------------------------------------


Ti
get_int_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::number_int);
    return static_cast<NumberInt*>(o.get())->value;
}

Tf
get_float_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::number_float);
    return static_cast<NumberFloat*>(o.get())->value;
}


std::string
get_string_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::string);
    return static_cast<String*>(o.get())->value;
}


bool
get_bool_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::boolean);
    return static_cast<Bool*>(o.get())->value;
}


bool
is_truthy(std::shared_ptr<Object> o)
{
    switch(o->get_type())
    {
    case ObjectType::nil:
        return false;
    case ObjectType::boolean:
        return static_cast<const Bool*>(o.get())->value;
    default:
        return true;
    }
}

// ----------------------------------------------------------------------------


ArgumentHelper::ArgumentHelper(const lox::Arguments& aargs)
    : args(aargs)
    , next_argument(0)
    , has_read_all_arguments(false)
{
}

void ArgumentHelper::verify_completion()
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

Ti
ArgumentHelper::require_int()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return 0; }
    return get_int_from_arg(args, argument_index);
}

Tf
ArgumentHelper::require_float()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return 0.0; }
    return get_float_from_arg(args, argument_index);
}

std::shared_ptr<Callable>
ArgumentHelper::require_callable()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_callable_from_arg(args, argument_index);
}

std::shared_ptr<NativeInstance>
ArgumentHelper::impl_require_native_instance(std::size_t klass)
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_native_instance_from_arg(args, argument_index, klass);
}

// ----------------------------------------------------------------------------



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
    template<> Tf get_from_obj_or_error<Tf>(std::shared_ptr<Object> obj) { return lox::get_float_from_obj_or_error(obj); }
    template<> Ti get_from_obj_or_error<Ti>(std::shared_ptr<Object> obj) { return lox::get_int_from_obj_or_error(obj); }

    template<> std::shared_ptr<Object> make_object<std::string>(std::string str) { return lox::make_string(str); }
    template<> std::shared_ptr<Object> make_object<bool>(bool b) { return lox::make_bool(b); }
    template<> std::shared_ptr<Object> make_object<Tf>(Tf n) { return lox::make_number_float(n); }
    template<> std::shared_ptr<Object> make_object<Ti>(Ti n) { return lox::make_number_int(n); }
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

    NativePackage(Interpreter* inter, const std::string& name)
        : Scope(inter)
        , package_name(name)
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

Scope::Scope(Interpreter* inter)
    : interpreter(inter)
{
    assert(interpreter);
}

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
    interpreter->register_native_klass(id, new_klass);
    return new_klass;
}

struct GlobalScope : Scope
{
    Environment& global;

    GlobalScope(Interpreter* inter, Environment& g)
        : Scope(inter)
        , global(g)
    {
    }

    void
    set_property(const std::string& name, std::shared_ptr<Object> value) override
    {
        global.define(name, value);
    }
};


// ----------------------------------------------------------------------------

std::shared_ptr<Scope>
get_global_scope(Interpreter* interpreter)
{
    return std::make_shared<GlobalScope>(interpreter, interpreter->get_global_environment());
}



std::shared_ptr<Scope>
get_package_scope_from_known_path(Interpreter* interpreter, const std::string& package_path)
{
    const auto path = parse_package_path(package_path);
    assert(path.empty()==false && "invalid path syntax");

    std::shared_ptr<NativePackage> package = nullptr;

    Environment& global_environment = interpreter->get_global_environment();

    for(const auto& name: path)
    {
        const bool use_global = package == nullptr;
        std::shared_ptr<Object> object = use_global
            ? global_environment.get_at_or_null(0, name)
            : package->get_property_or_null(name)
            ;
        if(object == nullptr)
        {
            auto new_package = std::make_shared<NativePackage>(interpreter, name);

            if(use_global)
            {
                global_environment.define(name, new_package);
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

// ----------------------------------------------------------------------------


}
