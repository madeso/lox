#pragma once

#include <string>


namespace lox
{

struct ErrorHandler
{
    virtual ~ErrorHandler() = default;

    void error(int line, const std::string& message);

    virtual void report(int line, const std::string& where, const std::string& message) = 0;
};

}
