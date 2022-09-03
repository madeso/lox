#pragma once

#include "catchy/falsestring.h"
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


struct ReportedError
{
    enum class Type
    {
        error, note
    };

    Type type;
    std::size_t start;
    std::size_t end;
    std::vector<std::string> messages;
};

struct AddErrorErrors : lox::ErrorHandler
{
    std::vector<ReportedError>* errors;

    explicit AddErrorErrors(std::vector<ReportedError>* e);

    void
    on_errors(const lox::Offset& offset, const std::vector<std::string>& message) override;

    void
    on_notes(const lox::Offset& offset, const std::vector<std::string>& message) override;
};

catchy::FalseString
ErrorEq(const std::vector<ReportedError>& lhs, const std::vector<ReportedError>& rhs);
