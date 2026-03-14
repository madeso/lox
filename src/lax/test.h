#pragma once

#include "catchy/falsestring.h"
#include "lax/printhandler.h"


struct ParseOutput
{
    std::string out;
    std::vector<std::string> err;
};

struct AddStringErrors : lax::PrintHandler
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

struct AddErrorErrors : lax::ErrorHandler
{
    std::vector<ReportedError>* errors;

    explicit AddErrorErrors(std::vector<ReportedError>* e);

    void
    on_errors(const lax::Offset& offset, const std::vector<std::string>& message) override;

    void
    on_notes(const lax::Offset& offset, const std::vector<std::string>& message) override;
};

catchy::FalseString
ErrorEq(const std::vector<ReportedError>& lhs, const std::vector<ReportedError>& rhs);
