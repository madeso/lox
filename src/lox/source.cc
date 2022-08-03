#include "lox/source.h"

namespace lox
{


Offset::Offset(std::shared_ptr<Source> src, std::size_t s, std::size_t e)
    : source(src)
    , start(s)
    , end(e)
{
}


Offset::Offset(std::shared_ptr<Source> src, std::size_t where)
    : source(src)
    , start(where)
    , end(where)
{
}


Source::Source(std::string str)
    : source(std::move(str))
{
}


StringMap&
Source::get_or_create_map()
{
    if(map)
    {
        return *map;
    }
    else
    {
        map = StringMap{source};
        return *map;
    }
}


StringMap::StringMap(std::string_view source)
{
    std::size_t line = 0;
    std::size_t start = 0;

    std::size_t index = 0;
    for(; index < source.size(); index += 1)
    {
        if(source[index] == '\n')
        {
            lines.emplace_back(LineData{line, OffsetNoSource{start, index}});
            start = index + 1;
            line += 1;
        }
    }

    if( start + 1 != index)
    {
        lines.emplace_back(LineData{line, OffsetNoSource{start, index}});
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
