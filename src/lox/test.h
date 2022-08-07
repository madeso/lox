#pragma once

#include "lox/printhandler.h"


struct ParseOutput
{
    std::string out;
    std::vector<std::string> err;
};

struct AddStringErrors : lox::PrintHandler
{
    std::vector<std::string>* errors;

    explicit AddStringErrors(std::vector<std::string>* o);

    void
    on_line(std::string_view line) override;
};

