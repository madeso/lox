#pragma once

#include "lox/source.h"


namespace lox
{

struct ErrorHandler
{
    virtual ~ErrorHandler() = default;

    // todo(Gustav): add support for more than one line per message

    virtual void on_error(const Offset& where, const std::string& message) = 0;
    virtual void on_note(const Offset& where, const std::string& message) = 0;
};


}
