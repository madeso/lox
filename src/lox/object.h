#pragma once

#include <string>

namespace lox
{
    
struct Object
{
    Object() = default;
    virtual ~Object() = default;

    virtual std::string toString() const = 0;
};

struct String : public Object
{
    explicit String(const std::string& s);
    virtual ~String() = default;

    std::string string;
    std::string toString() const override;
};

struct Number : public Object
{
    explicit Number(float f);
    virtual ~Number() = default;

    float number;
    std::string toString() const override;
};


}

