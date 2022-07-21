#pragma once

struct Offset
{
    std::size_t start;
    std::size_t end;

    constexpr Offset(std::size_t s, std::size_t e) : start(s), end(e) {}
    constexpr Offset(std::size_t where) : start(where), end(where) {}
};

