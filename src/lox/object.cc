#include "lox/object.h"

#include <sstream>

#include "lox/interpreter.h"
#include "lox/scanner.h"


namespace lox
{


struct ObjectIntegrationImpl : ObjectIntegration
{
    std::unordered_map<std::string, std::shared_ptr<Callable>> array_methods;
    
    ObjectIntegrationImpl()
    {
        register_functions();
    }

    void add_array_method
    (
        const std::string& name,
        std::function<std::shared_ptr<Object>(Callable*, ArgumentHelper& arguments)>&& func
    )
    {
        assert(array_methods.find(name) == array_methods.end());
        array_methods.insert({name, make_native_function(name, std::move(func))});
    }

    std::shared_ptr<Callable>
    find_array_method_or_null(const std::string& name)
    {
        auto found = array_methods.find(name);
        if(found == array_methods.end())
        {
            return nullptr;
        }

        return found->second;
    }

    void register_functions();

    std::shared_ptr<Object>
    make_array(std::vector<std::shared_ptr<Object>>&& values) override;
};


}


namespace lox { namespace
{


// ----------------------------------------------------------------------------

std::string quote_string(const std::string& str)
{
    std::ostringstream ss;
    ss << '\"';
    for(const auto c: str)
    {
        switch(c)
        {
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '"':
            ss << "\\\"";
            break;
        default:
            ss << c;
            break;
        }
    }
    ss << '\"';
    return ss.str();
}


// ----------------------------------------------------------------------------

struct Nil : public Object
{
    Nil() = default;
    virtual ~Nil() = default;

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;
    bool is_callable() const override { return false; }
};


struct String : public Object
{
    std::string value;

    explicit String(const std::string& s);
    virtual ~String() = default;

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;
    bool is_callable() const override { return false; }
};


struct Bool : public Object
{
    bool value;

    explicit Bool(bool b);
    virtual ~Bool() = default;

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;
    bool is_callable() const override { return false; }
};



struct NumberInt : public Object
{
    Ti value;

    explicit NumberInt(Ti f);
    virtual ~NumberInt() = default;

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;
    bool is_callable() const override { return false; }
};

struct NumberFloat : public Object
{
    Tf value;

    explicit NumberFloat(Tf f);
    virtual ~NumberFloat() = default;

    ObjectType get_type() const override;
    std::vector<std::string> to_string(const ToStringOptions&) const override;
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

