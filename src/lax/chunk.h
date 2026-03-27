#pragma once

namespace lax
{

enum class OpCode
{
    Return
};

struct Chunk
{
    std::vector<std::uint8_t> code;

    void write(std::uint8_t byte);
};

}
