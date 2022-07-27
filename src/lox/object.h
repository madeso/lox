#pragma once


namespace lox
{


enum class ObjectType
{
    nil, string, boolean, number
};


constexpr std::string_view objecttype_to_string(ObjectType ot)
{
    switch (ot)
    {
    case ObjectType::nil:     return "nil";
    case ObjectType::string:  return "string";
    case ObjectType::boolean: return "boolean";
    case ObjectType::number:  return "number";
    default:                  return "???";
    }
}


struct Object
{
    Object() = default;
    virtual ~Object() = default;

    virtual std::shared_ptr<Object> clone() const = 0;
    virtual ObjectType get_type() const = 0;
    virtual std::string to_string() const = 0;
};

// ----------------------------------------------------------------------------

struct Nil : public Object
{
    Nil() = default;
    virtual ~Nil() = default;

    std::shared_ptr<Object> clone() const override;
    ObjectType get_type() const override;
    std::string to_string() const override;
};



struct String : public Object
{
    std::string value;

    explicit String(const std::string& s);
    explicit String(const std::string_view& s);
    virtual ~String() = default;

    std::shared_ptr<Object> clone() const override;
    ObjectType get_type() const override;
    std::string to_string() const override;
};


struct Bool : public Object
{
    bool value;

    explicit Bool(bool b);
    virtual ~Bool() = default;

    std::shared_ptr<Object> clone() const override;
    ObjectType get_type() const override;
    std::string to_string() const override;
};


struct Number : public Object
{
    float value;

    explicit Number(float f);
    virtual ~Number() = default;

    std::shared_ptr<Object> clone() const override;
    ObjectType get_type() const override;
    std::string to_string() const override;
};


}

