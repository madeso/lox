#include "lox/test.h"

#include "catchy/vectorequals.h"

#include "fmt/format.h"
using namespace fmt::literals;


AddStringErrors::AddStringErrors(std::vector<std::string>* o)
    : errors(o)
{
}

void
AddStringErrors::on_line(std::string_view line)
{
    errors->emplace_back(line);
}


AddErrorErrors::AddErrorErrors(std::vector<ReportedError>* e)
    : errors(e)
{
}

void
AddErrorErrors::on_error(const lox::Offset& o, const std::string& message)
{
    errors->emplace_back(ReportedError{ReportedError::Type::error, o.start, o.end, message});
}

void
AddErrorErrors::on_note(const lox::Offset& o, const std::string& message)
{
    errors->emplace_back(ReportedError{ReportedError::Type::note, o.start, o.end, message});
}

std::string_view
type_to_string(ReportedError::Type t)
{
    switch(t)
    {
    case ReportedError::Type::error: return "error";
    case ReportedError::Type::note: return "note";
    default:
        assert(false && "unhandled error log type");
        return "";
    }
}

std::string
ErrorToString(const ReportedError& e)
{
    return "({} {} {}: {})"_format(type_to_string(e.type), e.start, e.end, e.message);
}

catchy::FalseString
SingleErrorEq(const ReportedError& lhs, const ReportedError& rhs)
{
    std::string err = "";

    auto add = [&err](const std::string& s) {
        if(err.empty() == false) { err += ", ";}
        err += s;
    };

    if(lhs.type != rhs.type) { add("{} != {}"_format(type_to_string(lhs.type), type_to_string(rhs.type))); }

    if(lhs.start != rhs.start || lhs.end != rhs.end) { add("({} {}) != ({} {})"_format(lhs.start, lhs.end, rhs.start, rhs.end)); }

    if(lhs.message != rhs.message) { add("<{}> != <{}>"_format(lhs.message, rhs.message)); }

    if(err.empty()) { return catchy::FalseString::True(); }
    else { return catchy::FalseString::False(err); }
}


catchy::FalseString
ErrorEq(const std::vector<ReportedError>& lhs, const std::vector<ReportedError>& rhs)
{
    return catchy::VectorEquals(lhs, rhs, ErrorToString, SingleErrorEq);
}
