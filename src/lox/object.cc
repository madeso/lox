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

struct NativeFunction : Callable
{
    std::string name;
    std::function<std::shared_ptr<Object>(const Arguments& arguments)> func;

    NativeFunction
    (
        const std::string& n,
        std::function<std::shared_ptr<Object>(const Arguments& arguments)> f
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
        return func(arguments);
    }

    std::shared_ptr<Callable>
    bind(std::shared_ptr<Object>) override
    {
        assert(false && "figure out how to make bind native function!");
        return std::static_pointer_cast<Callable>(shared_from_this());
    }
};


}}




// ----------------------------------------------------------------------------



namespace lox
{



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


// ----------------------------------------------------------------------------



Klass::Klass(const std::string& n)
    : name(n)
{
}

ObjectType
Klass::get_type() const
{
    return ObjectType::klass;
}

std::string
Klass::to_string() const
{
    return "<class {}>"_format(name);
}

std::shared_ptr<Object>
Klass::call(const Arguments& arguments)
{
    auto self = shared_from_this();
    assert(self != nullptr);
    assert(self->get_type() == ObjectType::klass);
    auto klass = std::static_pointer_cast<Klass>(self);
    return constructor(klass, arguments);
}

std::shared_ptr<Callable>
Klass::bind(std::shared_ptr<Object>)
{
    assert(false && "figure out how to make bind a klass!");
    return std::static_pointer_cast<Callable>(shared_from_this());
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


std::shared_ptr<Object>
make_native_function
(
    const std::string& name,
    std::function<std::shared_ptr<Object>(const Arguments& arguments)>&& func
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
