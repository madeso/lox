#include "lox/test.h"


AddStringErrors::AddStringErrors(std::vector<std::string>* o)
    : errors(o)
{
}

void
AddStringErrors::on_line(std::string_view line)
{
    errors->emplace_back(line);
}

