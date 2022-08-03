#pragma once

#include "lox/errorhandler.h"



namespace lox
{


struct PrintHandler : ErrorHandler
{
    PrintHandler() = default;
    virtual ~PrintHandler() = default;

    virtual void
    on_line(std::string_view line) = 0;

    void
    on_error(const Offset& offset, const std::string& message) override;

    void
    on_note(const Offset& offset, const std::string& message) override;
};



}