    std::vector<std::string>
    to_string(const ToStringOptions&) const override
    {
        return {"<native fun {}>"_format(name)};
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

std::vector<std::string>
Nil::to_string(const ToStringOptions&) const
{
    return {"nil"};
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

std::vector<std::string>
String::to_string(const ToStringOptions& tso) const
{
    if(tso.quote_string)
    {
        return {quote_string(value)};
    }
    else
    {
        return {value};
    }
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

std::vector<std::string>
Bool::to_string(const ToStringOptions&) const
{
    if(value) { return {"true"}; }
    else { return {"false"}; }
}



// ----------------------------------------------------------------------------


Array::Array(ObjectIntegrationImpl* i, std::vector<std::shared_ptr<Object>>&& v)
    : integration(i)
    , values(v)
{
}

ObjectType
Array::get_type() const
{
    return ObjectType::array;
}


std::optional<std::string>
Array::to_flat_string_representation(const ToStringOptions& tso) const
{
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    for(const auto& v: values)
    {
        if(first) { first = false; }
        else { ss <<  ", "; }

        const auto s = v->to_string(tso);
        if(s.size() != 1) { return std::nullopt; }

        ss << s[0];
    }
    ss << "]";
    return ss.str();
}


std::vector<std::string>
Array::to_string(const ToStringOptions& tsoa) const
{
    const auto tso = tsoa.with_quote_string(true);
    if(auto flat = to_flat_string_representation(tso); flat && flat->length() < tso.max_length)
    {
        return {*flat};
    }

    std::vector<std::string> r;
    r.emplace_back("[");
    bool first = true;
    for(const auto& v: values)
    {
        if(first) { first = false; }
        else { *r.rbegin() += ","; }

        const auto ss = v->to_string(tso);
        for(const auto& s: ss)
        {
            r.emplace_back("{}{}"_format(tso.indent, s));
        }
    }
    r.emplace_back("]");
    return r;
}



std::shared_ptr<Object>
Array::get_property_or_null(const std::string& name)
{
    if(auto method = integration->find_array_method_or_null(name); method != nullptr)
    {
        auto self = shared_from_this();
        assert(self != nullptr);
        return method->bind(self);
    }

    return nullptr;
}


bool
Array::set_property_or_false(const std::string&, std::shared_ptr<Object>)
{
    return false;
}



// ----------------------------------------------------------------------------


void
ObjectIntegrationImpl::register_functions()
{
    add_array_method("len", [](Callable* callable, ArgumentHelper& arguments) -> std::shared_ptr<Object>
    {
        // todo(Gustav): move to a more clean place?
        assert(callable->is_bound());
        BoundCallable* bound = static_cast<BoundCallable*>(callable);
        assert(bound->bound != nullptr);
        assert(bound->bound->get_type() == ObjectType::array);
        std::shared_ptr<Array> array = std::static_pointer_cast<Array>(bound->bound);
        assert(array != nullptr);

        arguments.complete();

        const std::size_t length = array->values.size();
        if(static_cast<std::size_t>(std::numeric_limits<Ti>::max()) < length)
        {
            throw NativeError { "array length too big for script numbers" };
        }

        return make_number_int( static_cast<Ti>(length) );
    });

    add_array_method("push", [](Callable* callable, ArgumentHelper& arguments) -> std::shared_ptr<Object>
    {
        // todo(Gustav): move to a more clean place?
        assert(callable->is_bound());
        BoundCallable* bound = static_cast<BoundCallable*>(callable);
        assert(bound->bound != nullptr);
        assert(bound->bound->get_type() == ObjectType::array);
        std::shared_ptr<Array> array = std::static_pointer_cast<Array>(bound->bound);
        assert(array != nullptr);

        auto to_add = arguments.require_object();
        arguments.complete();

        array->values.emplace_back(std::move(to_add));

        return make_nil();
    });

    add_array_method("remove_front", [](Callable* callable, ArgumentHelper& arguments) -> std::shared_ptr<Object>
    {
        // todo(Gustav): move to a more clean place?
        assert(callable->is_bound());
        BoundCallable* bound = static_cast<BoundCallable*>(callable);
        assert(bound->bound != nullptr);
        assert(bound->bound->get_type() == ObjectType::array);
        std::shared_ptr<Array> array = std::static_pointer_cast<Array>(bound->bound);
        assert(array != nullptr);

        arguments.complete();

        if(array->values.empty())
        {
            throw NativeError{{"Can't remove item from empty array"}};
        }
        array->values.erase(array->values.begin());

        return make_nil();
    });
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

std::vector<std::string>
NumberInt::to_string(const ToStringOptions&) const
{
    return {"{0}"_format(value)};
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

std::vector<std::string>
NumberFloat::to_string(const ToStringOptions&) const
{
    return {"{0}"_format(value)};
}



// ----------------------------------------------------------------------------


std::string
Object::to_flat_string(const ToStringOptions& tso) const
{
    std::ostringstream ss;
    const auto arr = to_string(tso);
    const auto many = arr.size() > 1;
    if(many) { ss << "["; }

    bool first = true;
    for(const auto& a: arr)
    {
        if(first) { first = false; }
        else {ss << ", ";}
        ss << a;
    }

    if(many) { ss << "]"; }
    return ss.str();
}


bool
Object::has_properties()
{
    return false;
}

std::shared_ptr<Object>
Object::get_property_or_null(const std::string&)
{
    return nullptr;
}

bool
Object::set_property_or_false(const std::string&, std::shared_ptr<Object>)
{
    return false;
}

bool
Object::has_index() const
{
    return false;
}


std::shared_ptr<Object>
Object::get_index_or_null(std::shared_ptr<Object>)
{
    return nullptr;
}


bool
Object::set_index_or_false(std::shared_ptr<Object>, std::shared_ptr<Object>)
{
    return false;
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

std::vector<std::string>
BoundCallable::to_string(const ToStringOptions&) const
{
    return {"<{} bound to {}>"_format(bound->to_flat_string(ToStringOptions::for_debug()), callable->to_flat_string(ToStringOptions::for_debug()))};
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


bool
Klass::has_properties()
{
    return true;
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

bool
Instance::has_properties()
{
    return true;
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

    if(parent == nullptr)
    {
        return nullptr;
    }

    return parent->get_property_or_null(name);
}

bool Instance::set_property_or_false(const std::string& name, std::shared_ptr<Object> value)
{
    auto was_set = set_field_or_false(name, value);
    if(was_set == false && parent != nullptr)
    {
        return parent->set_field_or_false(name, value);
    }
    return was_set;
}


// use this to call member functions on instance
std::shared_ptr<Callable>
Instance::get_bound_method_or_null(const std::string& name)
{
    auto prop = get_property_or_null(name);
    if(prop == nullptr) { return nullptr;}

    if(prop->is_callable() == false) { return nullptr; }
    return std::static_pointer_cast<Callable>(prop);
}


// ----------------------------------------------------------------------------


NativeKlass::NativeKlass(const std::string& n, std::size_t id, std::shared_ptr<Klass> sk)
        : Klass(n, sk)
        , native_id(id)
{
}

std::vector<std::string>
NativeKlass::to_string(const ToStringOptions&) const
{
    return {"<native class {}>"_format(klass_name)};
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

ObjectType
NativeInstance::get_type() const
{
    return ObjectType::native_instance;
}

std::vector<std::string>
NativeInstance::to_string(const ToStringOptions&) const
{
    return {"<native instance {}>"_format(klass->klass_name)};
}


std::shared_ptr<Object>
NativeInstance::get_field_or_null(const std::string& name)
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

bool
NativeInstance::set_field_or_false(const std::string& name, std::shared_ptr<Object> value)
{
    NativeKlass* nk = static_cast<NativeKlass*>(klass.get());
    if(auto found = nk->properties.find(name); found != nk->properties.end())
    {
        // todo(Gustav): what happens if the value is a not-supported type... error how?
        return found->second->set_value(this, value);
    }
    else
    {
        return false;
    }
}


// ----------------------------------------------------------------------------



bool Array::is_callable() const
{
    return false;
}

bool Array::has_properties()
{
    return true;
}


std::size_t Array::as_array_index(std::shared_ptr<Object> o)
{
    const std::optional<Ti> index = as_int(o);
    if(index.has_value() == false)
    {
        // todo(Gustav): allow multiple rows to include object to_string
        throw NativeError{ "array index needs to be a int, was {}"_format( objecttype_to_string(o->get_type()) ) };
    }

    if(*index < 0)
    {
        throw NativeError{ "array index needs to be positive, was {}"_format(*index) };
    }

    return static_cast<std::size_t>(*index);
}

bool Array::has_index() const
{
    return true;
}

std::shared_ptr<Object> Array::get_index_or_null(std::shared_ptr<Object> index_object)
{
    const auto index = as_array_index(index_object);
    if(index >= values.size())
    {
        throw NativeError{ "array index {} out of range, needs to be lower than {}"_format(index, values.size()) };
    }

    return values[index];
}

bool Array::set_index_or_false(std::shared_ptr<Object> index_object, std::shared_ptr<Object> new_value)
{
    const auto index = as_array_index(index_object);
    if(index >= values.size())
    {
        throw NativeError{ "array index {} is out of range, needs to be lower than {}"_format(index, values.size()) };
    }

    values[index] = new_value;
    return true;
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
ObjectIntegrationImpl::make_array(std::vector<std::shared_ptr<Object>>&& values)
{
    return std::make_shared<Array>(this, std::move(values));
}

std::unique_ptr<ObjectIntegration> make_object_integration()
{
    return std::make_unique<ObjectIntegrationImpl>();
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

std::shared_ptr<Array>
as_array(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::array) { return nullptr; }
    return std::static_pointer_cast<Array>(o);
}

std::shared_ptr<Instance>
as_instance(std::shared_ptr<Object> o)
{
    assert(o != nullptr);
    if(o->get_type() != ObjectType::instance) { return nullptr; }
    return std::static_pointer_cast<Instance>(o);
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

std::shared_ptr<Instance>
ArgumentHelper::require_instance()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_instance_from_arg(args, argument_index);
}

std::shared_ptr<Object>
ArgumentHelper::require_object()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_object_from_arg(args, argument_index);
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

std::shared_ptr<Array>
ArgumentHelper::require_array()
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_array_from_arg(args, argument_index);
}

std::shared_ptr<NativeInstance>
ArgumentHelper::impl_require_native_instance(std::size_t klass)
{
    const auto argument_index = next_argument++;
    if(args.arguments.size() <= argument_index) { return nullptr; }
    return get_native_instance_from_arg(args, argument_index, klass);
}

// ----------------------------------------------------------------------------

void raise_error(const std::string& message)
{
    throw NativeError { message };
}

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

    
    PropertyGet::PropertyGet
    (
        std::function<std::shared_ptr<Object>()>&& g
    )
        : getter(g)
    {
    }

    std::shared_ptr<Object> PropertyGet::get_value(NativeInstance*)
    {
        return getter();
    }

    bool PropertyGet::set_value(NativeInstance*, std::shared_ptr<Object>)
    {
        return false;
    }
}

// ----------------------------------------------------------------------------


struct NativeKlassImpl : NativeKlass
{
    std::function<std::shared_ptr<Instance> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)> constr;

    NativeKlassImpl
    (
        const std::string& name,
        std::size_t id,
        std::function<std::shared_ptr<Instance> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)>&& c    
    )
        : NativeKlass(name, id, nullptr)
        , constr(std::move(c))
    {
    }

    std::shared_ptr<Instance> constructor(const Arguments& arguments) override
    {
        auto obj = shared_from_this();
        assert(obj.get() == this);
        auto self = std::static_pointer_cast<NativeKlassImpl>(obj);
        ArgumentHelper ah{arguments};
        return constr(self, ah);
    }
};


struct NativePackage : Object, Scope
{
    std::string package_name;
    std::unordered_map<std::string, std::shared_ptr<Object>> members;
    std::unordered_map<std::string, ObjectGenerator> properties;

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

    std::vector<std::string>
    to_string(const ToStringOptions&) const override
    {
        return {"<native pkg {}>"_format(package_name)};
    }
    
    bool
    is_callable() const override
    {
        return false;
    }
    
    bool
    has_properties() override
    {
        return true;
    }

    std::shared_ptr<Object>
    get_property_or_null(const std::string& name) override
    {
        if(auto found_member = members.find(name); found_member != members.end())
        {
            return found_member->second;
        }
        else if(auto found_property = properties.find(name); found_property != properties.end())
        {
            return found_property->second();
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
        assert(members.find(name) == members.end() && "error: member already added (member)");
        assert(properties.find(name) == properties.end() && "error: member already added (property)");
        members.insert({name, value});
    }

    void
    add_property(const std::string& name, ObjectGenerator&& value) override
    {
        assert(members.find(name) == members.end() && "error: member already added (member)");
        assert(properties.find(name) == properties.end() && "error: member already added (property)");
        properties.insert({name, std::move(value)});
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
    std::function<std::shared_ptr<Instance> (std::shared_ptr<NativeKlass>, ArgumentHelper& ah)>&& c
)
{
    auto new_klass = std::make_shared<NativeKlassImpl>(name, id, std::move(c));
    set_property(name, new_klass);
    interpreter->register_native_klass(id, new_klass);
    return new_klass;
}

Scope&
Scope::add_native_getter(const std::string& name, ObjectGenerator&& getter)
{
    add_property(name, std::move(getter));
    return *this;
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

    void
    add_property(const std::string&, ObjectGenerator&&) override
    {
        assert(false && "global scope doesn't support properties");
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
