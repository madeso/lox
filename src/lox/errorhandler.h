#pragma once

#include "lox/source.h"


namespace lox
{

struct ErrorHandler
{
    virtual ~ErrorHandler() = default;

    // todo(Gustav): add support for more than one line per message

    virtual void on_errors(const Offset& where, const std::vector<std::string>& messages) = 0;
    virtual void on_notes(const Offset& where, const std::vector<std::string>& messages) = 0;

    void on_error(const Offset& where, const std::string& message);
    void on_note(const Offset& where, const std::string& message);
};


}
