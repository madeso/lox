#include "lox/stringmap.h"



namespace lox
{


StringMap::StringMap(std::string_view source)
{
    std::size_t line = 0;
    std::size_t start = 0;

    std::size_t index = 0;
    for(; index < source.size(); index += 1)
    {
        if(source[index] == '\n')
        {
            lines.emplace_back(LineData{line, Offset{start, index}});
            start = index + 1;
            line += 1;
        }
    }

    if( start + 1 != index)
    {
        lines.emplace_back(LineData{line, Offset{start, index}});
    }

    assert(lines.empty() == false);
}


LineData
StringMap::get_line_from_offset(std::size_t offset) const
{
    assert(lines.empty() == false);

    for(std::size_t index = 0; index < lines.size(); index += 1)
    {
        if(offset <= lines[index].offset.end)
        {
            return lines[index];
        }
    }

    return lines[lines.size() - 1];
}

}

