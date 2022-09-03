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
    on_errors(const Offset& offset, const std::vector<std::string>& message) override;

    void
    on_notes(const Offset& offset, const std::vector<std::string>& message) override;
};



}
