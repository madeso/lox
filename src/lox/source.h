#pragma once


namespace lox
{

struct OffsetNoSource
{
    std::size_t start;
    std::size_t end;

    constexpr OffsetNoSource(std::size_t s, std::size_t e) : start(s) , end(e) {}
    constexpr OffsetNoSource(std::size_t where) : start(where) , end(where) {}
};


struct LineData
{
    std::size_t line; // 0-based
    OffsetNoSource offset;
};


struct StringMap
{
    std::vector<LineData> lines;

    explicit StringMap(std::string_view source);

    LineData
    get_line_from_offset(std::size_t offset) const;
};


struct Source
{
    explicit Source(std::string str);

    std::string source;
    std::optional<StringMap> map;

    StringMap& get_or_create_map();
};


struct Offset
{
    std::shared_ptr<Source> source;
    std::size_t start;
    std::size_t end;

    Offset(std::shared_ptr<Source> src, std::size_t s, std::size_t e);
    Offset(std::shared_ptr<Source> src, std::size_t where);
};



}
