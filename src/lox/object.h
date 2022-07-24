#pragma once


namespace lox
{

struct Object
{
    Object() = default;
    virtual ~Object() = default;

    virtual std::string to_string() const = 0;
};

// ----------------------------------------------------------------------------

struct Nil : public Object
{
    Nil() = default;
    virtual ~Nil() = default;

    std::string to_string() const override;
};



struct String : public Object
{
    std::string value;

    explicit String(const std::string_view& s);
    virtual ~String() = default;

    std::string to_string() const override;
};


struct Bool : public Object
{
    bool value;

    explicit Bool(bool b);
    virtual ~Bool() = default;

    std::string to_string() const override;
};


struct Number : public Object
{
    float value;

    explicit Number(float f);
    virtual ~Number() = default;

    std::string to_string() const override;
};


}

