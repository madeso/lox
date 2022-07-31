#pragma once

#include "lox/errorhandler.h"



namespace lox
{


struct PrintHandler : ErrorHandler
{
    std::string_view current_source;
    bool error_detected = false;


    explicit PrintHandler(std::string_view source);
    virtual ~PrintHandler() = default;

    virtual void
    on_line(std::string_view line) = 0;

    void
    on_error(const Offset& offset, const std::string& message) override;

    void
    on_note(const Offset& offset, const std::string& message) override;
};



}
