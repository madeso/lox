#pragma once

#include "lox/offset.h"


namespace lox
{

struct ErrorHandler
{
    virtual ~ErrorHandler() = default;

    virtual void on_error(const Offset& where, const std::string& message) = 0;
    virtual void on_note(const Offset& where, const std::string& message) = 0;
};


}
