#include "lox/object.h"

#include "interpreter.h"


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

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

}
