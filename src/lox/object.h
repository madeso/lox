#pragma once


namespace lox
{

struct Object
{
    Object() = default;
    virtual ~Object() = default;

    virtual std::string to_string() const = 0;
};


struct String : public Object
{
    std::string string;

    explicit String(const std::string_view& s);
    virtual ~String() = default;

    std::string to_string() const override;
};


struct Number : public Object
{
    float number;

    explicit Number(float f);
    virtual ~Number() = default;

    std::string to_string() const override;
};


}

