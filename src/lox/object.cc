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


struct Number : public Object
{
    float value;

    explicit Number(float f);
    virtual ~Number() = default;

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
    std::function<std::shared_ptr<Object>(Callable*, const Arguments& arguments)> func;

    NativeFunction
    (
        const std::string& n,
        std::function<std::shared_ptr<Object>(Callable*, const Arguments& arguments)> f
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
    call(const Arguments& arguments) override
    {
        return func(this, arguments);
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



Number::Number(float f)
    : value(f)
{
}

ObjectType
Number::get_type() const
{
    return ObjectType::number;
}

std::string
Number::to_string() const
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
    return callable->func(this, arguments);
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

std::shared_ptr<Object>
Klass::call(const Arguments& arguments)
{
    return constructor(arguments);
}

std::shared_ptr<Callable>
Klass::bind(std::shared_ptr<Object>)
{
    assert(false && "figure out how to make bind a klass!");
    return std::static_pointer_cast<Callable>(shared_from_this());
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
make_number(float f)
{
    return std::make_shared<Number>(f);
}


std::shared_ptr<Callable>
make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(Callable*, const Arguments& arguments)>&& func
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

std::optional<float>
as_number(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::number) { return std::nullopt; }
    auto* ptr = static_cast<Number*>(o.get());
    return ptr->value;
}

std::shared_ptr<Callable>
as_callable(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->is_callable() == false) { return nullptr; }
    return std::static_pointer_cast<Callable>(o);
}


// ----------------------------------------------------------------------------


float
get_number_or_ub(std::shared_ptr<Object> o)
{
    assert(o->get_type() == ObjectType::number);
    return static_cast<Number*>(o.get())->value;
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
// ----------------------------------------------------------------------------

}
