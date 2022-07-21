#pragma once

#include "lox/offset.h"


namespace lox
{


struct LineData
{
    std::size_t line; // 0-based
    Offset offset;
};

struct StringMap
{
    std::vector<LineData> lines;

    explicit StringMap(std::string_view source);

    LineData
    get_line_from_offset(std::size_t offset) const;
};

}
